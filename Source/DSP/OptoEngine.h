#pragma once

#include <JuceHeader.h>
#include "CompressorEngine.h"
#include "EnvelopeDetector.h"
#include "GainComputer.h"
#include "SaturationStage.h"

namespace valvane {

/**
 * OptoEngine — LA-2A-style electro-optical compressor engine.
 *
 * Models the T4 opto-cell (electroluminescent panel + cadmium-sulphide LDR)
 * as a coupled nonlinear ODE system with two state variables:
 *
 *   1. conductance  –  instantaneous photocell conductance (fast, driven by
 *                      the EL panel brightness which tracks rectified audio)
 *   2. memory       –  slow integrator of recent gain-reduction history;
 *                      modulates the release time constant, reproducing the
 *                      real unit's program-dependent release behaviour
 *
 * Recovery follows a power-law decay: R(t) ∝ t^(−n), n ≈ 0.5–0.8,
 * approximated recursively per sample (no lookup table).
 *
 * Controls:
 *   • Gain (makeup)          –  −12 … +36 dB
 *   • Peak Reduction (drive) –  0 … 100 (maps to threshold-equivalent drive)
 *   • Comp / Limit toggle    –  changes drive-to-GR mapping, not just ratio
 *
 * Fixed soft knee.  No user attack / release controls.
 */
class OptoEngine final : public CompressorEngine
{
public:
    OptoEngine(std::atomic<float>* gain,
               std::atomic<float>* peakReduction,
               std::atomic<float>* compLimit,
               std::atomic<float>* detectorType,
               std::atomic<float>* detectorBlend);

    ~OptoEngine() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void processBlock(juce::AudioBuffer<float>& audioBuffer,
                      const juce::AudioBuffer<float>& detectorInput) override;
    float getGainReductionDb() const override;
    int   getLatencySamples() const override;

private:
    // ── parameter pointers (from APVTS) ──
    std::atomic<float>* paramGain            = nullptr;
    std::atomic<float>* paramPeakReduction   = nullptr;
    std::atomic<float>* paramCompLimit       = nullptr;
    std::atomic<float>* paramDetectorType    = nullptr;
    std::atomic<float>* paramDetectorBlend   = nullptr;

    // ── smoothed parameters ──
    juce::SmoothedValue<float> smoothGainDb;
    juce::SmoothedValue<float> smoothPeakReduction;

    // ── opto-cell ODE state (per channel) ──
    static constexpr int kMaxChannels = 2;
    float conductance[kMaxChannels] {};   // photocell conductance [0…1]
    float memory     [kMaxChannels] {};   // slow memory integrator [0…∞)
    float powerLawAccum[kMaxChannels] {}; // recursive power-law accumulator

    // ── envelope detector ──
    EnvelopeDetector detector;

    // ── saturation ──
    SaturationStage saturation;

    // ── runtime state ──
    double currentSampleRate = 44100.0;
    float  currentGainReductionDb = 0.0f;

    // ── internal helpers ──
    /** Per-sample opto-cell ODE step; returns gain reduction in dB (negative). */
    float processOptoSample(float detectorLevel, int channel, bool limitMode);
};

} // namespace valvane
