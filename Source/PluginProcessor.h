#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>

namespace valvane {
    class CompressorEngine;
}

class PresetManager;

// ═══════════════════════════════════════════════════════════════════════════
// ValvaneAudioProcessor
//
// Main plugin processor for the Valvane multi-character compressor.
// Manages:
//   - Four CompressorEngine instances (Opto, FET, VariMu, VCA)
//   - Parameter tree (AudioProcessorValueTreeState) with all automatable params
//   - Sidechain routing, HPF, stereo linking
//   - Oversampling (via SaturationStage inside each engine)
//   - Mix (dry/wet parallel compression)
//   - Bypass, A/B compare, preset management
//   - Metering (input, output, gain reduction)
//   - State save/restore
// ═══════════════════════════════════════════════════════════════════════════
class ValvaneAudioProcessor : public juce::AudioProcessor
{
public:
    ValvaneAudioProcessor();
    ~ValvaneAudioProcessor() override;

    // ---- AudioProcessor overrides ----
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override            { return "Valvane"; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- Public API for editor ----
    juce::AudioProcessorValueTreeState& getAPVTS()     { return apvts; }

    float getInputLevelDb()  const  { return inputLevelDb.load(); }
    float getOutputLevelDb() const  { return outputLevelDb.load(); }
    float getGainReductionDb() const { return gainReductionDb.load(); }

    // A/B compare
    void toggleAB();
    void copyAToB();
    void copyBToA();
    bool isStateA() const { return currentABState.load() == 0; }

    // Preset management
    PresetManager& getPresetManager() { return *presetManager; }

private:
    // ---- Parameter tree ----
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- Engines ----
    std::array<std::unique_ptr<valvane::CompressorEngine>, 4> engines;
    void createEngines();

    // ---- Atomic parameter pointers (for fast processBlock access) ----
    std::atomic<float>* pCharacterMode   = nullptr;
    std::atomic<float>* pBypass          = nullptr;
    std::atomic<float>* pMix             = nullptr;
    std::atomic<float>* pDetectorType    = nullptr;
    std::atomic<float>* pDetectorBlend   = nullptr;
    std::atomic<float>* pScHpfFreq       = nullptr;
    std::atomic<float>* pStereoLink      = nullptr;
    std::atomic<float>* pAutoMakeup      = nullptr;
    std::atomic<float>* pSoftClip        = nullptr;

    // ---- Sidechain HPF ----
    juce::dsp::IIR::Filter<float> scHpfL, scHpfR;
    float lastHpfFreq = 0.0f;

    // ---- Smoothed values ----
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMix;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedAutoMakeup;

    // ---- Metering (thread-safe) ----
    std::atomic<float> inputLevelDb  { -100.0f };
    std::atomic<float> outputLevelDb { -100.0f };
    std::atomic<float> gainReductionDb { 0.0f };

    // ---- Running average GR for auto-makeup ----
    float avgGainReduction = 0.0f;

    // ---- A/B state ----
    std::atomic<int> currentABState { 0 };  // 0 = A, 1 = B
    juce::ValueTree stateSnapshots[2];

    // ---- Preset manager ----
    std::unique_ptr<PresetManager> presetManager;

    // ---- Internal buffers ----
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> detectorBuffer;

    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValvaneAudioProcessor)
};
