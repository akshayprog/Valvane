#include "OptoEngine.h"
#include <cmath>
#include <algorithm>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

OptoEngine::OptoEngine(std::atomic<float>* gain,
                       std::atomic<float>* peakReduction,
                       std::atomic<float>* compLimit,
                       std::atomic<float>* detectorType,
                       std::atomic<float>* detectorBlend)
    : paramGain          (gain),
      paramPeakReduction (peakReduction),
      paramCompLimit     (compLimit),
      paramDetectorType  (detectorType),
      paramDetectorBlend (detectorBlend)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

void OptoEngine::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    // ── smoothed params: ~20 ms ramp ──
    smoothGainDb.reset(sampleRate, 0.02);
    smoothPeakReduction.reset(sampleRate, 0.02);

    // ── envelope detector (used to extract the level that drives the EL panel) ──
    detector.prepare(sampleRate);

    //  LA-2A attack is ~10 ms (EL panel phosphor rise time), release ~60 ms
    //  for the fast initial decay of the LDR; the slow tail is handled by
    //  the ODE memory state below.
    detector.setAttackMs(10.0f);
    detector.setReleaseMs(60.0f);

    // ── saturation (adds second-harmonic warmth typical of opto units) ──
    saturation.prepare(sampleRate, maxBlockSize, 2);
    saturation.setDrive(0.15f);        // moderate warmth
    saturation.setAsymmetry(0.10f);    // slight even-harmonic bias
    saturation.setEnabled(true);

    reset();
}

void OptoEngine::reset()
{
    detector.reset();
    saturation.reset();

    smoothGainDb.setCurrentAndTargetValue(paramGain ? paramGain->load() : 0.0f);
    smoothPeakReduction.setCurrentAndTargetValue(
        paramPeakReduction ? paramPeakReduction->load() : 0.0f);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        conductance[ch]    = 0.0f;
        memory[ch]         = 0.0f;
        powerLawAccum[ch]  = 1.0f;   // initialise to 1 so log(accum) = 0
    }

    currentGainReductionDb = 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Opto-Cell ODE  (per-sample)
// ═══════════════════════════════════════════════════════════════════════════════
//
// The T4 opto-cell is modelled as two coupled differential equations:
//
//   dG/dt  = (G_target − G) / τ_fast(memory)          ... (1)  conductance
//   dM/dt  = (G − M) / τ_slow                          ... (2)  memory
//
// where:
//   G         = conductance           (0 … 1), maps directly to gain reduction
//   M         = memory state          (0 … ∞), integrates recent GR
//   G_target  = target conductance    derived from the EL panel brightness
//   τ_fast    = attack/release TC for the LDR; release is modulated by M
//   τ_slow    = memory integration TC (~1–5 s)
//
// Power-law release approximation:
//   The LDR's release is NOT a simple exponential.  Real CdS cells follow
//   approximately R(t) ∝ t^(−n)  with n ≈ 0.5–0.8.
//   We approximate this by letting the release coefficient decay each sample:
//
//     α_release[n+1] = α_release[n] · (1 − ε)    (slow geometric decay)
//
//   so the effective time constant grows over time, producing a power-law tail.
//   ε is derived from sampleRate to keep behaviour consistent.
//
// Comp vs. Limit mode:
//   In "Comp" mode the drive-to-conductance mapping is gentle (~3:1 equiv).
//   In "Limit" mode the mapping is aggressive (≈ 15:1 equiv), achieved by
//   raising the drive exponent rather than just switching a ratio parameter.
// ─────────────────────────────────────────────────────────────────────────────

float OptoEngine::processOptoSample(float detectorLevel, int channel, bool limitMode)
{
    const double sr = currentSampleRate;

    // ── 1.  Derive target conductance from detector level ──
    //
    //  Map Peak Reduction [0…100] to a drive amount.
    //  Higher PR → lower effective threshold → more signal drives the EL panel.
    //
    //  Compress mode:  G_target = clamp( (drive · level)^0.6 )     soft ≈ 3:1
    //  Limit   mode:  G_target = clamp( (drive · level)^1.5 )     hard ≈ 15:1
    //
    //  The exponent difference is the key mechanism — NOT just a ratio change.

    const float peakRed = smoothPeakReduction.getNextValue();

    // peakRed [0…100] → drive [0…~4].  Exponential mapping so the knob
    // feels like the real LA-2A where the first 30% is subtle and the
    // last 30% is aggressive.
    const float drive = std::pow(10.0f, (peakRed - 50.0f) / 50.0f);  // ~0.1 at 0, ~1 at 50, ~10 at 100

    const float drivenLevel = drive * detectorLevel;

    float gTarget;
    if (limitMode)
    {
        // Limit: aggressive power-law mapping
        gTarget = std::min(1.0f, std::pow(drivenLevel, 1.5f));
    }
    else
    {
        // Compress: gentle power-law mapping (≈ 3:1 equivalent)
        gTarget = std::min(1.0f, std::pow(drivenLevel, 0.6f));
    }

    // ── 2.  Compute time constants from sample rate ──
    //
    //  τ_fast_attack  ≈ 10 ms  (EL panel rise time — fixed)
    //  τ_fast_release ≈ 60 ms  base, modulated by memory → up to ~2 s
    //  τ_slow         ≈ 2 s    (memory integrator)

    const float tauAttackSec  = 0.010f;
    const float tauRelBaseSec = 0.060f;
    const float tauSlowSec    = 2.0f;

    //  Memory modulates release: more memory → slower release
    //  τ_rel_effective = τ_base + memory · scaling
    const float memoryScale = 1.5f;
    const float tauRelEffective = tauRelBaseSec + memory[channel] * memoryScale;

    //  Convert τ (seconds) to per-sample smoothing coefficient:
    //  α = 1 − exp(−1 / (τ · sampleRate))
    const float alphaAttack  = 1.0f - std::exp(-1.0f / (float)(tauAttackSec  * sr));
    const float alphaRelease = 1.0f - std::exp(-1.0f / (float)(tauRelEffective * sr));

    // ── 3.  Power-law release approximation ──
    //
    //  Instead of a fixed α_release, we decay the accumulator each sample
    //  so that the effective release slows down over time:
    //
    //    accum[n+1] = accum[n] + ε
    //    α_rel_effective = α_release / accum[n]^n
    //
    //  where n ∈ [0.5, 0.8] controls the power-law exponent.
    //  ε = 1 / (sampleRate · T_window) keeps the accumulator growing in
    //  real-time units.

    const float powerLawN     = 0.65f;   // exponent (0.5–0.8 range)
    const float windowTimeSec = 5.0f;    // accumulator reset window
    const float epsilon       = 1.0f / (float)(sr * windowTimeSec);

    float& accum = powerLawAccum[channel];

    if (gTarget > conductance[channel])
    {
        // ── Attack phase: conductance rising → use fast α ──
        accum = 1.0f;   // reset power-law accumulator on new attack
        conductance[channel] += alphaAttack * (gTarget - conductance[channel]);
    }
    else
    {
        // ── Release phase: conductance falling → power-law decay ──
        accum += epsilon;
        const float powerLawMod   = std::pow(accum, powerLawN);
        const float alphaRelPL    = alphaRelease / std::max(powerLawMod, 1.0f);
        conductance[channel] += alphaRelPL * (gTarget - conductance[channel]);
    }

    // Clamp conductance to [0, 1]
    conductance[channel] = std::clamp(conductance[channel], 0.0f, 1.0f);

    // ── 4.  Update memory state (slow integrator of conductance) ──
    //
    //  dM/dt = (G − M) / τ_slow   →   M[n+1] = M[n] + α_slow · (G − M)

    const float alphaSlow = 1.0f - std::exp(-1.0f / (float)(tauSlowSec * sr));
    memory[channel] += alphaSlow * (conductance[channel] - memory[channel]);
    memory[channel] = std::max(0.0f, memory[channel]);

    // ── 5.  Map conductance → gain reduction (dB) ──
    //
    //  Linear mapping:  GR_dB = −conductance · maxGR
    //  maxGR in Comp mode ≈ 15 dB, Limit mode ≈ 30 dB

    const float maxGrDb = limitMode ? 30.0f : 15.0f;
    const float grDb = -conductance[channel] * maxGrDb;

    return grDb;   // negative value = gain reduction
}

// ═══════════════════════════════════════════════════════════════════════════════
// processBlock
// ═══════════════════════════════════════════════════════════════════════════════

void OptoEngine::processBlock(juce::AudioBuffer<float>& audioBuffer,
                              const juce::AudioBuffer<float>& detectorInput)
{
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples  = audioBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // ── read atomic params ──
    const float targetGainDb  = paramGain          ? paramGain->load()          : 0.0f;
    const float targetPR      = paramPeakReduction ? paramPeakReduction->load() : 0.0f;
    const bool  limitMode     = paramCompLimit     ? (paramCompLimit->load() >= 0.5f) : false;

    smoothGainDb.setTargetValue(targetGainDb);
    smoothPeakReduction.setTargetValue(targetPR);

    // ── configure detector ──
    if (paramDetectorType)
    {
        const int dt = static_cast<int>(paramDetectorType->load());
        detector.setType(static_cast<EnvelopeDetector::Type>(
            std::clamp(dt, 0, 2)));
    }
    if (paramDetectorBlend)
        detector.setBlend(paramDetectorBlend->load() * 0.01f);  // 0–100% → 0–1

    // ── per-sample processing ──
    float peakGr = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // ── Build mono detector signal from detectorInput ──
        float detMono = 0.0f;
        const int detChannels = detectorInput.getNumChannels();
        for (int ch = 0; ch < detChannels; ++ch)
            detMono += std::abs(detectorInput.getSample(ch, s));
        if (detChannels > 1)
            detMono /= static_cast<float>(detChannels);

        // ── Envelope detection (smooth the rectified signal) ──
        const float envLevel = detector.processSample(detMono);

        // ── Get smoothed makeup gain for this sample ──
        const float makeupGainDb = smoothGainDb.getNextValue();

        // ── Process each audio channel ──
        for (int ch = 0; ch < numChannels; ++ch)
        {
            // Opto-cell ODE step → gain reduction in dB
            const float grDb = processOptoSample(envLevel, std::min(ch, kMaxChannels - 1), limitMode);

            // Convert total gain (GR + makeup) to linear multiplier
            const float totalGainDb  = grDb + makeupGainDb;
            const float gainLinear   = GainComputer::dbToLinear(totalGainDb);

            // Apply gain
            float* samplePtr = audioBuffer.getWritePointer(ch);
            samplePtr[s] *= gainLinear;

            // Track worst-case GR for metering
            peakGr = std::min(peakGr, grDb);
        }
    }

    // ── Saturation stage (warm, subtle even harmonics) ──
    saturation.processBlock(audioBuffer);

    // ── Update metering value ──
    currentGainReductionDb = peakGr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Accessors
// ═══════════════════════════════════════════════════════════════════════════════

float OptoEngine::getGainReductionDb() const
{
    return currentGainReductionDb;
}

int OptoEngine::getLatencySamples() const
{
    return saturation.getLatencySamples();
}

} // namespace valvane
