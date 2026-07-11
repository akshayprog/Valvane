#pragma once
#include <JuceHeader.h>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════
// Character mode enumeration — each mode selects a distinct detector /
// gain-reduction / saturation topology.
// ═══════════════════════════════════════════════════════════════════════════
enum class CharacterMode : int
{
    Opto    = 0,   // LA-2A-style electro-optical leveling amplifier
    FET     = 1,   // 1176-style field-effect-transistor limiter
    VariMu  = 2,   // Fairchild-style variable-mu tube compressor
    VCA     = 3,   // SSL-bus-style voltage-controlled amplifier
    NumModes = 4
};

// ═══════════════════════════════════════════════════════════════════════════
// Parameter ID constants — used with AudioProcessorValueTreeState.
// Every automatable control in the plugin maps to exactly one of these.
// ═══════════════════════════════════════════════════════════════════════════
namespace ParamIDs {

    // ---- Global / shared ----
    inline constexpr const char* CHARACTER_MODE   = "characterMode";
    inline constexpr const char* BYPASS           = "bypass";
    inline constexpr const char* MIX              = "mix";
    inline constexpr const char* DETECTOR_TYPE    = "detectorType";
    inline constexpr const char* DETECTOR_BLEND   = "detectorBlend";
    inline constexpr const char* SC_HPF_FREQ      = "scHPFFreq";
    inline constexpr const char* STEREO_LINK      = "stereoLink";
    inline constexpr const char* AUTO_MAKEUP      = "autoMakeup";
    inline constexpr const char* SOFT_CLIP        = "softClip";

    // ---- Opto (LA-2A-style) ----
    inline constexpr const char* OPTO_GAIN            = "optoGain";
    inline constexpr const char* OPTO_PEAK_REDUCTION  = "optoPeakReduction";
    inline constexpr const char* OPTO_COMP_LIMIT      = "optoCompLimit";

    // ---- FET (1176-style) ----
    inline constexpr const char* FET_INPUT        = "fetInput";
    inline constexpr const char* FET_OUTPUT       = "fetOutput";
    inline constexpr const char* FET_ATTACK       = "fetAttack";
    inline constexpr const char* FET_RELEASE      = "fetRelease";
    inline constexpr const char* FET_RATIO        = "fetRatio";
    inline constexpr const char* FET_ALL_BUTTONS  = "fetAllButtons";

    // ---- Vari-Mu (Fairchild-style) ----
    inline constexpr const char* VARI_INPUT          = "variInput";
    inline constexpr const char* VARI_OUTPUT         = "variOutput";
    inline constexpr const char* VARI_THRESHOLD      = "variThreshold";
    inline constexpr const char* VARI_TIME_CONSTANT  = "variTimeConstant";

    // ---- VCA (SSL-bus-style) ----
    inline constexpr const char* VCA_THRESHOLD     = "vcaThreshold";
    inline constexpr const char* VCA_RATIO         = "vcaRatio";
    inline constexpr const char* VCA_ATTACK        = "vcaAttack";
    inline constexpr const char* VCA_RELEASE       = "vcaRelease";
    inline constexpr const char* VCA_AUTO_RELEASE  = "vcaAutoRelease";
    inline constexpr const char* VCA_KNEE          = "vcaKnee";
    inline constexpr const char* VCA_MAKEUP        = "vcaMakeup";

} // namespace ParamIDs

// ═══════════════════════════════════════════════════════════════════════════
// CompressorEngine — abstract interface implemented by each character mode.
//
// The host PluginProcessor owns one instance of each engine and switches
// between them based on the CharacterMode parameter.  Engines are prepared
// with the *oversampled* sample rate (the processor handles up/down-sampling).
// ═══════════════════════════════════════════════════════════════════════════
class CompressorEngine
{
public:
    virtual ~CompressorEngine() = default;

    // Called once after construction and whenever the sample rate or block
    // size changes.  |sampleRate| is the oversampled rate.
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;

    // Clear all internal state (envelopes, filters, etc.).
    virtual void reset() = 0;

    // Process audio in-place.
    //   audioBuffer  — the main audio to compress (modified in-place)
    //   detectorInput — the signal used by the envelope detector.
    //                   May be the same as audioBuffer (internal SC) or
    //                   from the external sidechain bus (already HPF'd and
    //                   stereo-linked by the processor).
    virtual void processBlock (juce::AudioBuffer<float>& audioBuffer,
                               const juce::AudioBuffer<float>& detectorInput) = 0;

    // Returns the instantaneous gain reduction in dB (always >= 0).
    virtual float getGainReductionDb() const = 0;

    // Latency samples added by this engine (e.g. internal oversampling).
    virtual int getLatencySamples() const { return 0; }
};

} // namespace valvane
