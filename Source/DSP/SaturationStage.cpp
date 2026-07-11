#include "SaturationStage.h"
#include <cmath>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════
// SaturationStage implementation
//
// Waveshaping chain (per sample at oversampled rate):
//   1. Asymmetric even-harmonic:  y = x - k * x^2       (k ≈ 0.05-0.15)
//   2. Odd-harmonic tanh:         y = tanh(drive * x) / tanh(drive)
//
// The asymmetric shaper generates 2nd harmonic content (tube-style), while
// the tanh produces odd harmonics (3rd, 5th, …).  Combining both yields a
// rich analog-like harmonic fingerprint.
//
// Frequency-dependent drive:
//   Pre-emphasis low-shelf (boost below preEmphasisFreq) → nonlinearity →
//   de-emphasis low-shelf (cut to compensate).
//   This makes the low end saturate harder, emulating transformer cores.
//
// Oversampling:
//   juce::dsp::Oversampling with 2x, IIR half-band filter, 1 stage.
//   Latency from oversampling is reported back so the host can compensate.
// ═══════════════════════════════════════════════════════════════════════════

SaturationStage::SaturationStage()
{
}

SaturationStage::~SaturationStage() = default;

void SaturationStage::prepare (double sampleRate, int maxBlockSize, int numChannels)
{
    currentSR = sampleRate;
    const auto chCount = static_cast<size_t> (std::min (numChannels, kMaxChannels));

    // 2x oversampling, IIR half-band, 1 stage
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        chCount,                              // numChannels
        1,                                     // oversampling order (2^1 = 2x)
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true                                   // isMaxQuality
    );
    oversampling->initProcessing (static_cast<size_t> (maxBlockSize));

    // Set up pre/de-emphasis shelving filters
    updateEmphasisFilters();

    for (size_t ch = 0; ch < chCount; ++ch)
    {
        preEmphasis[ch].reset();
        deEmphasis[ch].reset();
    }
}

void SaturationStage::reset()
{
    if (oversampling)
        oversampling->reset();

    for (auto& f : preEmphasis)  f.reset();
    for (auto& f : deEmphasis)   f.reset();
}

void SaturationStage::processBlock (juce::AudioBuffer<float>& buffer)
{
    if (!enabled || buffer.getNumSamples() == 0)
        return;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // ---- 1. Pre-emphasis (boost lows before saturation) ----
    for (int ch = 0; ch < std::min (numChannels, kMaxChannels); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = preEmphasis[ch].processSample (data[i]);
    }

    // ---- 2. Upsample ----
    auto audioBlock = juce::dsp::AudioBlock<float> (buffer);
    auto oversampledBlock = oversampling->processSamplesUp (audioBlock);

    // ---- 3. Apply nonlinearity at oversampled rate ----
    const auto osNumSamples = static_cast<int> (oversampledBlock.getNumSamples());
    for (int ch = 0; ch < static_cast<int> (oversampledBlock.getNumChannels()); ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer (static_cast<size_t> (ch));
        for (int i = 0; i < osNumSamples; ++i)
        {
            data[i] = processSampleRaw (data[i]);
        }
    }

    // ---- 4. Downsample ----
    oversampling->processSamplesDown (audioBlock);

    // ---- 5. De-emphasis (undo the low-frequency boost) ----
    for (int ch = 0; ch < std::min (numChannels, kMaxChannels); ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = deEmphasis[ch].processSample (data[i]);
    }
}

float SaturationStage::processSampleRaw (float input) const
{
    // Step A: Asymmetric even-harmonic shaper (generates 2nd harmonic)
    //   y = x - k * x^2
    float y = input - asymmetry * input * input;

    // Step B: Odd-harmonic tanh saturation
    //   y = tanh(drive * x) / tanh(drive)
    // Normalised so that unity-gain input maps to unity-gain output at low levels.
    if (drive > 0.01f)
    {
        const float tanhDrive = std::tanh (drive);
        y = std::tanh (drive * y) / tanhDrive;
    }

    return y;
}

void SaturationStage::setPreEmphasisFreq (float hz)
{
    preEmphasisFreq = juce::jlimit (50.0f, 500.0f, hz);
    updateEmphasisFilters();
}

int SaturationStage::getLatencySamples() const
{
    if (oversampling && enabled)
        return static_cast<int> (oversampling->getLatencyInSamples());
    return 0;
}

void SaturationStage::updateEmphasisFilters()
{
    // Pre-emphasis: low-shelf boost at preEmphasisFreq, +6 dB gain
    // This makes low frequencies hit the nonlinearity harder.
    auto preCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        currentSR, preEmphasisFreq, 0.707f, 2.0f  // 2x = ~+6 dB boost
    );

    // De-emphasis: low-shelf cut — exact inverse of pre-emphasis
    auto deCoeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
        currentSR, preEmphasisFreq, 0.707f, 0.5f  // 0.5x = ~-6 dB cut
    );

    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        preEmphasis[ch].coefficients = preCoeffs;
        deEmphasis[ch].coefficients  = deCoeffs;
    }
}

} // namespace valvane
