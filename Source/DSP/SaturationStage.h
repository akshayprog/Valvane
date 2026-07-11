#pragma once
#include <JuceHeader.h>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════
// SaturationStage
//
// Character-dependent nonlinear waveshaping with mandatory 2x oversampling
// (juce::dsp::Oversampling) to suppress aliasing from the nonlinearity.
//
// Signal flow inside processBlock():
//   1. Pre-emphasis shelf (boost below ~200 Hz)
//   2. Upsample 2x
//   3. Per-sample waveshaping:
//        a) Asymmetric even-harmonic (tube-style):  y = x - k * x^2
//        b) Odd-harmonic saturation:                y = tanh(drive * x) / tanh(drive)
//   4. Downsample 2x
//   5. De-emphasis shelf (undo the boost)
//
// The pre/de-emphasis pair causes low frequencies to saturate more heavily
// than highs, emulating transformer-core–style behaviour.
//
// Latency from oversampling is reported via getLatencySamples() and must be
// summed into the plugin's total reported latency.
// ═══════════════════════════════════════════════════════════════════════════
class SaturationStage
{
public:
    SaturationStage();
    ~SaturationStage();

    void prepare (double sampleRate, int maxBlockSize, int numChannels = 2);
    void reset();

    /// Process a stereo (or mono) block with oversampled saturation.
    void processBlock (juce::AudioBuffer<float>& buffer);

    /// Process a single sample without oversampling (for use inside an
    /// already-oversampled context, e.g. a coupled engine).
    float processSampleRaw (float input) const;

    // ---- Configuration ----
    void setDrive            (float d)    { drive = std::max (d, 0.01f); }
    void setAsymmetry        (float k)    { asymmetry = juce::jlimit (0.0f, 0.25f, k); }
    void setEnabled          (bool e)     { enabled = e; }
    void setPreEmphasisFreq  (float hz);

    float getDrive()       const { return drive; }
    float getAsymmetry()   const { return asymmetry; }
    bool  isEnabled()      const { return enabled; }
    int   getLatencySamples() const;

private:
    // Oversampling (2x, IIR half-band, 1 stage)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    float  drive          = 1.0f;     // 1.0 = unity, higher = more saturation
    float  asymmetry      = 0.10f;    // k in y = x - k*x^2 (even harmonics)
    bool   enabled        = true;
    double currentSR      = 44100.0;

    // Pre-emphasis / de-emphasis shelving filters (per-channel)
    static constexpr int kMaxChannels = 2;
    juce::dsp::IIR::Filter<float> preEmphasis[kMaxChannels];
    juce::dsp::IIR::Filter<float> deEmphasis[kMaxChannels];
    float preEmphasisFreq = 200.0f;

    void updateEmphasisFilters();
};

} // namespace valvane
