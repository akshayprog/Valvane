#pragma once

#include <JuceHeader.h>
#include "CompressorEngine.h"
#include "EnvelopeDetector.h"
#include "GainComputer.h"
#include "SaturationStage.h"

namespace valvane {

/**
 * FETEngine — 1176-style FET compressor engine.
 *
 * Models the key FET compressor behaviours:
 *
 *   • Diode-bridge nonlinearity  — pre-shapes the detector input using a
 *     Shockley-diode-inspired transfer:  f(x) = sign(x) · (exp(|x|/V_T) − 1)
 *     This makes detector sensitivity depend on signal level, reproducing the
 *     real unit's level-dependent detector behaviour.
 *
 *   • Sub-millisecond attack (20–800 µs), user adjustable.
 *
 *   • Level-dependent release  — release time constant is modulated by
 *     current gain reduction:  τ_rel(GR) = τ_base · (1 − k·|GR|)
 *     so release gets subtly faster as GR decreases, matching the real unit.
 *
 *   • All-Buttons mode — emulates the classic "slam all buttons" trick:
 *       – Ratio → ~100:1
 *       – Heavy saturation drive
 *       – Modulated attack/release for erratic, characterful compression
 *
 * Controls:
 *   • Input   (−20 … +20 dB drive)
 *   • Output  (−20 … +20 dB makeup)
 *   • Attack  (0.02 … 0.8 ms)
 *   • Release (50 … 1200 ms)
 *   • Ratio   (4:1, 8:1, 12:1, 20:1)
 *   • All-Buttons toggle
 */
class FETEngine final : public CompressorEngine
{
public:
    FETEngine(std::atomic<float>* input,
              std::atomic<float>* output,
              std::atomic<float>* attack,
              std::atomic<float>* release,
              std::atomic<float>* ratio,
              std::atomic<float>* allButtons,
              std::atomic<float>* detectorType,
              std::atomic<float>* detectorBlend);

    ~FETEngine() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>& audioBuffer,
                      const juce::AudioBuffer<float>& detectorInput) override;
    float getGainReductionDb() const override;
    int   getLatencySamples() const override;

private:
    // ── parameter pointers ──
    std::atomic<float>* paramInput        = nullptr;
    std::atomic<float>* paramOutput       = nullptr;
    std::atomic<float>* paramAttack       = nullptr;
    std::atomic<float>* paramRelease      = nullptr;
    std::atomic<float>* paramRatio        = nullptr;
    std::atomic<float>* paramAllButtons   = nullptr;
    std::atomic<float>* paramDetectorType = nullptr;
    std::atomic<float>* paramDetectorBlend= nullptr;

    // ── smoothed parameters ──
    juce::SmoothedValue<float> smoothInputDb;
    juce::SmoothedValue<float> smoothOutputDb;
    juce::SmoothedValue<float> smoothAttackMs;
    juce::SmoothedValue<float> smoothReleaseMs;

    // ── envelope detector ──
    EnvelopeDetector detector;

    // ── saturation ──
    SaturationStage saturation;

    // ── FET-specific state (per channel) ──
    static constexpr int kMaxChannels = 2;
    float envelopeState[kMaxChannels] {};  // smoothed envelope level (linear)
    float gainReductionLinear[kMaxChannels] {}; // current GR in linear domain

    // ── runtime ──
    double currentSampleRate = 44100.0;
    float  currentGainReductionDb = 0.0f;

    // ── ratio table ──
    static constexpr float kRatioTable[4] = { 4.0f, 8.0f, 12.0f, 20.0f };

    // ── Shockley diode parameters ──
    static constexpr float kThermalVoltage = 0.05f; // V_T scaling for nonlinear pre-shaping

    // ── internal helpers ──
    /** Apply Shockley-diode nonlinear pre-shaping to rectified detector input. */
    float applyDiodeBridgeShaping(float rectifiedInput) const;

    /** Compute level-dependent release coefficient. */
    float computeReleaseCoefficientWithGRModulation(float baseReleaseMs,
                                                      float currentGrDb) const;
};

} // namespace valvane
