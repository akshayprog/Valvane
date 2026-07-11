#pragma once

#include <JuceHeader.h>
#include "CompressorEngine.h"
#include "EnvelopeDetector.h"
#include "GainComputer.h"

namespace valvane
{

//==============================================================================
/**
    VariMuEngine — Fairchild 670-inspired variable-mu tube compressor.

    Architecture (CRITICAL DESIGN):
    --------------------------------
    Unlike conventional compressors that chain detector → gain-computer → VCA → saturation
    as separate blocks, the Vari-Mu models gain reduction and harmonic distortion as
    COUPLED outputs of ONE shared state variable: the tube grid bias.

    The detector drives a slow-moving `gridBias` state variable (0 = no compression,
    1 = maximum compression). The audio is then processed through a tube transfer
    function whose shape depends on `gridBias`:

        1. As gridBias increases → gain decreases (compression)
        2. As gridBias increases → 2nd/3rd harmonic content increases (warmth/color)

    These two effects emerge from the SAME nonlinear function, not from separate stages.

    Transfer function model:
        Soft-clipping log-exponential model inspired by triode plate characteristics:
            V_out = (Vp / k) * ln(1 + exp(k * V_in / Vp))
        where Vp (plate voltage proxy) and k (curve sharpness) shift with gridBias.

    Time constants:
        The slowest, most logarithmic response of all four modes:
        - Program-dependent: heavier GR → longer release
        - Time Constant macro (0–100) blends between:
            0  = fastest: attack ~1ms,  release ~300ms
            100 = slowest: attack ~20ms, release ~5000ms

    Internal ratio: fixed curve, approximately 2:1 to 5:1 depending on level (soft knee).
    No user-adjustable ratio knob.

    Oversampling: Internal 2× oversampling via juce::dsp::Oversampling for the nonlinear
    tube function, reported through getLatencySamples().
*/
class VariMuEngine : public CompressorEngine
{
public:
    //==========================================================================
    VariMuEngine (std::atomic<float>* input,
                  std::atomic<float>* output,
                  std::atomic<float>* threshold,
                  std::atomic<float>* timeConstant,
                  std::atomic<float>* detectorType,
                  std::atomic<float>* detectorBlend);

    ~VariMuEngine() override = default;

    //==========================================================================
    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;
    void processBlock (juce::AudioBuffer<float>& audioBuffer,
                       const juce::AudioBuffer<float>& detectorInput) override;
    float getGainReductionDb() const override;
    int getLatencySamples() const override;

private:
    //==========================================================================
    // Parameter pointers (atomic, from the APVTS)
    std::atomic<float>* inputParam      = nullptr;
    std::atomic<float>* outputParam     = nullptr;
    std::atomic<float>* thresholdParam  = nullptr;
    std::atomic<float>* timeConstParam  = nullptr;
    std::atomic<float>* detTypeParam    = nullptr;
    std::atomic<float>* detBlendParam   = nullptr;

    //==========================================================================
    // Smoothed parameters (per-sample interpolation)
    juce::SmoothedValue<float> inputGainSmoothed;
    juce::SmoothedValue<float> outputGainSmoothed;
    juce::SmoothedValue<float> thresholdSmoothed;

    //==========================================================================
    // Core state: the grid bias per channel (0 = no GR, 1 = max GR)
    static constexpr int kMaxChannels = 2;
    float gridBias[kMaxChannels] = {};

    //==========================================================================
    // Envelope detector (for sidechain level measurement)
    EnvelopeDetector detector[kMaxChannels];

    //==========================================================================
    // Oversampling for the nonlinear tube function (2× with IIR half-band filters)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    //==========================================================================
    // Internal state
    double currentSampleRate = 44100.0;
    float currentGainReductionDb = 0.0f;

    //==========================================================================
    // Tube model constants
    // Vp: virtual plate voltage scaling. Controls saturation ceiling.
    // k:  curve sharpness. Higher k = sharper nonlinearity onset.
    static constexpr float kVpBase     = 1.5f;   // base plate voltage proxy
    static constexpr float kSharpBase  = 3.0f;   // base curve sharpness

    // Internal soft-knee ratio curve parameters
    static constexpr float kInternalThresholdKneeDb = 12.0f;  // wide soft knee
    static constexpr float kMinRatio = 2.0f;   // low-level ratio
    static constexpr float kMaxRatio = 5.0f;   // high-level ratio

    //==========================================================================
    /** Process a single sample through the bias-dependent tube transfer function.
        @param input   The input sample (after input drive)
        @param bias    The current grid bias (0..1)
        @return        The processed sample with coupled GR + harmonics
    */
    float processTubeSample (float input, float bias) const;

    /** Compute the envelope follower time constants from the macro knob (0–100).
        Returns { attackMs, releaseMs }.
    */
    std::pair<float, float> computeTimeConstants (float macroValue) const;

    /** Compute the target grid bias from the detector level and threshold.
        Uses the internal soft-knee ratio curve.
        @param detectorLevelDb  Detected sidechain level in dB
        @param thresholdDb      User threshold in dB
        @return                 Target bias in range [0, 1]
    */
    float computeTargetBias (float detectorLevelDb, float thresholdDb) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariMuEngine)
};

} // namespace valvane
