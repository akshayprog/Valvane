#include "VCAEngine.h"
#include <cmath>

namespace valvane
{

//==============================================================================
VCAEngine::VCAEngine (std::atomic<float>* threshold,
                      std::atomic<float>* ratio,
                      std::atomic<float>* attack,
                      std::atomic<float>* release,
                      std::atomic<float>* autoRelease,
                      std::atomic<float>* knee,
                      std::atomic<float>* makeup,
                      std::atomic<float>* detectorType,
                      std::atomic<float>* detectorBlend)
    : thresholdParam   (threshold),
      ratioParam       (ratio),
      attackParam      (attack),
      releaseParam     (release),
      autoReleaseParam (autoRelease),
      kneeParam        (knee),
      makeupParam      (makeup),
      detTypeParam     (detectorType),
      detBlendParam    (detectorBlend)
{
}

//==============================================================================
void VCAEngine::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    // --- Smoothed parameters: 20ms ramp for zipper-free changes ---
    thresholdSmoothed.reset (sampleRate, 0.02);
    ratioSmoothed.reset (sampleRate, 0.02);
    kneeSmoothed.reset (sampleRate, 0.02);
    makeupSmoothed.reset (sampleRate, 0.02);

    // --- Envelope detectors ---
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        detector[ch].prepare (sampleRate);
        detector[ch].reset();
    }

    // --- Saturation (very light — VCA is the clean option) ---
    saturation.prepare (sampleRate, maxBlockSize, kMaxChannels);
    saturation.setDrive (0.05f);      // minimal drive
    saturation.setAsymmetry (0.0f);   // no asymmetry
    saturation.setEnabled (false);    // disabled by default — clean VCA

    // --- Reset state ---
    reset();
}

//==============================================================================
void VCAEngine::reset()
{
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        detector[ch].reset();
        gainReductionEnvDb[ch] = 0.0f;
        autoReleaseEnv[ch]     = 0.0f;
    }

    saturation.reset();
    currentGainReductionDb = 0.0f;
}

//==============================================================================
void VCAEngine::processBlock (juce::AudioBuffer<float>& audioBuffer,
                              const juce::AudioBuffer<float>& detectorInput)
{
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples  = audioBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // ------------------------------------------------------------------
    // 1. Read atomic parameters
    // ------------------------------------------------------------------
    const float threshDb     = thresholdParam->load();
    const float ratio        = ratioParam->load();
    const float attackMs     = attackParam->load();
    const float releaseMs    = releaseParam->load();
    const bool  autoRelOn    = autoReleaseParam->load() >= 0.5f;
    const float kneeDb       = kneeParam->load();
    const float makeupDb     = makeupParam->load();
    const float detType      = detTypeParam->load();
    const float detBlend     = detBlendParam->load();

    // --- Set smoothed targets ---
    thresholdSmoothed.setTargetValue (threshDb);
    ratioSmoothed.setTargetValue (ratio);
    kneeSmoothed.setTargetValue (kneeDb);
    makeupSmoothed.setTargetValue (GainComputer::dbToLinear (makeupDb));

    // --- Configure detectors ---
    for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
    {
        detector[ch].setType (static_cast<EnvelopeDetector::Type> (static_cast<int> (detType)));
        detector[ch].setBlend (detBlend / 100.0f);  // param is 0–100%, detector expects 0–1
        detector[ch].setAttackMs (attackMs);
        detector[ch].setReleaseMs (releaseMs);
    }

    // ------------------------------------------------------------------
    // 2. Compute attack/release coefficients for the gain smoothing filter
    //    (separate from the detector's own attack/release).
    //
    //    The detector outputs an envelope. We then compute the desired GR
    //    from the gain computer and smooth THAT with separate ballistics.
    //    This two-stage approach (detector → gain computer → gain smoother)
    //    follows the Giannoulis/Reiss/Rice (2012) architecture.
    //
    //    Gain smoother coefficients:
    //      coeff = exp(-1 / (sampleRate * timeMs * 0.001))
    //    This gives a one-pole IIR filter with the specified time constant.
    // ------------------------------------------------------------------
    const float sr = static_cast<float> (currentSampleRate);
    const float gainAttackCoeff = std::exp (-1.0f / (sr * attackMs * 0.001f));

    // Release coefficient depends on auto-release mode (computed per-sample below)
    const float gainReleaseCoeff = std::exp (-1.0f / (sr * releaseMs * 0.001f));

    // Auto-release coefficients (pre-computed)
    const float fastReleaseCoeff = std::exp (-1.0f / (sr * kFastReleaseMs * 0.001f));
    const float slowReleaseCoeff = std::exp (-1.0f / (sr * kSlowReleaseMs * 0.001f));
    const float autoFollowerCoeff = std::exp (-1.0f / (sr * kAutoReleaseFollowerMs * 0.001f));

    // ------------------------------------------------------------------
    // 3. Per-sample processing loop
    // ------------------------------------------------------------------
    float grAccumDb = 0.0f;  // for metering

    for (int s = 0; s < numSamples; ++s)
    {
        // --- Read smoothed parameter values ---
        const float thresh = thresholdSmoothed.getNextValue();
        const float rat    = ratioSmoothed.getNextValue();
        const float kn     = kneeSmoothed.getNextValue();
        const float mkGain = makeupSmoothed.getNextValue();

        // ---------------------------------------------------------------
        // 3a. Detection: envelope follow the sidechain (detector input)
        //     Stereo-linked: max envelope across all channels
        // ---------------------------------------------------------------
        float maxEnvLevel = 0.0f;
        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float detSample = detectorInput.getSample (ch, s);
            const float envLevel  = detector[ch].processSample (detSample);
            maxEnvLevel = std::max (maxEnvLevel, envLevel);
        }

        // ---------------------------------------------------------------
        // 3b. Gain computation (Giannoulis/Reiss/Rice soft-knee)
        //
        //     Convert envelope to dB, compute the ideal output level from
        //     the transfer curve, derive the gain change in dB.
        //
        //     GR_dB = computeGainDb(inputDb, thresh, ratio, knee) - inputDb
        //     This is negative (gain reduction) when input > threshold.
        // ---------------------------------------------------------------
        const float inputDb = GainComputer::linearToDb (maxEnvLevel);
        const float outputDb = GainComputer::computeGainDb (inputDb, thresh, rat, kn);
        const float targetGrDb = outputDb - inputDb;  // negative = gain reduction

        // ---------------------------------------------------------------
        // 3c. Gain smoothing (ballistic filter on the GR signal)
        //
        //     This is the second stage of smoothing — the first is the
        //     envelope detector. Here we smooth the computed GR to avoid
        //     abrupt gain changes, using attack/release ballistics.
        //
        //     Attack: when GR is increasing (targetGrDb < current)
        //     Release: when GR is decreasing (targetGrDb > current)
        //
        //     In auto-release mode, the release coefficient is a blend
        //     of fast and slow release, driven by a GR envelope follower.
        // ---------------------------------------------------------------
        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
        {
            const float prevGrDb = gainReductionEnvDb[ch];

            if (targetGrDb < prevGrDb)
            {
                // Attack (GR increasing — becoming more negative)
                gainReductionEnvDb[ch] = targetGrDb + gainAttackCoeff * (prevGrDb - targetGrDb);
            }
            else
            {
                // Release (GR decreasing — moving back toward 0)
                if (autoRelOn)
                {
                    // --- Auto-release: dual time-constant ---
                    // Track the GR signal with a slow envelope follower.
                    // High sustained GR → autoReleaseEnv is large → more slow release.
                    // Brief transients → autoReleaseEnv stays low → fast release.
                    const float absGr = std::abs (gainReductionEnvDb[ch]);
                    autoReleaseEnv[ch] = autoFollowerCoeff * autoReleaseEnv[ch]
                                       + (1.0f - autoFollowerCoeff) * absGr;

                    // Blend factor: normalize the GR envelope to [0, 1]
                    // At 0 dB GR → blend = 0 (all fast release)
                    // At 20+ dB GR → blend = 1 (all slow release)
                    const float blendFactor = juce::jlimit (0.0f, 1.0f, autoReleaseEnv[ch] / 20.0f);

                    // Interpolate between fast and slow release coefficients
                    const float effectiveReleaseCoeff = fastReleaseCoeff * (1.0f - blendFactor)
                                                      + slowReleaseCoeff * blendFactor;

                    gainReductionEnvDb[ch] = targetGrDb + effectiveReleaseCoeff * (prevGrDb - targetGrDb);
                }
                else
                {
                    // Standard release
                    gainReductionEnvDb[ch] = targetGrDb + gainReleaseCoeff * (prevGrDb - targetGrDb);
                }
            }
        }

        // ---------------------------------------------------------------
        // 3d. Apply gain reduction + makeup to audio
        //
        //     Stereo-linked: use the maximum GR across channels (most
        //     conservative, preserves stereo image).
        // ---------------------------------------------------------------
        float linkedGrDb = 0.0f;
        for (int ch = 0; ch < numChannels && ch < kMaxChannels; ++ch)
            linkedGrDb = std::min (linkedGrDb, gainReductionEnvDb[ch]);

        const float grLinear = GainComputer::dbToLinear (linkedGrDb);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float inSample = audioBuffer.getSample (ch, s);
            audioBuffer.setSample (ch, s, inSample * grLinear * mkGain);
        }

        // Accumulate for metering
        grAccumDb += linkedGrDb;
    }

    // ------------------------------------------------------------------
    // 4. Optional saturation stage (very light — VCA is clean)
    // ------------------------------------------------------------------
    saturation.processBlock (audioBuffer);

    // ------------------------------------------------------------------
    // 5. Update metering
    // ------------------------------------------------------------------
    currentGainReductionDb = grAccumDb / static_cast<float> (numSamples);
}

//==============================================================================
float VCAEngine::getGainReductionDb() const
{
    return currentGainReductionDb;
}

//==============================================================================
int VCAEngine::getLatencySamples() const
{
    return saturation.getLatencySamples();
}

//==============================================================================
float VCAEngine::computeEffectiveReleaseMs (int channelIndex, float grDb,
                                            float baseReleaseMs, bool autoEnabled)
{
    //--------------------------------------------------------------------------
    // Auto-release: dual time-constant program-dependent release.
    //
    // The idea: short transients should recover quickly (fast release),
    // while sustained heavy compression should release slowly (to avoid
    // pumping/breathing artifacts).
    //
    // Implementation:
    //   1. Track the GR signal with a slow envelope follower
    //   2. Blend between fast release (~50ms) and slow release (~500ms)
    //      based on the follower's output:
    //      - Low GR envelope → mostly fast release
    //      - High GR envelope → mostly slow release
    //
    // When auto-release is disabled, simply return the user's release setting.
    //--------------------------------------------------------------------------
    if (! autoEnabled)
        return baseReleaseMs;

    const int ch = juce::jlimit (0, kMaxChannels - 1, channelIndex);

    // Normalize the GR envelope to a blend factor [0, 1]
    // At 0 dB GR: blend = 0 (all fast)
    // At 20 dB GR: blend = 1 (all slow)
    const float absGr = std::abs (autoReleaseEnv[ch]);
    const float blendFactor = juce::jlimit (0.0f, 1.0f, absGr / 20.0f);

    // Blend between fast and slow release
    return kFastReleaseMs * (1.0f - blendFactor) + kSlowReleaseMs * blendFactor;
}

} // namespace valvane
