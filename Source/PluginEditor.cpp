#include "PluginEditor.h"
#include "PresetManager.h"
#include "DSP/CompressorEngine.h"

using namespace valvane;

// ═══════════════════════════════════════════════════════════════════════════
// Helper: configure a rotary slider with label and tooltip
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessorEditor::setupSlider (juce::Slider& slider,
                                                juce::Label& label,
                                                const juce::String& labelText,
                                                const juce::String& tooltip)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.attachToComponent (&slider, false);
    addAndMakeVisible (label);
}

void ValvaneAudioProcessorEditor::setupToggle (juce::ToggleButton& btn,
                                                const juce::String& tooltip)
{
    btn.setTooltip (tooltip);
    addAndMakeVisible (btn);
}

// ═══════════════════════════════════════════════════════════════════════════
// Constructor
// ═══════════════════════════════════════════════════════════════════════════
ValvaneAudioProcessorEditor::ValvaneAudioProcessorEditor (ValvaneAudioProcessor& p)
    : AudioProcessorEditor (p),
      processorRef (p)
{
    setLookAndFeel (&customLnF);

    // ---- Resizable, DPI-aware ----
    setResizable (true, true);
    setResizeLimits (700, 500, 1400, 1000);
    setSize (900, 640);

    // ---- Title / branding ----
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colour (50, 50, 50));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setFont (juce::FontOptions (12.0f));
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (100, 100, 100));
    subtitleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (subtitleLabel);

    // ---- Character selector ----
    characterSelector.addItem ("Opto", 1);
    characterSelector.addItem ("FET", 2);
    characterSelector.addItem ("Vari-Mu", 3);
    characterSelector.addItem ("VCA", 4);
    characterSelector.setTooltip ("Select compressor character: Opto (warm, smooth LA-2A style), "
                                  "FET (fast, punchy 1176 style), Vari-Mu (lush, glue-y Fairchild style), "
                                  "VCA (clean, precise SSL style)");
    characterSelector.onChange = [this] { updateVisibility(); };
    addAndMakeVisible (characterSelector);

    // ---- Preset selector ----
    auto& pm = processorRef.getPresetManager();
    for (int i = 0; i < pm.getNumPresets(); ++i)
        presetSelector.addItem (pm.getPresetName (i), i + 1);
    presetSelector.setTooltip ("Browse and load presets");
    presetSelector.onChange = [this] {
        int idx = presetSelector.getSelectedItemIndex();
        if (idx >= 0)
            processorRef.getPresetManager().loadPreset (idx);
    };
    addAndMakeVisible (presetSelector);

    // ---- VU Meter ----
    addAndMakeVisible (vuMeter);

    // ---- Shared controls ----
    setupSlider (mixSlider, mixLabel, "Mix",
                 "Dry/Wet mix for parallel (New York style) compression. "
                 "100% = fully compressed, lower values blend dry signal.");
    setupSlider (scHpfSlider, scHpfLabel, "SC HPF",
                 "Sidechain high-pass filter frequency. Prevents bass from "
                 "triggering excessive compression (pumping).");
    setupSlider (detectorBlendSlider, detBlendLabel, "Det. Blend",
                 "Blend between Peak and RMS detection when Detector is set to Blend mode.");

    detectorTypeSelector.addItem ("Peak", 1);
    detectorTypeSelector.addItem ("RMS", 2);
    detectorTypeSelector.addItem ("Blend", 3);
    detectorTypeSelector.setTooltip ("Detector type: Peak (fastest response), "
                                     "RMS (average-sensing, smoother), "
                                     "Blend (mix both).");
    addAndMakeVisible (detectorTypeSelector);
    detTypeLabel.setText ("Detector", juce::dontSendNotification);
    detTypeLabel.setJustificationType (juce::Justification::centred);
    detTypeLabel.attachToComponent (&detectorTypeSelector, false);
    addAndMakeVisible (detTypeLabel);

    setupToggle (bypassButton, "True bypass — routes audio around all processing.");
    setupToggle (stereoLinkButton, "Stereo link — detector uses max(L,R) instead of per-channel.");
    setupToggle (autoMakeupButton, "Auto makeup gain — compensates for average gain reduction.");
    setupToggle (softClipButton, "Soft-clip output ceiling — prevents digital clipping on aggressive settings.");

    // ---- A/B compare ----
    abButton.setTooltip ("Toggle between A and B parameter snapshots for quick comparison.");
    abButton.onClick = [this] {
        processorRef.toggleAB();
        abButton.setButtonText (processorRef.isStateA() ? "A" : "B");
    };
    addAndMakeVisible (abButton);

    copyABButton.setTooltip ("Copy current settings from A to B.");
    copyABButton.onClick = [this] { processorRef.copyAToB(); };
    addAndMakeVisible (copyABButton);

    // ── Opto controls ────────────────────────────────────────────────────
    setupSlider (optoGainSlider, optoGainLabel, "Gain",
                 "Makeup gain (dB). Boosts the output level after compression.");
    setupSlider (optoPeakReductionSlider, optoPRLabel, "Peak Reduction",
                 "Controls how much compression is applied. Higher values = more "
                 "gain reduction. This is the only 'threshold' control — matching "
                 "real opto leveling amplifier hardware.");
    setupToggle (optoCompLimitButton,
                 "Toggle between Compress (~3:1 soft ratio) and Limit (aggressive high ratio). "
                 "Changes the drive-to-gain-reduction mapping, not just the ratio number.");

    optoComponents = { &optoGainSlider, &optoGainLabel,
                       &optoPeakReductionSlider, &optoPRLabel,
                       &optoCompLimitButton };

    // ── FET controls ─────────────────────────────────────────────────────
    setupSlider (fetInputSlider, fetInLabel, "Input",
                 "Input drive (dB). Pushes signal into the compressor — higher values "
                 "= more compression and harmonic saturation.");
    setupSlider (fetOutputSlider, fetOutLabel, "Output",
                 "Output level (dB). Adjust to compensate for gain changes.");
    setupSlider (fetAttackSlider, fetAtkLabel, "Attack",
                 "Attack time (ms). Sub-millisecond range for fast transient control. "
                 "Lower = faster clamping of peaks.");
    setupSlider (fetReleaseSlider, fetRelLabel, "Release",
                 "Release time (ms). How quickly gain recovers after compression. "
                 "The FET release subtly speeds up as GR decreases.");

    fetRatioSelector.addItem ("4:1", 1);
    fetRatioSelector.addItem ("8:1", 2);
    fetRatioSelector.addItem ("12:1", 3);
    fetRatioSelector.addItem ("20:1", 4);
    fetRatioSelector.setTooltip ("Compression ratio. Higher ratios = more aggressive limiting.");
    addAndMakeVisible (fetRatioSelector);
    fetRatioLabel.setText ("Ratio", juce::dontSendNotification);
    fetRatioLabel.setJustificationType (juce::Justification::centred);
    fetRatioLabel.attachToComponent (&fetRatioSelector, false);
    addAndMakeVisible (fetRatioLabel);

    setupToggle (fetAllButtonsToggle,
                 "All-Buttons mode: Engages all ratio buttons simultaneously for extreme, "
                 "distorted compression with erratic gain reduction — a classic 1176 trick.");

    fetComponents = { &fetInputSlider, &fetInLabel,
                      &fetOutputSlider, &fetOutLabel,
                      &fetAttackSlider, &fetAtkLabel,
                      &fetReleaseSlider, &fetRelLabel,
                      &fetRatioSelector, &fetRatioLabel,
                      &fetAllButtonsToggle };

    // ── Vari-Mu controls ─────────────────────────────────────────────────
    setupSlider (variInputSlider, variInLabel, "Input",
                 "Input drive (dB). Drives signal into the tube stage for more "
                 "compression and harmonic richness.");
    setupSlider (variOutputSlider, variOutLabel, "Output",
                 "Output level (dB). Compensate for gain changes.");
    setupSlider (variThresholdSlider, variThreshLabel, "Threshold",
                 "Compression threshold (dB). Signal above this level gets compressed.");
    setupSlider (variTimeConstantSlider, variTCLabel, "Time Constant",
                 "Blended attack/release macro — like the real Fairchild's multi-position "
                 "switch. Lower = faster response, higher = slower and more gentle.");

    variMuComponents = { &variInputSlider, &variInLabel,
                         &variOutputSlider, &variOutLabel,
                         &variThresholdSlider, &variThreshLabel,
                         &variTimeConstantSlider, &variTCLabel };

    // ── VCA controls ─────────────────────────────────────────────────────
    setupSlider (vcaThresholdSlider, vcaThreshLabel, "Threshold",
                 "Compression threshold (dB). Signal above this level is compressed.");
    setupSlider (vcaRatioSlider, vcaRatioLabel, "Ratio",
                 "Compression ratio (1:1 to 20:1). Higher = more aggressive compression.");
    setupSlider (vcaAttackSlider, vcaAtkLabel, "Attack",
                 "Attack time (ms). How fast the compressor responds to signals "
                 "exceeding the threshold.");
    setupSlider (vcaReleaseSlider, vcaRelLabel, "Release",
                 "Release time (ms). How quickly the compressor stops compressing "
                 "after the signal drops below threshold.");
    setupSlider (vcaKneeSlider, vcaKneeLabel, "Knee",
                 "Knee width (dB). 0 = hard knee (abrupt compression onset), "
                 "higher = soft knee (gradual onset, more transparent).");
    setupSlider (vcaMakeupSlider, vcaMakeupLabel, "Makeup",
                 "Makeup gain (dB). Boosts output to compensate for gain reduction.");
    setupToggle (vcaAutoReleaseButton,
                 "Auto Release: Uses program-dependent release timing — fast for "
                 "transients, slow for sustained material.");

    vcaComponents = { &vcaThresholdSlider, &vcaThreshLabel,
                      &vcaRatioSlider, &vcaRatioLabel,
                      &vcaAttackSlider, &vcaAtkLabel,
                      &vcaReleaseSlider, &vcaRelLabel,
                      &vcaKneeSlider, &vcaKneeLabel,
                      &vcaMakeupSlider, &vcaMakeupLabel,
                      &vcaAutoReleaseButton };

    // ── APVTS attachments ────────────────────────────────────────────────
    auto& apvts = processorRef.getAPVTS();

    characterAttach  = std::make_unique<ComboBoxAttachment> (apvts, ParamIDs::CHARACTER_MODE, characterSelector);
    mixAttach        = std::make_unique<SliderAttachment>   (apvts, ParamIDs::MIX, mixSlider);
    scHpfAttach      = std::make_unique<SliderAttachment>   (apvts, ParamIDs::SC_HPF_FREQ, scHpfSlider);
    detBlendAttach   = std::make_unique<SliderAttachment>   (apvts, ParamIDs::DETECTOR_BLEND, detectorBlendSlider);
    detTypeAttach    = std::make_unique<ComboBoxAttachment> (apvts, ParamIDs::DETECTOR_TYPE, detectorTypeSelector);
    bypassAttach     = std::make_unique<ButtonAttachment>   (apvts, ParamIDs::BYPASS, bypassButton);
    stereoLinkAttach = std::make_unique<ButtonAttachment>   (apvts, ParamIDs::STEREO_LINK, stereoLinkButton);
    autoMakeupAttach = std::make_unique<ButtonAttachment>   (apvts, ParamIDs::AUTO_MAKEUP, autoMakeupButton);
    softClipAttach   = std::make_unique<ButtonAttachment>   (apvts, ParamIDs::SOFT_CLIP, softClipButton);

    optoGainAttach      = std::make_unique<SliderAttachment> (apvts, ParamIDs::OPTO_GAIN, optoGainSlider);
    optoPRAttach        = std::make_unique<SliderAttachment> (apvts, ParamIDs::OPTO_PEAK_REDUCTION, optoPeakReductionSlider);
    optoCompLimitAttach = std::make_unique<ButtonAttachment> (apvts, ParamIDs::OPTO_COMP_LIMIT, optoCompLimitButton);

    fetInputAttach      = std::make_unique<SliderAttachment>   (apvts, ParamIDs::FET_INPUT, fetInputSlider);
    fetOutputAttach     = std::make_unique<SliderAttachment>   (apvts, ParamIDs::FET_OUTPUT, fetOutputSlider);
    fetAttackAttach     = std::make_unique<SliderAttachment>   (apvts, ParamIDs::FET_ATTACK, fetAttackSlider);
    fetReleaseAttach    = std::make_unique<SliderAttachment>   (apvts, ParamIDs::FET_RELEASE, fetReleaseSlider);
    fetRatioAttach      = std::make_unique<ComboBoxAttachment> (apvts, ParamIDs::FET_RATIO, fetRatioSelector);
    fetAllButtonsAttach = std::make_unique<ButtonAttachment>   (apvts, ParamIDs::FET_ALL_BUTTONS, fetAllButtonsToggle);

    variInputAttach  = std::make_unique<SliderAttachment> (apvts, ParamIDs::VARI_INPUT, variInputSlider);
    variOutputAttach = std::make_unique<SliderAttachment> (apvts, ParamIDs::VARI_OUTPUT, variOutputSlider);
    variThreshAttach = std::make_unique<SliderAttachment> (apvts, ParamIDs::VARI_THRESHOLD, variThresholdSlider);
    variTCAttach     = std::make_unique<SliderAttachment> (apvts, ParamIDs::VARI_TIME_CONSTANT, variTimeConstantSlider);

    vcaThreshAttach  = std::make_unique<SliderAttachment> (apvts, ParamIDs::VCA_THRESHOLD, vcaThresholdSlider);
    vcaRatioAttach   = std::make_unique<SliderAttachment> (apvts, ParamIDs::VCA_RATIO, vcaRatioSlider);
    vcaAtkAttach     = std::make_unique<SliderAttachment> (apvts, ParamIDs::VCA_ATTACK, vcaAttackSlider);
    vcaRelAttach     = std::make_unique<SliderAttachment> (apvts, ParamIDs::VCA_RELEASE, vcaReleaseSlider);
    vcaAutoRelAttach = std::make_unique<ButtonAttachment> (apvts, ParamIDs::VCA_AUTO_RELEASE, vcaAutoReleaseButton);
    vcaKneeAttach    = std::make_unique<SliderAttachment> (apvts, ParamIDs::VCA_KNEE, vcaKneeSlider);
    vcaMakeupAttach  = std::make_unique<SliderAttachment> (apvts, ParamIDs::VCA_MAKEUP, vcaMakeupSlider);

    // ---- Initial visibility ----
    updateVisibility();

    // ---- Start timer for meter/UI updates (30 fps) ----
    startTimerHz (30);
}

ValvaneAudioProcessorEditor::~ValvaneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    stopTimer();
}

// ═══════════════════════════════════════════════════════════════════════════
// Paint — brushed metal background, branding, decorative elements
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Brushed metal background
    CustomLookAndFeel::paintBrushedMetalBackground (g, getLocalBounds());

    // Top bar background
    auto topBar = getLocalBounds().removeFromTop (60);
    g.setColour (juce::Colour (170, 170, 165).withAlpha (0.4f));
    g.fillRect (topBar);
    g.setColour (juce::Colour (140, 140, 135));
    g.drawLine (0.0f, 60.0f, static_cast<float> (getWidth()), 60.0f, 1.0f);

    // Bottom bar background
    auto bottomBar = getLocalBounds().removeFromBottom (60);
    g.setColour (juce::Colour (170, 170, 165).withAlpha (0.3f));
    g.fillRect (bottomBar);
    g.setColour (juce::Colour (140, 140, 135));
    g.drawLine (0.0f, static_cast<float> (getHeight() - 60),
                static_cast<float> (getWidth()), static_cast<float> (getHeight() - 60), 1.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Layout — dynamically positions all controls based on current size
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth();

    // ── Top bar: branding + selectors ────────────────────────────────────
    auto topBar = bounds.removeFromTop (60);
    titleLabel.setBounds (topBar.removeFromLeft (160).reduced (10, 8));
    subtitleLabel.setBounds (10, 32, 150, 20);

    auto selectorArea = topBar.removeFromRight (w / 2);
    presetSelector.setBounds (selectorArea.removeFromRight (200).reduced (8, 16));
    characterSelector.setBounds (selectorArea.removeFromRight (140).reduced (8, 16));

    // ── Bottom bar: toggles, bypass, A/B ────────────────────────────────
    auto bottomBar = bounds.removeFromBottom (60);
    const int toggleW = 100;
    bypassButton.setBounds      (bottomBar.removeFromLeft (toggleW).reduced (6, 10));
    stereoLinkButton.setBounds  (bottomBar.removeFromLeft (toggleW).reduced (6, 10));
    autoMakeupButton.setBounds  (bottomBar.removeFromLeft (toggleW).reduced (6, 10));
    softClipButton.setBounds    (bottomBar.removeFromLeft (toggleW).reduced (6, 10));

    auto abArea = bottomBar.removeFromRight (160);
    abButton.setBounds     (abArea.removeFromLeft (60).reduced (6, 12));
    copyABButton.setBounds (abArea.removeFromLeft (80).reduced (6, 12));

    // ── Main area: meter center, controls on sides ───────────────────────
    auto mainArea = bounds.reduced (10, 5);

    // Shared controls on the right side
    auto sharedArea = mainArea.removeFromRight (100);
    const int knobH = 80;
    const int labelOffset = 18;

    mixSlider.setBounds          (sharedArea.removeFromTop (knobH + labelOffset).reduced (5, 0).withTrimmedTop (labelOffset));
    scHpfSlider.setBounds        (sharedArea.removeFromTop (knobH + labelOffset).reduced (5, 0).withTrimmedTop (labelOffset));
    detectorBlendSlider.setBounds(sharedArea.removeFromTop (knobH + labelOffset).reduced (5, 0).withTrimmedTop (labelOffset));
    detectorTypeSelector.setBounds(sharedArea.removeFromTop (40 + labelOffset).reduced (5, 0).withTrimmedTop (labelOffset));

    // VU meter in the center
    const int meterW = std::min (320, mainArea.getWidth() / 2);
    auto meterArea = mainArea.removeFromTop (mainArea.getHeight());
    auto meterRect = meterArea.withSizeKeepingCentre (meterW, std::min (280, meterArea.getHeight()));

    // Mode-specific controls on the left side
    auto controlArea = mainArea.withWidth (mainArea.getWidth());

    // Position the VU meter
    vuMeter.setBounds (meterRect);

    // ── Mode-specific control layout ─────────────────────────────────────
    // These are positioned relative to the meter, flanking it on the left
    const int leftW = meterRect.getX() - bounds.getX() - 10;
    const int rightW = (bounds.getX() + bounds.getWidth()) - meterRect.getRight() - 120;
    auto leftArea = juce::Rectangle<int> (bounds.getX() + 10, meterRect.getY(),
                                           std::max (leftW, 200), meterRect.getHeight());
    auto rightKnobArea = juce::Rectangle<int> (meterRect.getRight() + 10, meterRect.getY(),
                                                std::max (rightW, 100), meterRect.getHeight());

    const int kS = 80; // knob size
    const int kGap = 10;

    // Opto mode layout: 2 knobs + 1 toggle
    {
        auto area = leftArea;
        optoGainSlider.setBounds (area.removeFromTop (kS + labelOffset).reduced (10, 0).withTrimmedTop (labelOffset));
        optoPeakReductionSlider.setBounds (area.removeFromTop (kS + labelOffset).reduced (10, 0).withTrimmedTop (labelOffset));
        optoCompLimitButton.setBounds (area.removeFromTop (40).reduced (10, 5));
    }

    // FET mode layout: 4 knobs + ratio selector + toggle
    {
        auto area = leftArea;
        int halfW = area.getWidth() / 2;
        auto topRow = area.removeFromTop (kS + labelOffset);
        fetInputSlider.setBounds (topRow.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        fetOutputSlider.setBounds (topRow.reduced (5, 0).withTrimmedTop (labelOffset));

        auto midRow = area.removeFromTop (kS + labelOffset);
        fetAttackSlider.setBounds (midRow.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        fetReleaseSlider.setBounds (midRow.reduced (5, 0).withTrimmedTop (labelOffset));

        auto botRow = area.removeFromTop (50);
        fetRatioSelector.setBounds (botRow.removeFromLeft (halfW).reduced (5, 5).withTrimmedTop (labelOffset));
        fetAllButtonsToggle.setBounds (botRow.reduced (5, 10));
    }

    // Vari-Mu mode layout: 4 knobs in 2x2
    {
        auto area = leftArea;
        int halfW = area.getWidth() / 2;
        auto topRow = area.removeFromTop (kS + labelOffset);
        variInputSlider.setBounds (topRow.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        variOutputSlider.setBounds (topRow.reduced (5, 0).withTrimmedTop (labelOffset));

        auto midRow = area.removeFromTop (kS + labelOffset);
        variThresholdSlider.setBounds (midRow.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        variTimeConstantSlider.setBounds (midRow.reduced (5, 0).withTrimmedTop (labelOffset));
    }

    // VCA mode layout: 6 knobs in 3x2 + toggle
    {
        auto area = leftArea;
        int halfW = area.getWidth() / 2;
        auto row1 = area.removeFromTop (kS + labelOffset);
        vcaThresholdSlider.setBounds (row1.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        vcaRatioSlider.setBounds (row1.reduced (5, 0).withTrimmedTop (labelOffset));

        auto row2 = area.removeFromTop (kS + labelOffset);
        vcaAttackSlider.setBounds (row2.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        vcaReleaseSlider.setBounds (row2.reduced (5, 0).withTrimmedTop (labelOffset));

        auto row3 = area.removeFromTop (kS + labelOffset);
        vcaKneeSlider.setBounds (row3.removeFromLeft (halfW).reduced (5, 0).withTrimmedTop (labelOffset));
        vcaMakeupSlider.setBounds (row3.reduced (5, 0).withTrimmedTop (labelOffset));

        vcaAutoReleaseButton.setBounds (area.removeFromTop (40).reduced (5, 5));
    }

    updateVisibility();
}

// ═══════════════════════════════════════════════════════════════════════════
// Timer callback — update meters and check mode changes
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessorEditor::timerCallback()
{
    // Update VU meter with current values from processor
    vuMeter.setGainReductionDb (processorRef.getGainReductionDb());
    vuMeter.setInputLevel (processorRef.getInputLevelDb());
    vuMeter.setOutputLevel (processorRef.getOutputLevelDb());

    // Check if mode changed
    int mode = static_cast<int> (processorRef.getAPVTS()
                   .getRawParameterValue (ParamIDs::CHARACTER_MODE)->load());
    if (mode != lastMode)
        updateVisibility();
}

// ═══════════════════════════════════════════════════════════════════════════
// Dynamic visibility — show/hide controls based on Character mode
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessorEditor::updateVisibility()
{
    int mode = static_cast<int> (processorRef.getAPVTS()
                   .getRawParameterValue (ParamIDs::CHARACTER_MODE)->load());
    lastMode = mode;

    auto setVisible = [] (std::vector<juce::Component*>& comps, bool visible) {
        for (auto* c : comps)
            if (c) c->setVisible (visible);
    };

    setVisible (optoComponents,   mode == 0);
    setVisible (fetComponents,    mode == 1);
    setVisible (variMuComponents, mode == 2);
    setVisible (vcaComponents,    mode == 3);

    repaint();
}
