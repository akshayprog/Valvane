#pragma once
#include <JuceHeader.h>
#include <vector>
#include <functional>

// ═══════════════════════════════════════════════════════════════════════════
// PresetManager
//
// Handles save / load / browse of user and factory presets for the Valvane
// compressor plugin.  Factory presets are built-in (embedded in code);
// user presets are stored as XML files in a platform-appropriate directory.
//
// Factory presets: at least 3 per Character mode (12+ total) covering
// typical use cases (vocal, bus glue, mastering, aggressive).
// ═══════════════════════════════════════════════════════════════════════════
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& apvts);
    ~PresetManager();

    // ---- Preset I/O ----
    void savePreset (const juce::String& name);
    void loadPreset (int index);
    void loadPresetByName (const juce::String& name);
    void deletePreset (int index);

    // ---- Browsing ----
    int getNumPresets() const;
    juce::String getPresetName (int index) const;
    int getCurrentPresetIndex() const   { return currentPreset; }
    void setCurrentPresetIndex (int i)  { currentPreset = i; }

    // ---- Factory preset initialisation ----
    void loadFactoryPresets();

    // ---- Listener for preset changes ----
    std::function<void()> onPresetChanged;

    struct Preset
    {
        juce::String name;
        bool isFactory = false;
        std::unique_ptr<juce::XmlElement> state;  // full APVTS state
    };

    const std::vector<Preset>& getPresets() const { return presets; }

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    std::vector<Preset> presets;
    int currentPreset = -1;

    juce::File getUserPresetDirectory() const;
    void scanUserPresets();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
