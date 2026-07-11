#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/CustomLookAndFeel.h"
#include "UI/VUMeterComponent.h"

// ═══════════════════════════════════════════════════════════════════════════
// ValvaneAudioProcessorEditor
//
// Vintage-hardware-inspired resizable, DPI-scalable UI.
//
// Layout:
//   Top bar:    VALVANE branding + Character mode selector + Preset browser
//   Center:     Large VU-style GR needle meter flanked by knobs
//   Bottom:     Toggle switches, bypass, A/B compare
//
// Dynamic visibility: changing Character mode shows/hides the controls
// relevant to that mode only.
// ═══════════════════════════════════════════════════════════════════════════
class ValvaneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit ValvaneAudioProcessorEditor (ValvaneAudioProcessor& processor);
    ~ValvaneAudioProcessorEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateVisibility();

    ValvaneAudioProcessor& processorRef;
    valvane::CustomLookAndFeel customLnF;

    // ---- Top bar controls ----
    juce::Label titleLabel { "title", "VALVANE" };
    juce::Label subtitleLabel { "subtitle", "by AJaudio" };
    juce::ComboBox characterSelector;
    juce::ComboBox presetSelector;

    // ---- VU Meter ----
    valvane::VUMeterComponent vuMeter;

    // ---- Shared controls ----
    juce::Slider mixSlider;
    juce::Slider scHpfSlider;
    juce::Slider detectorBlendSlider;
    juce::ComboBox detectorTypeSelector;

    juce::ToggleButton bypassButton     { "Bypass" };
    juce::ToggleButton stereoLinkButton { "Stereo Link" };
    juce::ToggleButton autoMakeupButton { "Auto Makeup" };
    juce::ToggleButton softClipButton   { "Soft Clip" };

    // A/B compare
    juce::TextButton abButton { "A" };
    juce::TextButton copyABButton { "A\xe2\x86\x92\x42" };

    // ---- Opto controls ----
    juce::Slider optoGainSlider;
    juce::Slider optoPeakReductionSlider;
    juce::ToggleButton optoCompLimitButton { "Limit" };

    // ---- FET controls ----
    juce::Slider fetInputSlider;
    juce::Slider fetOutputSlider;
    juce::Slider fetAttackSlider;
    juce::Slider fetReleaseSlider;
    juce::ComboBox fetRatioSelector;
    juce::ToggleButton fetAllButtonsToggle { "All Buttons" };

    // ---- Vari-Mu controls ----
    juce::Slider variInputSlider;
    juce::Slider variOutputSlider;
    juce::Slider variThresholdSlider;
    juce::Slider variTimeConstantSlider;

    // ---- VCA controls ----
    juce::Slider vcaThresholdSlider;
    juce::Slider vcaRatioSlider;
    juce::Slider vcaAttackSlider;
    juce::Slider vcaReleaseSlider;
    juce::Slider vcaKneeSlider;
    juce::Slider vcaMakeupSlider;
    juce::ToggleButton vcaAutoReleaseButton { "Auto Rel." };

    // ---- Labels ----
    juce::Label mixLabel, scHpfLabel, detBlendLabel, detTypeLabel;
    juce::Label optoGainLabel, optoPRLabel;
    juce::Label fetInLabel, fetOutLabel, fetAtkLabel, fetRelLabel, fetRatioLabel;
    juce::Label variInLabel, variOutLabel, variThreshLabel, variTCLabel;
    juce::Label vcaThreshLabel, vcaRatioLabel, vcaAtkLabel, vcaRelLabel, vcaKneeLabel, vcaMakeupLabel;

    // ---- APVTS attachments ----
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ComboBoxAttachment>  characterAttach;
    std::unique_ptr<SliderAttachment>    mixAttach;
    std::unique_ptr<SliderAttachment>    scHpfAttach;
    std::unique_ptr<SliderAttachment>    detBlendAttach;
    std::unique_ptr<ComboBoxAttachment>  detTypeAttach;
    std::unique_ptr<ButtonAttachment>    bypassAttach;
    std::unique_ptr<ButtonAttachment>    stereoLinkAttach;
    std::unique_ptr<ButtonAttachment>    autoMakeupAttach;
    std::unique_ptr<ButtonAttachment>    softClipAttach;

    // Opto
    std::unique_ptr<SliderAttachment>    optoGainAttach;
    std::unique_ptr<SliderAttachment>    optoPRAttach;
    std::unique_ptr<ButtonAttachment>    optoCompLimitAttach;

    // FET
    std::unique_ptr<SliderAttachment>    fetInputAttach;
    std::unique_ptr<SliderAttachment>    fetOutputAttach;
    std::unique_ptr<SliderAttachment>    fetAttackAttach;
    std::unique_ptr<SliderAttachment>    fetReleaseAttach;
    std::unique_ptr<ComboBoxAttachment>  fetRatioAttach;
    std::unique_ptr<ButtonAttachment>    fetAllButtonsAttach;

    // Vari-Mu
    std::unique_ptr<SliderAttachment>    variInputAttach;
    std::unique_ptr<SliderAttachment>    variOutputAttach;
    std::unique_ptr<SliderAttachment>    variThreshAttach;
    std::unique_ptr<SliderAttachment>    variTCAttach;

    // VCA
    std::unique_ptr<SliderAttachment>    vcaThreshAttach;
    std::unique_ptr<SliderAttachment>    vcaRatioAttach;
    std::unique_ptr<SliderAttachment>    vcaAtkAttach;
    std::unique_ptr<SliderAttachment>    vcaRelAttach;
    std::unique_ptr<ButtonAttachment>    vcaAutoRelAttach;
    std::unique_ptr<SliderAttachment>    vcaKneeAttach;
    std::unique_ptr<SliderAttachment>    vcaMakeupAttach;

    // ---- Tooltip window ----
    juce::TooltipWindow tooltipWindow { this, 400 };

    // All mode-specific components grouped for visibility toggling
    std::vector<juce::Component*> optoComponents;
    std::vector<juce::Component*> fetComponents;
    std::vector<juce::Component*> variMuComponents;
    std::vector<juce::Component*> vcaComponents;

    int lastMode = -1;

    void setupSlider (juce::Slider& slider, juce::Label& label,
                      const juce::String& labelText, const juce::String& tooltip);
    void setupToggle (juce::ToggleButton& btn, const juce::String& tooltip);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ValvaneAudioProcessorEditor)
};
