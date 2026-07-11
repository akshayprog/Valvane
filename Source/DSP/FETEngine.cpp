#include "FETEngine.h"
#include <cmath>
#include <algorithm>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════════
// Construction
// ═══════════════════════════════════════════════════════════════════════════════

FETEngine::FETEngine(std::atomic<float>* input,
                     std::atomic<float>* output,
                     std::atomic<float>* attack,
                     std::atomic<float>* release,
                     std::atomic<float>* ratio,
                     std::atomic<float>* allButtons,
                     std::atomic<float>* detectorType,
                     std::atomic<float>* detectorBlend)
    : paramInput       (input),
      paramOutput      (output),
      paramAttack      (attack),
      paramRelease     (release),
      paramRatio       (ratio),
      paramAllButtons  (allButtons),
      paramDetectorType(detectorType),
      paramDetectorBlend(detectorBlend)
{
}

// ═══════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═══════════════════════════════════════════════════════════════════════════════

void FETEngine::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;

    // ── smoothed params: ~10 ms ramp for click-free transitions ──
    smoothInputDb.reset(sampleRate, 0.01);
    smoothOutputDb.reset(sampleRate, 0.01);
    smoothAttackMs.reset(sampleRate, 0.01);
    smoothReleaseMs.reset(sampleRate, 0.01);

    // ── envelope detector ──
    detector.prepare(sampleRate);

    // ── saturation (FET transistor character: moderate-heavy, odd harmonics) ──
    saturation.prepare(sampleRate, maxBlockSize, 2);
    saturation.setDrive(0.35f);        // moderate-heavy FET crunch
    saturation.setAsymmetry(0.05f);    // slight asymmetry
    saturation.setEnabled(true);

    reset();
}

void FETEngine::reset()
{
    detector.reset();
    saturation.reset();

    smoothInputDb.setCurrentAndTargetValue(paramInput   ? paramInput->load()   : 0.0f);
    smoothOutputDb.setCurrentAndTargetValue(paramOutput  ? paramOutput->load()  : 0.0f);
    smoothAttackMs.setCurrentAndTargetValue(paramAttack  ? paramAttack->load()  : 0.2f);
    smoothReleaseMs.setCurrentAndTargetValue(paramRelease ? paramRelease->load() : 300.0f);

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        envelopeState[ch]      = 0.0f;
        gainReductionLinear[ch] = 1.0f;
    }

    currentGainReductionDb = 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Diode-Bridge Nonlinear Pre-Shaping
// ═══════════════════════════════════════════════════════════════════════════════
//
// The 1176's feedback detector uses a diode bridge whose transfer function
// is inherently nonlinear.  We approximate this with a Shockley-diode-inspired
// function applied to the rectified detector input:
//
//   f(x) = sign(x) · V_T · ln(1 + |x| / V_T)
//
// This is the inverse of the exponential I-V curve (I ∝ exp(V/V_T) − 1),
// which compresses large peaks while leaving small signals relatively linear.
// V_T ≈ 0.05 is a tuning parameter (not physical thermal voltage) chosen to
// give a pleasing amount of detector-path "softening".
//
// The effect: the detector "sees" a subtly compressed version of the input,
// so transient peaks cause proportionally less gain reduction than they would
// in a linear detector — reproducing the 1176's famously smooth transient
// handling.
// ─────────────────────────────────────────────────────────────────────────────

float FETEngine::applyDiodeBridgeShaping(float rectifiedInput) const
{
    // f(x) = V_T · ln(1 + x / V_T)  for x ≥ 0
    //
    // Properties:
    //   f(0)       = 0
    //   f'(0)      = 1         (unity gain for small signals)
    //   f(x → ∞)  ≈ V_T·ln(x) (logarithmic compression of peaks)
    const float x = std::abs(rectifiedInput);
    return kThermalVoltage * std::log(1.0f + x / kThermalVoltage);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Level-Dependent Release Coefficient
// ═══════════════════════════════════════════════════════════════════════════════
//
// The 1176's release is not purely exponential — it exhibits a subtle
// level-dependent speed-up where the release gets faster as gain reduction
// decreases.  We model this as:
//
//   τ_effective = τ_base · (1 − k · |GR_dB| / maxGR)
//
// where k ∈ (0, 0.5) controls the modulation depth and maxGR is a
// normalisation constant.  When GR is heavy (e.g. −20 dB), release is
// slower; as GR approaches 0, release speeds up.
//
// The per-sample coefficient is:
//   α_release = 1 − exp(−1 / (τ_effective · sampleRate))
// ─────────────────────────────────────────────────────────────────────────────

float FETEngine::computeReleaseCoefficientWithGRModulation(float baseReleaseMs,
                                                            float currentGrDb) const
{
    const float baseReleaseSec = baseReleaseMs * 0.001f;

    // |GR| normalised to [0, 1] range (assuming max −40 dB)
    const float grNorm = std::clamp(std::abs(currentGrDb) / 40.0f, 0.0f, 1.0f);

    // Modulation: release gets faster (τ shrinks) as GR decreases
    //   k = 0.35 → up to 35% speed-up when GR → 0
    const float k = 0.35f;
    const float tauEffective = baseReleaseSec * (1.0f - k * (1.0f - grNorm));

    // Ensure τ never goes below 5 ms (safety floor)
    const float tauClamped = std::max(tauEffective, 0.005f);

    return 1.0f - std::exp(-1.0f / (tauClamped * static_cast<float>(currentSampleRate)));
}

// ═══════════════════════════════════════════════════════════════════════════════
// processBlock
// ═══════════════════════════════════════════════════════════════════════════════

void FETEngine::processBlock(juce::AudioBuffer<float>& audioBuffer,
                             const juce::AudioBuffer<float>& detectorInput)
{
    const int numChannels = audioBuffer.getNumChannels();
    const int numSamples  = audioBuffer.getNumSamples();

    if (numChannels == 0 || numSamples == 0)
        return;

    // ── read atomic parameters ──
    const float targetInputDb  = paramInput     ? paramInput->load()     : 0.0f;
    const float targetOutputDb = paramOutput    ? paramOutput->load()    : 0.0f;
    const float targetAttackMs = paramAttack    ? paramAttack->load()    : 0.2f;
    const float targetReleaseMs= paramRelease   ? paramRelease->load()   : 300.0f;
    const int   ratioIndex     = paramRatio     ? static_cast<int>(paramRatio->load()) : 0;
    const bool  allButtonsMode = paramAllButtons? (paramAllButtons->load() >= 0.5f) : false;

    smoothInputDb.setTargetValue(targetInputDb);
    smoothOutputDb.setTargetValue(targetOutputDb);
    smoothAttackMs.setTargetValue(targetAttackMs);
    smoothReleaseMs.setTargetValue(targetReleaseMs);

    // ── ratio ──
    const float ratio = allButtonsMode
        ? 100.0f  // All-Buttons: extreme ratio
        : kRatioTable[std::clamp(ratioIndex, 0, 3)];

    // ── fixed soft knee for FET character (narrower than opto) ──
    const float kneeWidthDb = 4.0f;

    // ── threshold for FET: fixed at −10 dBFS (input drive pushes signal into it) ──
    const float thresholdDb = -10.0f;

    // ── configure detector ──
    if (paramDetectorType)
    {
        const int dt = static_cast<int>(paramDetectorType->load());
        detector.setType(static_cast<EnvelopeDetector::Type>(
            std::clamp(dt, 0, 2)));
    }
    if (paramDetectorBlend)
        detector.setBlend(paramDetectorBlend->load() * 0.01f);  // 0–100% → 0–1

    // ── All-Buttons: crank the saturation and use more extreme settings ──
    if (allButtonsMode)
    {
        saturation.setDrive(0.75f);      // heavy distortion
        saturation.setAsymmetry(0.20f);  // more even harmonics for grit
    }
    else
    {
        saturation.setDrive(0.35f);      // standard FET character
        saturation.setAsymmetry(0.05f);
    }

    // ── per-sample processing ──
    float peakGr = 0.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        // ── smoothed parameter values for this sample ──
        const float inputGainDb  = smoothInputDb.getNextValue();
        const float outputGainDb = smoothOutputDb.getNextValue();
        const float attackMs     = smoothAttackMs.getNextValue();
        const float releaseMs    = smoothReleaseMs.getNextValue();

        const float inputGainLin  = GainComputer::dbToLinear(inputGainDb);
        const float outputGainLin = GainComputer::dbToLinear(outputGainDb);

        // ── Build mono detector signal ──
        float detMono = 0.0f;
        const int detChannels = detectorInput.getNumChannels();
        for (int ch = 0; ch < detChannels; ++ch)
            detMono += std::abs(detectorInput.getSample(ch, s));
        if (detChannels > 1)
            detMono /= static_cast<float>(detChannels);

        // ── Apply input drive to detector path ──
        detMono *= inputGainLin;

        // ── Diode-bridge nonlinear pre-shaping ──
        //  This happens BEFORE the envelope follower, modelling how
        //  the diode bridge's nonlinearity affects detector sensitivity.
        const float shapedDet = applyDiodeBridgeShaping(detMono);

        // ── Envelope follower with per-sample attack / release ──
        //
        //  Attack time: user knob (0.02–0.8 ms), i.e. 20–800 µs.
        //  In All-Buttons mode: attack is halved for more aggressive response,
        //  with a subtle pseudo-random modulation to emulate the erratic
        //  behaviour of the real unit with all buttons jammed.
        float effectiveAttackMs  = attackMs;
        float effectiveReleaseMs = releaseMs;

        if (allButtonsMode)
        {
            // ── All-Buttons erratic modulation ──
            //  Use a simple hash of the sample index for fast modulation
            //  (no need for a full LFO — we want chaos, not periodicity).
            const float mod = 1.0f + 0.3f * std::sin(static_cast<float>(s) * 0.37f
                                                     + static_cast<float>(s * s) * 0.0013f);
            effectiveAttackMs  = attackMs * 0.5f * mod;
            effectiveReleaseMs = releaseMs * 0.6f * mod;
        }

        //  Convert ms → per-sample coefficient:
        //    α = 1 − exp(−1 / (τ_sec · sampleRate))
        const float attackSec  = std::max(effectiveAttackMs * 0.001f, 1.0e-6f);
        const float alphaAttack = 1.0f - std::exp(-1.0f / (attackSec * static_cast<float>(currentSampleRate)));

        // ── Process each audio channel ──
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const int ci = std::min(ch, kMaxChannels - 1);

            // ── Envelope follower (asymmetric: fast attack, slow release) ──
            float& env = envelopeState[ci];
            if (shapedDet > env)
            {
                // Attack
                env += alphaAttack * (shapedDet - env);
            }
            else
            {
                // Release: level-dependent coefficient
                const float grDbCurrent = (gainReductionLinear[ci] < 1.0f)
                    ? GainComputer::linearToDb(gainReductionLinear[ci])
                    : 0.0f;
                const float alphaRelease = computeReleaseCoefficientWithGRModulation(
                    effectiveReleaseMs, grDbCurrent);
                env += alphaRelease * (shapedDet - env);
            }
            env = std::max(env, 0.0f);

            // ── Convert envelope to dB ──
            const float envDb = GainComputer::linearToDb(env + 1.0e-30f);

            // ── Gain computation (Giannoulis/Reiss/Rice 2012 style) ──
            //
            //  desiredGainDb = computeGainDb(envDb, threshold, ratio, knee)
            //  This returns the target output level for the given input level.
            //  The gain reduction is:  GR = desiredGainDb − envDb
            const float desiredDb = GainComputer::computeGainDb(
                envDb, thresholdDb, ratio, kneeWidthDb);
            const float grDb = desiredDb - envDb;  // ≤ 0

            // ── Convert GR to linear and apply to audio ──
            const float grLinear = GainComputer::dbToLinear(grDb);
            gainReductionLinear[ci] = grLinear;

            // Total gain = input drive · GR · output makeup
            const float totalGainLinear = inputGainLin * grLinear * outputGainLin;

            float* samplePtr = audioBuffer.getWritePointer(ch);
            samplePtr[s] *= totalGainLinear;

            // Track worst-case GR for metering (report only GR, not makeup)
            peakGr = std::min(peakGr, grDb);
        }
    }

    // ── Saturation stage (FET transistor harmonics) ──
    saturation.processBlock(audioBuffer);

    // ── Update metering value ──
    currentGainReductionDb = peakGr;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Accessors
// ═══════════════════════════════════════════════════════════════════════════════

float FETEngine::getGainReductionDb() const
{
    return currentGainReductionDb;
}

int FETEngine::getLatencySamples() const
{
    return saturation.getLatencySamples();
}

} // namespace valvane
