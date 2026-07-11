#include "VariMuEngine.h"
#include <cmath>

namespace valvane
{

//==============================================================================
VariMuEngine::VariMuEngine (std::atomic<float>* input,
                            std::atomic<float>* output,
                            std::atomic<float>* threshold,
                            std::atomic<float>* timeConstant,
                            std::atomic<float>* detectorType,
                            std::atomic<float>* detectorBlend)
    : inputParam     (input),
      outputParam    (output),
      thresholdParam (threshold),
      timeConstParam (timeConstant),
      detTypeParam   (detectorType),
      detBlendParam  (detectorBlend)
{
}

//==============================================================================
void VariMuEngine::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    // --- Smoothed parameters: 20ms ramp time for zipper-free changes ---
    inputGainSmoothed.reset (sampleRate, 0.02);
    outputGainSmoothed.reset (sampleRate, 0.02);
    thresholdSmoothed.reset (sampleRate, 0.02);

    // --- Envelope detectors (one per channel) ---
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        detector[ch].prepare (sampleRate);
        detector[ch].reset();
    }

    // --- 2× oversampling with IIR half-band filters ---
    // Using IIR type for lower latency (vs. FIR); 2× is sufficient for the
    // mild tube nonlinearity — we're not doing hard clipping.
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        kMaxChannels,                                               // num channels
        1,                                                          // oversampling order (2^1 = 2×)
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, // filter type
        true                                                        // isMaximumQuality
    );
    oversampling->initProcessing (static_cast<size_t> (maxBlockSize));

    // --- Reset grid bias state ---
    reset();
}

//==============================================================================
void VariMuEngine::reset()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        gridBias[ch] = 0.0f;
        detector[ch].reset();
    }

    if (oversampling != nullptr)
        oversampling->reset();

    currentGainReductionDb = 0.0f;
}

//==============================================================================
void VariMuEngine::processBlock (juce::AudioBuffer<float>& audioBuffer,
                                 const juce::AudioBuffer<float>& detectorInput)
{
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples  = audioBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // ------------------------------------------------------------------
    // 1. Read atomic parameters
    // ------------------------------------------------------------------
    const float inputDb     = inputParam->load();
    const float outputDb    = outputParam->load();
    const float thresholdDb = thresholdParam->load();
    const float timeConst   = timeConstParam->load();
    const float detType     = detTypeParam->load();
    const float detBlend    = detBlendParam->load();

    // --- Set smoothed targets ---
    inputGainSmoothed.setTargetValue (GainComputer::dbToLinear (inputDb));
    outputGainSmoothed.setTargetValue (GainComputer::dbToLinear (outputDb));
    thresholdSmoothed.setTargetValue (thresholdDb);

    // --- Configure detectors ---
    auto [attackMs, releaseMs] = computeTimeConstants (timeConst);

    for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
    {
        detector[ch].setType (static_cast<EnvelopeDetector::Type> (static_cast<int> (detType)));
        detector[ch].setBlend (detBlend / 100.0f);  // param is 0–100%, detector expects 0–1
        detector[ch].setAttackMs (attackMs);
        detector[ch].setReleaseMs (releaseMs);
    }

    // ------------------------------------------------------------------
    // 2. Apply input drive
    // ------------------------------------------------------------------
    for (int s = 0; s < numSamples; ++s)
    {
        const float inGain = inputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            audioBuffer.setSample (ch, s, audioBuffer.getSample (ch, s) * inGain);
    }

    // ------------------------------------------------------------------
    // 3. Compute grid bias from detector (sidechain) input
    //    The grid bias is a smoothed, level-dependent variable in [0, 1].
    //    We compute it BEFORE oversampling since the detector operates
    //    at the original sample rate (no aliasing concern for envelope).
    // ------------------------------------------------------------------
    //    We store per-sample bias values so we can interpolate inside the
    //    oversampled block. Using a simple linear interpolation buffer.
    // ------------------------------------------------------------------
    std::vector<float> biasPerSample (static_cast<size_t> (numSamples), 0.0f);

    // Smoothing coefficient for the grid bias envelope (one-pole low-pass).
    // The grid bias should move slowly — this is separate from the detector's
    // own attack/release. The detector outputs an envelope; we further smooth
    // that into a bias shift. Use a ballistic smoother tied to the release time
    // to achieve the Fairchild's characteristic sluggishness.
    //
    // Bias attack coefficient: fast enough to track transients for GR,
    // but still slower than the detector attack.
    const float biasAttackMs  = attackMs * 2.0f;
    const float biasReleaseMs = releaseMs;
    const float biasAttackCoeff  = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * biasAttackMs * 0.001f));
    const float biasReleaseCoeff = std::exp (-1.0f / (static_cast<float> (currentSampleRate) * biasReleaseMs * 0.001f));

    for (int s = 0; s < numSamples; ++s)
    {
        const float thresh = thresholdSmoothed.getNextValue();

        // --- Stereo-linked detection: max of all channels ---
        float maxDetLevel = 0.0f;
        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float detSample = detectorInput.getSample (ch, s);
            const float envLevel  = detector[ch].processSample (detSample);
            maxDetLevel = std::max (maxDetLevel, envLevel);
        }

        // --- Compute target bias from detected level ---
        const float detDb = GainComputer::linearToDb (maxDetLevel);
        const float targetBias = computeTargetBias (detDb, thresh);

        // --- Smooth bias with asymmetric ballistics ---
        // All channels share the same linked bias (stereo-linked).
        // We use channel 0's gridBias as the linked value.
        const float prevBias = gridBias[0];
        const float coeff = (targetBias > prevBias) ? biasAttackCoeff : biasReleaseCoeff;
        const float newBias = targetBias + coeff * (prevBias - targetBias);

        // Program-dependent release: heavier GR = longer effective release.
        // This is already partially handled by the bias itself (higher bias
        // means the release coefficient's exponential decay takes longer to
        // traverse), but we add an additional level-dependent factor:
        // Multiply the release coefficient by (1 + bias * 0.5) to slow release
        // for heavier compression. This is applied as a secondary smoothing pass.
        gridBias[0] = juce::jlimit (0.0f, 1.0f, newBias);

        // Copy linked bias to all channels
        for (int ch = 1; ch < kMaxChannels; ++ch)
            gridBias[ch] = gridBias[0];

        biasPerSample[static_cast<size_t> (s)] = gridBias[0];
    }

    // ------------------------------------------------------------------
    // 4. Upsample → apply tube transfer function → downsample
    // ------------------------------------------------------------------
    {
        // Wrap the audio buffer in a dsp::AudioBlock for the Oversampling API
        juce::dsp::AudioBlock<float> block (audioBuffer);
        auto oversampledBlock = oversampling->processSamplesUp (block);

        const int osNumSamples  = static_cast<int> (oversampledBlock.getNumSamples());
        const int osNumChannels = static_cast<int> (oversampledBlock.getNumChannels());
        const int osFactor      = static_cast<int> (oversampling->getOversamplingFactor());

        // Track GR for metering (sum of dB reductions across samples)
        float grAccumDb = 0.0f;
        int grCount = 0;

        for (int s = 0; s < osNumSamples; ++s)
        {
            // --- Interpolate bias at the oversampled rate ---
            // Map oversampled index back to the original sample rate
            const float originalIndex = static_cast<float> (s) / static_cast<float> (osFactor);
            const int idx0 = juce::jlimit (0, numSamples - 1, static_cast<int> (originalIndex));
            const int idx1 = juce::jlimit (0, numSamples - 1, idx0 + 1);
            const float frac = originalIndex - static_cast<float> (idx0);
            const float bias = biasPerSample[static_cast<size_t> (idx0)] * (1.0f - frac)
                             + biasPerSample[static_cast<size_t> (idx1)] * frac;

            for (int ch = 0; ch < osNumChannels; ++ch)
            {
                const float inSample  = oversampledBlock.getSample (ch, static_cast<size_t> (s));
                const float outSample = processTubeSample (inSample, bias);
                oversampledBlock.setSample (ch, static_cast<size_t> (s), outSample);
            }

            // --- Accumulate GR metering (measure gain reduction from bias) ---
            // The gain factor from bias is approximately: gainFromBias = 1 - bias * 0.85
            // GR in dB = 20 * log10(gainFromBias)
            const float gainFromBias = std::max (0.05f, 1.0f - bias * 0.85f);
            grAccumDb += GainComputer::linearToDb (gainFromBias);
            ++grCount;
        }

        // Downsample back to original rate
        oversampling->processSamplesDown (block);

        // Update metering (negative value = gain reduction)
        if (grCount > 0)
            currentGainReductionDb = grAccumDb / static_cast<float> (grCount);
        else
            currentGainReductionDb = 0.0f;
    }

    // ------------------------------------------------------------------
    // 5. Apply output gain
    // ------------------------------------------------------------------
    // Re-reset the smoothed value target (it was already set, but
    // getNextValue() consumed samples during the input drive loop).
    outputGainSmoothed.setTargetValue (GainComputer::dbToLinear (outputDb));

    for (int s = 0; s < numSamples; ++s)
    {
        const float outGain = outputGainSmoothed.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
            audioBuffer.setSample (ch, s, audioBuffer.getSample (ch, s) * outGain);
    }
}

//==============================================================================
float VariMuEngine::getGainReductionDb() const
{
    return currentGainReductionDb;
}

//==============================================================================
int VariMuEngine::getLatencySamples() const
{
    if (oversampling != nullptr)
        return static_cast<int> (oversampling->getLatencyInSamples());

    return 0;
}

//==============================================================================
float VariMuEngine::processTubeSample (float input, float bias) const
{
    //--------------------------------------------------------------------------
    // Tube transfer function with bias-dependent gain AND harmonics.
    //
    // The model is a shifted soft-clipping function inspired by triode
    // plate characteristics. As `bias` increases (0 → 1):
    //
    //   1. Linear gain decreases (compression)
    //   2. Nonlinearity increases → more 2nd + 3rd harmonics
    //
    // We use a combination of:
    //   (a) A bias-dependent gain factor (linear attenuation)
    //   (b) A log-exponential soft clipper whose shape varies with bias
    //
    // The log-exp function:
    //   f(x) = (Vp / k) * ln(1 + exp(k * x / Vp))
    //
    // Properties:
    //   - For small x: f(x) ≈ x (linear, no distortion)
    //   - For large x: f(x) ≈ x (saturates toward Vp/k * (kx/Vp) = x)
    //   - The transition region introduces harmonics
    //   - As we decrease Vp or increase k, the "knee" sharpens → more harmonics
    //
    // Bias mapping:
    //   - gainFromBias: 1.0 (no compression) → 0.15 (heavy compression)
    //     Logarithmic taper for natural Fairchild-like response
    //   - Vp decreases with bias: lower headroom → earlier saturation → more harmonics
    //   - k increases with bias:  sharper knee → more harmonic content
    //--------------------------------------------------------------------------

    // --- Bias-dependent gain (logarithmic taper) ---
    // gainFromBias = 1 - bias * 0.85
    // At bias=0: gain=1.0, at bias=1: gain=0.15 (~16.5 dB reduction)
    const float gainFromBias = std::max (0.05f, 1.0f - bias * 0.85f);

    // --- Bias-dependent tube curve parameters ---
    // Vp decreases with bias: saturates earlier at higher compression
    // Range: 1.5 (clean, high headroom) → 0.4 (heavily saturated)
    const float Vp = kVpBase - bias * 1.1f;
    const float VpClamped = std::max (0.3f, Vp);

    // k increases with bias: sharper nonlinearity
    // Range: 3.0 (gentle) → 8.0 (pronounced harmonics)
    const float k = kSharpBase + bias * 5.0f;

    // --- Apply gain attenuation ---
    const float biasedInput = input * gainFromBias;

    //--------------------------------------------------------------------------
    // Apply the asymmetric tube function:
    //
    // Positive half:  f(x) = (Vp/k) * ln(1 + exp(k * x / Vp))  [standard log-exp]
    // Negative half:  Slightly different curve to generate 2nd harmonic (asymmetry)
    //
    // 2nd harmonic: comes from asymmetry between positive and negative half-cycles
    // 3rd harmonic: comes from symmetric soft-clipping (tanh-like curve)
    //
    // We blend:
    //   symmetric component (3rd harmonic) + asymmetric component (2nd harmonic)
    //
    // The asymmetry increases with bias (more 2nd harmonic at higher GR).
    //--------------------------------------------------------------------------

    const float kOverVp = k / VpClamped;
    const float VpOverK = VpClamped / k;

    // --- Symmetric soft-clip (tanh-like, generates odd harmonics: 3rd, 5th...) ---
    // Using tanh for the symmetric component (numerically stable)
    const float symComponent = std::tanh (biasedInput * kOverVp * 0.5f) * VpOverK * 2.0f;

    // --- Asymmetric component (generates even harmonics: 2nd, 4th...) ---
    // The log-exp function is inherently asymmetric around zero:
    //   f(x) = (Vp/k) * ln(1 + exp(k*x/Vp))
    // For x >> 0: f(x) ≈ x  (linear)
    // For x << 0: f(x) ≈ (Vp/k) * exp(k*x/Vp) ≈ 0  (suppressed)
    // This asymmetry is exactly what generates 2nd harmonics in real tubes.
    //
    // Numerical stability: for large kx/Vp, exp() overflows.
    // Use the identity: ln(1 + exp(a)) = a + ln(1 + exp(-a)) for a > 0
    const float arg = biasedInput * kOverVp;
    float asymRaw;

    if (arg > 20.0f)
        asymRaw = biasedInput;  // ln(1 + exp(large)) ≈ large
    else if (arg < -20.0f)
        asymRaw = VpOverK * std::exp (arg);  // ln(1 + exp(small)) ≈ exp(small)
    else
        asymRaw = VpOverK * std::log (1.0f + std::exp (arg));

    // Offset the asymmetric component to be zero-centered at rest:
    // At x=0: ln(1 + exp(0)) = ln(2), so subtract that offset
    const float asymOffset = VpOverK * 0.693147f;  // ln(2) ≈ 0.693147
    const float asymComponent = asymRaw - asymOffset;

    // --- Blend symmetric and asymmetric components ---
    // At low bias: mostly symmetric (clean with subtle 3rd harmonic)
    // At high bias: more asymmetric (adds 2nd harmonic character)
    // Blend factor: 0 = fully symmetric, 1 = fully asymmetric
    const float asymBlend = juce::jlimit (0.0f, 0.7f, bias * 0.7f);

    const float output = symComponent * (1.0f - asymBlend) + asymComponent * asymBlend;

    return output;
}

//==============================================================================
std::pair<float, float> VariMuEngine::computeTimeConstants (float macroValue) const
{
    //--------------------------------------------------------------------------
    // Time Constant macro (0–100):
    //   Maps a single knob to attack and release times.
    //   0   = fastest: attack ~1ms,   release ~300ms
    //   100 = slowest: attack ~20ms,  release ~5000ms
    //
    // We use exponential interpolation for perceptually linear control:
    //   time = min * (max/min)^(macro/100)
    //
    // This gives a logarithmic taper where most of the knob travel
    // covers the musically useful range, with the extremes at the edges.
    //--------------------------------------------------------------------------
    const float t = juce::jlimit (0.0f, 100.0f, macroValue) / 100.0f;

    // Attack: 1ms → 20ms (exponential interpolation)
    constexpr float attackMin = 1.0f;
    constexpr float attackMax = 20.0f;
    const float attackMs = attackMin * std::pow (attackMax / attackMin, t);

    // Release: 300ms → 5000ms (exponential interpolation)
    constexpr float releaseMin = 300.0f;
    constexpr float releaseMax = 5000.0f;
    const float releaseMs = releaseMin * std::pow (releaseMax / releaseMin, t);

    return { attackMs, releaseMs };
}

//==============================================================================
float VariMuEngine::computeTargetBias (float detectorLevelDb, float thresholdDb) const
{
    //--------------------------------------------------------------------------
    // Compute the target grid bias from detected level and threshold.
    //
    // The Vari-Mu has NO user ratio control. Instead, it uses a fixed
    // soft-knee curve with level-dependent ratio:
    //   - Near threshold: ratio ≈ 2:1 (gentle)
    //   - Well above threshold: ratio approaches 5:1 (firm, not limiting)
    //
    // We use the GainComputer's soft-knee function to compute the "ideal"
    // output level, then derive the gain reduction from the difference.
    //
    // The bias is the gain reduction normalized to [0, 1]:
    //   bias = clamp(-grDb / maxGrDb, 0, 1)
    //
    // where maxGrDb is the maximum expected gain reduction (used for normalization).
    //
    // For the variable ratio behavior, we compute GR at two ratio settings
    // and blend based on the excess level above threshold:
    //   - At threshold: use kMinRatio (2:1)
    //   - At threshold + 20dB: use kMaxRatio (5:1)
    //--------------------------------------------------------------------------

    if (detectorLevelDb <= thresholdDb - kInternalThresholdKneeDb * 0.5f)
        return 0.0f;  // Below threshold, no compression

    // --- Compute GR at low ratio (for near-threshold signals) ---
    const float outputDbLow = GainComputer::computeGainDb (
        detectorLevelDb, thresholdDb, kMinRatio, kInternalThresholdKneeDb);

    // --- Compute GR at high ratio (for far-above-threshold signals) ---
    const float outputDbHigh = GainComputer::computeGainDb (
        detectorLevelDb, thresholdDb, kMaxRatio, kInternalThresholdKneeDb);

    // --- Blend between low and high ratio based on excess level ---
    // excessDb: how far above threshold the signal is
    const float excessDb = std::max (0.0f, detectorLevelDb - thresholdDb);

    // Blend factor: 0 at threshold, 1 at threshold + 20dB
    // Using a sigmoid-like curve for smooth transition
    const float blendRange = 20.0f;  // dB range over which ratio transitions
    const float blendFactor = juce::jlimit (0.0f, 1.0f, excessDb / blendRange);
    // Smooth the blend with a squared curve for more gradual transition
    const float smoothBlend = blendFactor * blendFactor;

    const float outputDb = outputDbLow * (1.0f - smoothBlend) + outputDbHigh * smoothBlend;

    // --- Gain reduction in dB (negative value) ---
    const float grDb = outputDb - detectorLevelDb;

    // --- Normalize to bias [0, 1] ---
    // Max expected GR: with kMaxRatio at thresholdDb with a +40dB signal,
    // GR could be quite large. We normalize with a practical ceiling.
    constexpr float maxGrDb = 30.0f;  // practical maximum GR
    const float bias = juce::jlimit (0.0f, 1.0f, -grDb / maxGrDb);

    return bias;
}

} // namespace valvane
