#include "PresetManager.h"

// ═══════════════════════════════════════════════════════════════════════════
// PresetManager implementation
//
// Factory presets: 12+ covering all four Character modes.
// User presets: stored as XML in platform-specific app data directory.
// ═══════════════════════════════════════════════════════════════════════════

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState (apvts)
{
    loadFactoryPresets();
    scanUserPresets();
}

PresetManager::~PresetManager() = default;

// ---------------------------------------------------------------------------
// Factory presets — embedded in code, 3+ per Character mode
// ---------------------------------------------------------------------------
void PresetManager::loadFactoryPresets()
{
    presets.clear();

    // Helper: create a preset Xml from param-value pairs
    auto makePreset = [&] (const juce::String& name,
                           std::initializer_list<std::pair<juce::String, float>> params) -> Preset
    {
        Preset p;
        p.name = name;
        p.isFactory = true;

        // Build a minimal APVTS-compatible XML tree
        auto xml = std::make_unique<juce::XmlElement> ("ValvaneState");
        auto* paramsXml = xml->createNewChildElement ("PARAMS");

        for (auto& [id, val] : params)
        {
            auto* paramEl = paramsXml->createNewChildElement ("PARAM");
            paramEl->setAttribute ("id", id);
            paramEl->setAttribute ("value", static_cast<double> (val));
        }

        p.state = std::move (xml);
        return p;
    };

    // ── Opto presets ──────────────────────────────────────────────────────
    presets.push_back (makePreset ("Opto - Vocal Leveler",
    {
        {"characterMode", 0.0f}, {"optoGain", 6.0f}, {"optoPeakReduction", 45.0f},
        {"optoCompLimit", 0.0f}, {"mix", 100.0f}, {"scHPFFreq", 80.0f},
        {"stereoLink", 1.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 0.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("Opto - Bus Glue",
    {
        {"characterMode", 0.0f}, {"optoGain", 3.0f}, {"optoPeakReduction", 30.0f},
        {"optoCompLimit", 0.0f}, {"mix", 80.0f}, {"scHPFFreq", 100.0f},
        {"stereoLink", 1.0f}, {"detectorType", 2.0f}, {"detectorBlend", 50.0f},
        {"autoMakeup", 1.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("Opto - Limiting",
    {
        {"characterMode", 0.0f}, {"optoGain", 8.0f}, {"optoPeakReduction", 70.0f},
        {"optoCompLimit", 1.0f}, {"mix", 100.0f}, {"scHPFFreq", 60.0f},
        {"stereoLink", 1.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 0.0f}, {"softClip", 1.0f}, {"bypass", 0.0f}
    }));

    // ── FET presets ───────────────────────────────────────────────────────
    presets.push_back (makePreset ("FET - Vocal Punch",
    {
        {"characterMode", 1.0f}, {"fetInput", 6.0f}, {"fetOutput", -4.0f},
        {"fetAttack", 0.2f}, {"fetRelease", 300.0f}, {"fetRatio", 1.0f},
        {"fetAllButtons", 0.0f}, {"mix", 100.0f}, {"scHPFFreq", 120.0f},
        {"stereoLink", 0.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 0.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("FET - Drum Smash",
    {
        {"characterMode", 1.0f}, {"fetInput", 12.0f}, {"fetOutput", -8.0f},
        {"fetAttack", 0.05f}, {"fetRelease", 150.0f}, {"fetRatio", 3.0f},
        {"fetAllButtons", 0.0f}, {"mix", 70.0f}, {"scHPFFreq", 80.0f},
        {"stereoLink", 1.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 1.0f}, {"softClip", 1.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("FET - All Buttons Crush",
    {
        {"characterMode", 1.0f}, {"fetInput", 15.0f}, {"fetOutput", -12.0f},
        {"fetAttack", 0.1f}, {"fetRelease", 200.0f}, {"fetRatio", 0.0f},
        {"fetAllButtons", 1.0f}, {"mix", 60.0f}, {"scHPFFreq", 100.0f},
        {"stereoLink", 1.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 1.0f}, {"softClip", 1.0f}, {"bypass", 0.0f}
    }));

    // ── Vari-Mu presets ───────────────────────────────────────────────────
    presets.push_back (makePreset ("VariMu - Master Glue",
    {
        {"characterMode", 2.0f}, {"variInput", 4.0f}, {"variOutput", -2.0f},
        {"variThreshold", -18.0f}, {"variTimeConstant", 50.0f},
        {"mix", 100.0f}, {"scHPFFreq", 100.0f},
        {"stereoLink", 1.0f}, {"detectorType", 1.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 1.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("VariMu - Warm Vocals",
    {
        {"characterMode", 2.0f}, {"variInput", 6.0f}, {"variOutput", -3.0f},
        {"variThreshold", -15.0f}, {"variTimeConstant", 30.0f},
        {"mix", 100.0f}, {"scHPFFreq", 80.0f},
        {"stereoLink", 0.0f}, {"detectorType", 2.0f}, {"detectorBlend", 40.0f},
        {"autoMakeup", 0.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("VariMu - Slow Mastering",
    {
        {"characterMode", 2.0f}, {"variInput", 2.0f}, {"variOutput", 0.0f},
        {"variThreshold", -20.0f}, {"variTimeConstant", 75.0f},
        {"mix", 100.0f}, {"scHPFFreq", 120.0f},
        {"stereoLink", 1.0f}, {"detectorType", 1.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 1.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    // ── VCA presets ───────────────────────────────────────────────────────
    presets.push_back (makePreset ("VCA - Transparent Bus",
    {
        {"characterMode", 3.0f}, {"vcaThreshold", -18.0f}, {"vcaRatio", 2.0f},
        {"vcaAttack", 10.0f}, {"vcaRelease", 100.0f}, {"vcaAutoRelease", 1.0f},
        {"vcaKnee", 12.0f}, {"vcaMakeup", 4.0f},
        {"mix", 100.0f}, {"scHPFFreq", 100.0f},
        {"stereoLink", 1.0f}, {"detectorType", 1.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 0.0f}, {"softClip", 0.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("VCA - Aggressive Mix",
    {
        {"characterMode", 3.0f}, {"vcaThreshold", -24.0f}, {"vcaRatio", 6.0f},
        {"vcaAttack", 1.0f}, {"vcaRelease", 60.0f}, {"vcaAutoRelease", 0.0f},
        {"vcaKnee", 3.0f}, {"vcaMakeup", 8.0f},
        {"mix", 75.0f}, {"scHPFFreq", 80.0f},
        {"stereoLink", 1.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 0.0f}, {"softClip", 1.0f}, {"bypass", 0.0f}
    }));

    presets.push_back (makePreset ("VCA - Parallel Drums",
    {
        {"characterMode", 3.0f}, {"vcaThreshold", -30.0f}, {"vcaRatio", 10.0f},
        {"vcaAttack", 0.5f}, {"vcaRelease", 50.0f}, {"vcaAutoRelease", 0.0f},
        {"vcaKnee", 0.0f}, {"vcaMakeup", 12.0f},
        {"mix", 40.0f}, {"scHPFFreq", 60.0f},
        {"stereoLink", 1.0f}, {"detectorType", 0.0f}, {"detectorBlend", 0.0f},
        {"autoMakeup", 0.0f}, {"softClip", 1.0f}, {"bypass", 0.0f}
    }));
}

// ---------------------------------------------------------------------------
// User preset directory (platform-specific)
// ---------------------------------------------------------------------------
juce::File PresetManager::getUserPresetDirectory() const
{
    auto appData = juce::File::getSpecialLocation (
        juce::File::userApplicationDataDirectory);
    return appData.getChildFile ("AJaudio").getChildFile ("Valvane").getChildFile ("Presets");
}

void PresetManager::scanUserPresets()
{
    auto dir = getUserPresetDirectory();
    if (!dir.isDirectory())
        return;

    for (const auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
    {
        if (auto xml = juce::parseXML (file))
        {
            Preset p;
            p.name = file.getFileNameWithoutExtension();
            p.isFactory = false;
            p.state = std::move (xml);
            presets.push_back (std::move (p));
        }
    }
}

// ---------------------------------------------------------------------------
// Save / Load / Delete
// ---------------------------------------------------------------------------
void PresetManager::savePreset (const juce::String& name)
{
    auto dir = getUserPresetDirectory();
    dir.createDirectory();

    auto stateXml = valueTreeState.copyState().createXml();
    if (!stateXml)
        return;

    // Wrap it
    auto wrapper = std::make_unique<juce::XmlElement> ("ValvaneState");
    auto* paramsXml = wrapper->createNewChildElement ("PARAMS");

    // Copy all parameter values
    for (auto* param : valueTreeState.processor.getParameters())
    {
        if (auto* rpwl = dynamic_cast<juce::RangedAudioParameter*>(param))
        {
            auto* el = paramsXml->createNewChildElement ("PARAM");
            el->setAttribute ("id", rpwl->getParameterID());
            el->setAttribute ("value", static_cast<double>(rpwl->getValue()));
        }
    }

    auto file = dir.getChildFile (name + ".xml");
    wrapper->writeTo (file);

    // Add to list
    Preset p;
    p.name = name;
    p.isFactory = false;
    p.state = std::move (wrapper);
    presets.push_back (std::move (p));
    currentPreset = static_cast<int> (presets.size()) - 1;

    if (onPresetChanged)
        onPresetChanged();
}

void PresetManager::loadPreset (int index)
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    const auto& preset = presets[static_cast<size_t> (index)];
    if (!preset.state)
        return;

    // Restore parameters from the preset XML
    if (auto* paramsXml = preset.state->getChildByName ("PARAMS"))
    {
        for (auto* paramEl : paramsXml->getChildIterator())
        {
            auto id = paramEl->getStringAttribute ("id");
            auto val = static_cast<float> (paramEl->getDoubleAttribute ("value"));

            if (auto* param = valueTreeState.getParameter (id))
                param->setValueNotifyingHost (param->convertTo0to1 (val));
        }
    }

    currentPreset = index;

    if (onPresetChanged)
        onPresetChanged();
}

void PresetManager::loadPresetByName (const juce::String& name)
{
    for (int i = 0; i < static_cast<int> (presets.size()); ++i)
    {
        if (presets[static_cast<size_t> (i)].name == name)
        {
            loadPreset (i);
            return;
        }
    }
}

void PresetManager::deletePreset (int index)
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    auto& preset = presets[static_cast<size_t> (index)];
    if (preset.isFactory)
        return;  // Can't delete factory presets

    // Delete the file if it exists
    auto file = getUserPresetDirectory().getChildFile (preset.name + ".xml");
    if (file.existsAsFile())
        file.deleteFile();

    presets.erase (presets.begin() + index);

    if (currentPreset >= static_cast<int> (presets.size()))
        currentPreset = static_cast<int> (presets.size()) - 1;

    if (onPresetChanged)
        onPresetChanged();
}

int PresetManager::getNumPresets() const
{
    return static_cast<int> (presets.size());
}

juce::String PresetManager::getPresetName (int index) const
{
    if (index >= 0 && index < static_cast<int> (presets.size()))
        return presets[static_cast<size_t> (index)].name;
    return {};
}
