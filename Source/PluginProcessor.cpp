#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PresetManager.h"
#include "DSP/CompressorEngine.h"
#include "DSP/OptoEngine.h"
#include "DSP/FETEngine.h"
#include "DSP/VariMuEngine.h"
#include "DSP/VCAEngine.h"
#include "DSP/GainComputer.h"

using namespace valvane;

// ═══════════════════════════════════════════════════════════════════════════
// Parameter layout — every automatable control in the plugin
// ═══════════════════════════════════════════════════════════════════════════
juce::AudioProcessorValueTreeState::ParameterLayout
ValvaneAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // ---- Global / shared ----
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::CHARACTER_MODE, 1 }, "Character",
        juce::StringArray { "Opto", "FET", "Vari-Mu", "VCA" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::BYPASS, 1 }, "Bypass", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::MIX, 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::DETECTOR_TYPE, 1 }, "Detector",
        juce::StringArray { "Peak", "RMS", "Blend" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::DETECTOR_BLEND, 1 }, "Det. Blend",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, "%"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::SC_HPF_FREQ, 1 }, "SC HPF",
        juce::NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.4f), 80.0f, "Hz"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::STEREO_LINK, 1 }, "Stereo Link", true));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::AUTO_MAKEUP, 1 }, "Auto Makeup", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::SOFT_CLIP, 1 }, "Soft Clip", false));

    // ---- Opto (LA-2A-style) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::OPTO_GAIN, 1 }, "Opto Gain",
        juce::NormalisableRange<float> (-12.0f, 36.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::OPTO_PEAK_REDUCTION, 1 }, "Peak Reduction",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::OPTO_COMP_LIMIT, 1 }, "Comp/Limit", false));

    // ---- FET (1176-style) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::FET_INPUT, 1 }, "FET Input",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::FET_OUTPUT, 1 }, "FET Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::FET_ATTACK, 1 }, "FET Attack",
        juce::NormalisableRange<float> (0.02f, 0.8f, 0.001f, 0.4f), 0.2f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::FET_RELEASE, 1 }, "FET Release",
        juce::NormalisableRange<float> (50.0f, 1200.0f, 1.0f, 0.4f), 300.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::FET_RATIO, 1 }, "FET Ratio",
        juce::StringArray { "4:1", "8:1", "12:1", "20:1" }, 0));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::FET_ALL_BUTTONS, 1 }, "All Buttons", false));

    // ---- Vari-Mu (Fairchild-style) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VARI_INPUT, 1 }, "Vari-Mu Input",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VARI_OUTPUT, 1 }, "Vari-Mu Output",
        juce::NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VARI_THRESHOLD, 1 }, "Vari-Mu Threshold",
        juce::NormalisableRange<float> (-40.0f, 0.0f, 0.1f), -18.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VARI_TIME_CONSTANT, 1 }, "Time Constant",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    // ---- VCA (SSL-bus-style) ----
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VCA_THRESHOLD, 1 }, "VCA Threshold",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -18.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VCA_RATIO, 1 }, "VCA Ratio",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f), 4.0f, ":1"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VCA_ATTACK, 1 }, "VCA Attack",
        juce::NormalisableRange<float> (0.01f, 100.0f, 0.01f, 0.3f), 10.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VCA_RELEASE, 1 }, "VCA Release",
        juce::NormalisableRange<float> (10.0f, 2000.0f, 1.0f, 0.4f), 100.0f, "ms"));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::VCA_AUTO_RELEASE, 1 }, "Auto Release", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VCA_KNEE, 1 }, "VCA Knee",
        juce::NormalisableRange<float> (0.0f, 24.0f, 0.1f), 6.0f, "dB"));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::VCA_MAKEUP, 1 }, "VCA Makeup",
        juce::NormalisableRange<float> (-12.0f, 36.0f, 0.1f), 0.0f, "dB"));

    return { params.begin(), params.end() };
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════
ValvaneAudioProcessor::ValvaneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",    juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",   juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "ValvaneParams", createParameterLayout())
{
    // Cache atomic pointers for real-time access
    pCharacterMode = apvts.getRawParameterValue (ParamIDs::CHARACTER_MODE);
    pBypass        = apvts.getRawParameterValue (ParamIDs::BYPASS);
    pMix           = apvts.getRawParameterValue (ParamIDs::MIX);
    pDetectorType  = apvts.getRawParameterValue (ParamIDs::DETECTOR_TYPE);
    pDetectorBlend = apvts.getRawParameterValue (ParamIDs::DETECTOR_BLEND);
    pScHpfFreq     = apvts.getRawParameterValue (ParamIDs::SC_HPF_FREQ);
    pStereoLink    = apvts.getRawParameterValue (ParamIDs::STEREO_LINK);
    pAutoMakeup    = apvts.getRawParameterValue (ParamIDs::AUTO_MAKEUP);
    pSoftClip      = apvts.getRawParameterValue (ParamIDs::SOFT_CLIP);

    createEngines();
    presetManager = std::make_unique<PresetManager> (apvts);
}

ValvaneAudioProcessor::~ValvaneAudioProcessor() = default;

// ═══════════════════════════════════════════════════════════════════════════
// Engine creation — one instance per character mode
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessor::createEngines()
{
    engines[0] = std::make_unique<OptoEngine> (
        apvts.getRawParameterValue (ParamIDs::OPTO_GAIN),
        apvts.getRawParameterValue (ParamIDs::OPTO_PEAK_REDUCTION),
        apvts.getRawParameterValue (ParamIDs::OPTO_COMP_LIMIT),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_TYPE),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_BLEND)
    );

    engines[1] = std::make_unique<FETEngine> (
        apvts.getRawParameterValue (ParamIDs::FET_INPUT),
        apvts.getRawParameterValue (ParamIDs::FET_OUTPUT),
        apvts.getRawParameterValue (ParamIDs::FET_ATTACK),
        apvts.getRawParameterValue (ParamIDs::FET_RELEASE),
        apvts.getRawParameterValue (ParamIDs::FET_RATIO),
        apvts.getRawParameterValue (ParamIDs::FET_ALL_BUTTONS),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_TYPE),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_BLEND)
    );

    engines[2] = std::make_unique<VariMuEngine> (
        apvts.getRawParameterValue (ParamIDs::VARI_INPUT),
        apvts.getRawParameterValue (ParamIDs::VARI_OUTPUT),
        apvts.getRawParameterValue (ParamIDs::VARI_THRESHOLD),
        apvts.getRawParameterValue (ParamIDs::VARI_TIME_CONSTANT),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_TYPE),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_BLEND)
    );

    engines[3] = std::make_unique<VCAEngine> (
        apvts.getRawParameterValue (ParamIDs::VCA_THRESHOLD),
        apvts.getRawParameterValue (ParamIDs::VCA_RATIO),
        apvts.getRawParameterValue (ParamIDs::VCA_ATTACK),
        apvts.getRawParameterValue (ParamIDs::VCA_RELEASE),
        apvts.getRawParameterValue (ParamIDs::VCA_AUTO_RELEASE),
        apvts.getRawParameterValue (ParamIDs::VCA_KNEE),
        apvts.getRawParameterValue (ParamIDs::VCA_MAKEUP),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_TYPE),
        apvts.getRawParameterValue (ParamIDs::DETECTOR_BLEND)
    );
}

// ═══════════════════════════════════════════════════════════════════════════
// Bus layout support — mono, stereo, and stereo+sidechain
// ═══════════════════════════════════════════════════════════════════════════
bool ValvaneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    // Main input/output must be mono or stereo, and must match
    if (mainIn != mainOut)
        return false;

    if (mainIn != juce::AudioChannelSet::mono() &&
        mainIn != juce::AudioChannelSet::stereo())
        return false;

    // Sidechain: either disabled or stereo
    const auto numSCBuses = layouts.inputBuses.size();
    if (numSCBuses > 1)
    {
        const auto& sc = layouts.inputBuses[1];
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::stereo() &&
            sc != juce::AudioChannelSet::mono())
            return false;
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// Prepare / Release
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Prepare all engines (they are always "warm" for instant switching)
    for (auto& engine : engines)
    {
        if (engine)
            engine->prepare (sampleRate, samplesPerBlock);
    }

    // Prepare sidechain HPF filters
    lastHpfFreq = pScHpfFreq->load();
    auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
        sampleRate, lastHpfFreq, 0.707f);
    scHpfL.coefficients = hpfCoeffs;
    scHpfR.coefficients = hpfCoeffs;
    scHpfL.reset();
    scHpfR.reset();

    // Prepare smoothed values
    smoothedMix.reset (sampleRate, 0.02);   // 20ms smoothing
    smoothedAutoMakeup.reset (sampleRate, 0.05);

    // Allocate internal buffers
    dryBuffer.setSize (2, samplesPerBlock);
    detectorBuffer.setSize (2, samplesPerBlock);

    // Report latency — sum of the active engine's saturation latency
    int latency = 0;
    const int mode = static_cast<int> (pCharacterMode->load());
    if (mode >= 0 && mode < 4 && engines[mode])
        latency = engines[mode]->getLatencySamples();
    setLatencySamples (latency);

    avgGainReduction = 0.0f;
}

void ValvaneAudioProcessor::releaseResources()
{
    for (auto& engine : engines)
        if (engine)
            engine->reset();
}

// ═══════════════════════════════════════════════════════════════════════════
// Process block — main audio processing loop
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Nothing to do
    if (numChannels == 0 || numSamples == 0)
        return;

    // ---- Read parameters ----
    const bool bypassed        = pBypass->load() >= 0.5f;
    const float mixPercent     = pMix->load();
    const int   modeIndex      = static_cast<int> (pCharacterMode->load());
    const float hpfFreq        = pScHpfFreq->load();
    const bool  stereoLink     = pStereoLink->load() >= 0.5f;
    const bool  autoMakeup     = pAutoMakeup->load() >= 0.5f;
    const bool  softClip       = pSoftClip->load() >= 0.5f;

    // ---- True bypass ----
    if (bypassed)
    {
        // Route audio straight through (true bypass)
        inputLevelDb.store (0.0f);
        outputLevelDb.store (0.0f);
        gainReductionDb.store (0.0f);
        return;
    }

    // ---- Measure input level ----
    float inPeak = 0.0f;
    for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
        inPeak = std::max (inPeak, buffer.getMagnitude (ch, 0, numSamples));
    inputLevelDb.store (GainComputer::linearToDb (inPeak));

    // ---- Save dry copy for mix ----
    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // ---- Prepare detector input ----
    detectorBuffer.setSize (std::min (numChannels, 2), numSamples, false, false, true);

    // Check if external sidechain bus is active
    const bool hasExternalSC = (getBusCount (true) > 1 &&
                                getBus (true, 1) != nullptr &&
                                getBus (true, 1)->isEnabled() &&
                                getTotalNumInputChannels() > numChannels);

    if (hasExternalSC)
    {
        // Read sidechain input from channels beyond the main bus
        const int mainChCount = getMainBusNumInputChannels();
        for (int ch = 0; ch < std::min (2, numChannels); ++ch)
        {
            const int scCh = mainChCount + ch;
            if (scCh < buffer.getNumChannels())
                detectorBuffer.copyFrom (ch, 0, buffer, scCh, 0, numSamples);
            else
                detectorBuffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
        }
    }
    else
    {
        // Internal sidechain: use main input
        for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
            detectorBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    }

    // ---- Sidechain HPF (applied only to detector, not audible output) ----
    if (std::abs (hpfFreq - lastHpfFreq) > 0.5f)
    {
        lastHpfFreq = hpfFreq;
        auto hpfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
            currentSampleRate, hpfFreq, 0.707f);
        scHpfL.coefficients = hpfCoeffs;
        scHpfR.coefficients = hpfCoeffs;
    }
    {
        auto* detL = detectorBuffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            detL[i] = scHpfL.processSample (detL[i]);

        if (detectorBuffer.getNumChannels() > 1)
        {
            auto* detR = detectorBuffer.getWritePointer (1);
            for (int i = 0; i < numSamples; ++i)
                detR[i] = scHpfR.processSample (detR[i]);
        }
    }

    // ---- Stereo link: combine L/R detector to single linked signal ----
    if (stereoLink && detectorBuffer.getNumChannels() > 1)
    {
        auto* detL = detectorBuffer.getWritePointer (0);
        auto* detR = detectorBuffer.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            // Use max(|L|, |R|) for stereo-linked detection
            const float linked = std::max (std::abs (detL[i]), std::abs (detR[i]));
            const float sign = (detL[i] >= 0.0f) ? 1.0f : -1.0f;
            detL[i] = linked * sign;
            detR[i] = linked * ((detR[i] >= 0.0f) ? 1.0f : -1.0f);
        }
    }

    // ---- Process through active engine ----
    if (modeIndex >= 0 && modeIndex < 4 && engines[modeIndex])
    {
        engines[modeIndex]->processBlock (buffer, detectorBuffer);

        // Read GR for metering
        const float gr = engines[modeIndex]->getGainReductionDb();
        gainReductionDb.store (gr);

        // Update running average GR for auto-makeup
        constexpr float grSmooth = 0.001f;
        avgGainReduction += (gr - avgGainReduction) * grSmooth;
    }

    // ---- Auto makeup gain ----
    if (autoMakeup)
    {
        // Compensate for average gain reduction
        const float makeupDb = std::max (avgGainReduction, 0.0f);
        const float makeupGain = GainComputer::dbToLinear (makeupDb);
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.applyGain (ch, 0, numSamples, makeupGain);
    }

    // ---- Soft-clip ceiling (smooth saturating limiter) ----
    if (softClip)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                // Smooth soft-clip using tanh with a gentle curve
                // Ceiling at approximately 0 dBFS
                data[i] = std::tanh (data[i]);
            }
        }
    }

    // ---- Mix (parallel compression) ----
    smoothedMix.setTargetValue (mixPercent * 0.01f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = smoothedMix.getNextValue();
        const float dry = 1.0f - wet;

        for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
        {
            auto* out    = buffer.getWritePointer (ch);
            const auto* dryData = dryBuffer.getReadPointer (ch);
            out[i] = out[i] * wet + dryData[i] * dry;
        }
    }

    // ---- Measure output level ----
    float outPeak = 0.0f;
    for (int ch = 0; ch < std::min (numChannels, 2); ++ch)
        outPeak = std::max (outPeak, buffer.getMagnitude (ch, 0, numSamples));
    outputLevelDb.store (GainComputer::linearToDb (outPeak));

    // ---- Update reported latency if engine changed ----
    {
        int latency = 0;
        if (modeIndex >= 0 && modeIndex < 4 && engines[modeIndex])
            latency = engines[modeIndex]->getLatencySamples();
        if (latency != getLatencySamples())
            setLatencySamples (latency);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// State save / restore
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save the full APVTS state + A/B state + current preset index
    auto state = apvts.copyState();
    auto xml = state.createXml();

    if (xml)
    {
        // Attach A/B state info
        xml->setAttribute ("currentAB", currentABState.load());
        xml->setAttribute ("currentPreset", presetManager ? presetManager->getCurrentPresetIndex() : -1);

        copyXmlToBinary (*xml, destData);
    }
}

void ValvaneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentABState.store (xml->getIntAttribute ("currentAB", 0));

            if (presetManager)
            {
                int presetIdx = xml->getIntAttribute ("currentPreset", -1);
                presetManager->setCurrentPresetIndex (presetIdx);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// A/B compare
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessor::toggleAB()
{
    const int current = currentABState.load();

    // Save current state to current slot
    stateSnapshots[current] = apvts.copyState();

    // Switch to the other slot
    const int other = 1 - current;
    currentABState.store (other);

    // Restore the other slot's state (if it has been saved before)
    if (stateSnapshots[other].isValid())
        apvts.replaceState (stateSnapshots[other]);
}

void ValvaneAudioProcessor::copyAToB()
{
    stateSnapshots[1] = apvts.copyState();
}

void ValvaneAudioProcessor::copyBToA()
{
    if (stateSnapshots[1].isValid())
        stateSnapshots[0] = stateSnapshots[1].createCopy();
}

// ═══════════════════════════════════════════════════════════════════════════
// Editor factory
// ═══════════════════════════════════════════════════════════════════════════
juce::AudioProcessorEditor* ValvaneAudioProcessor::createEditor()
{
    return new ValvaneAudioProcessorEditor (*this);
}

// ═══════════════════════════════════════════════════════════════════════════
// Plugin instantiation entry point
// ═══════════════════════════════════════════════════════════════════════════
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ValvaneAudioProcessor();
}
