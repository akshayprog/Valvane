#pragma once

#include <JuceHeader.h>
#include "CompressorEngine.h"
#include "EnvelopeDetector.h"
#include "GainComputer.h"
#include "SaturationStage.h"

namespace valvane
{

//==============================================================================
/**
    VCAEngine — SSL G-Bus-style voltage-controlled amplifier compressor.

    Architecture:
    --------------------------------
    The most straightforward, "textbook" topology:
        detect → compute gain reduction → apply gain → optional light saturation

    This is the transparent/modern/clean option. No coupling between dynamics
    and harmonic content. Uses the Giannoulis/Reiss/Rice (2012) soft-knee
    gain computer for precise, predictable compression curves.

    Detector:
        Switchable Peak / RMS / Blend via the shared DETECTOR_TYPE parameter.
        Uses the EnvelopeDetector class for envelope following.

    Gain Computer:
        Soft-knee curve from GainComputer::computeGainDb() with user-adjustable
        knee width (0 dB = hard knee, 24 dB = very soft knee).

    Auto-Release:
        Dual time-constant program-dependent release (when enabled):
        - Fast release (~50ms)  for transient recovery
        - Slow release (~500ms) for sustained compression
        - A slow envelope follower tracks the GR signal
        - More sustained GR → more slow-release weighting
        - Brief transients → fast release dominates

    Saturation:
        SaturationStage with very light drive (effectively disabled for cleanest output).
        VCA is the "clean" option — saturation is available but minimal.

    Latency:
        Reported from the SaturationStage (if it uses oversampling), otherwise 0.
*/
class VCAEngine : public CompressorEngine
{
public:
    //==========================================================================
    VCAEngine (std::atomic<float>* threshold,
               std::atomic<float>* ratio,
               std::atomic<float>* attack,
               std::atomic<float>* release,
               std::atomic<float>* autoRelease,
               std::atomic<float>* knee,
               std::atomic<float>* makeup,
               std::atomic<float>* detectorType,
               std::atomic<float>* detectorBlend);

    ~VCAEngine() override = default;

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
    std::atomic<float>* thresholdParam   = nullptr;
    std::atomic<float>* ratioParam       = nullptr;
    std::atomic<float>* attackParam      = nullptr;
    std::atomic<float>* releaseParam     = nullptr;
    std::atomic<float>* autoReleaseParam = nullptr;
    std::atomic<float>* kneeParam        = nullptr;
    std::atomic<float>* makeupParam      = nullptr;
    std::atomic<float>* detTypeParam     = nullptr;
    std::atomic<float>* detBlendParam    = nullptr;

    //==========================================================================
    // Smoothed parameters (per-sample interpolation)
    juce::SmoothedValue<float> thresholdSmoothed;
    juce::SmoothedValue<float> ratioSmoothed;
    juce::SmoothedValue<float> kneeSmoothed;
    juce::SmoothedValue<float> makeupSmoothed;

    //==========================================================================
    // Envelope detectors (one per channel for stereo-linked or independent use)
    static constexpr int kMaxChannels = 2;
    EnvelopeDetector detector[kMaxChannels];

    //==========================================================================
    // Gain smoothing state (per channel)
    // Holds the current smoothed gain-reduction envelope in dB (negative values)
    float gainReductionEnvDb[kMaxChannels] = {};

    //==========================================================================
    // Auto-release state
    // Slow envelope follower on the GR signal to blend between fast/slow release
    float autoReleaseEnv[kMaxChannels] = {};

    // Auto-release time constants
    static constexpr float kFastReleaseMs = 50.0f;
    static constexpr float kSlowReleaseMs = 500.0f;
    static constexpr float kAutoReleaseFollowerMs = 200.0f;  // GR envelope follower speed

    //==========================================================================
    // Saturation (very light, VCA is the "clean" option)
    SaturationStage saturation;

    //==========================================================================
    // Internal state
    double currentSampleRate = 44100.0;
    float currentGainReductionDb = 0.0f;

    //==========================================================================
    /** Compute the effective release time in ms, potentially blending fast/slow
        for auto-release mode.
        @param channelIndex  Which channel's auto-release state to use
        @param grDb          Current gain reduction in dB (negative)
        @param baseReleaseMs The user's release knob value in ms
        @param autoEnabled   Whether auto-release is on
        @return              Effective release time in ms
    */
    float computeEffectiveReleaseMs (int channelIndex, float grDb,
                                     float baseReleaseMs, bool autoEnabled);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VCAEngine)
};

} // namespace valvane
