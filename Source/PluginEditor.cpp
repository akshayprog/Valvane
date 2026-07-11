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
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 15);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
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
    // 1. Paint main brushed metal faceplate
    CustomLookAndFeel::paintBrushedMetalBackground (g, getLocalBounds());

    // Get active character mode
    int mode = static_cast<int> (processorRef.getAPVTS()
                   .getRawParameterValue (ParamIDs::CHARACTER_MODE)->load());

    // 2. Paint vintage branding elements based on mode
    if (mode == 0) // Opto (LA-2A style)
    {
        const float logoX = 45.0f;
        const float logoY = 110.0f;

        // Vintage red logo box
        g.setColour (juce::Colour (180, 30, 30));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 23.0f, juce::Font::bold)));
        g.drawSingleLineText ("VALVANE", logoX, logoY);

        // Slanted underline matching the Teletronix branding
        g.drawLine (logoX, logoY + 4.0f, logoX + 115.0f, logoY + 4.0f, 1.8f);
        g.drawLine (logoX, logoY + 7.0f, logoX + 115.0f, logoY + 7.0f, 0.8f);

        // Subtitle labels
        g.setColour (juce::Colour (50, 50, 50));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.5f, juce::Font::bold)));
        g.drawSingleLineText ("LEVELING AMPLIFIER", logoX + 2.0f, logoY + 20.0f);
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.0f, juce::Font::plain)));
        g.drawSingleLineText ("MODEL LA-2A STYLE", logoX + 2.0f, logoY + 31.0f);

        // Switch labels
        g.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        g.drawSingleLineText ("LIMIT", 48.0f, 222.0f);
        g.drawSingleLineText ("COMPRESS", 35.0f, 310.0f);

        g.drawSingleLineText ("ON", getWidth() - 71.0f, 222.0f);
        g.drawSingleLineText ("POWER", getWidth() - 82.0f, 310.0f);

        // Draw flat-head calibration screw next to toggle switch
        const float screwX = 98.0f;
        const float screwY = 265.0f;
        
        juce::ColourGradient screwGrad (juce::Colour (215, 215, 218), screwX - 5.0f, screwY - 5.0f,
                                        juce::Colour (135, 135, 138), screwX + 5.0f, screwY + 5.0f, true);
        g.setGradientFill (screwGrad);
        g.fillEllipse (screwX - 6.5f, screwY - 6.5f, 13.0f, 13.0f);
        g.setColour (juce::Colour (90, 90, 92));
        g.drawEllipse (screwX - 6.5f, screwY - 6.5f, 13.0f, 13.0f, 1.0f);
        g.setColour (juce::Colour (40, 40, 42));
        g.drawLine (screwX - 4.5f, screwY - 2.5f, screwX + 4.5f, screwY + 2.5f, 1.5f);
    }
    else if (mode == 1) // FET (1176 style)
    {
        const float logoX = 45.0f;
        const float logoY = 110.0f;

        g.setColour (juce::Colour (30, 30, 32));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 23.0f, juce::Font::bold)));
        g.drawSingleLineText ("VALVANE", logoX, logoY);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.5f, juce::Font::bold)));
        g.drawSingleLineText ("LIMITING AMPLIFIER", logoX + 2.0f, logoY + 20.0f);
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.0f, juce::Font::plain)));
        g.drawSingleLineText ("MODEL 1176 LN STYLE", logoX + 2.0f, logoY + 31.0f);

        // Switch labels
        g.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        g.drawSingleLineText ("ALL BTNS", 38.0f, 222.0f);
        g.drawSingleLineText ("NORMAL", 41.0f, 310.0f);

        g.drawSingleLineText ("ON", getWidth() - 71.0f, 222.0f);
        g.drawSingleLineText ("POWER", getWidth() - 82.0f, 310.0f);
    }
    else if (mode == 2) // Vari-Mu (Fairchild style)
    {
        const float logoX = 45.0f;
        const float logoY = 110.0f;

        g.setColour (juce::Colour (40, 35, 30));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 22.0f, juce::Font::bold)));
        g.drawSingleLineText ("VALVANE TUBE", logoX, logoY);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.5f, juce::Font::bold)));
        g.drawSingleLineText ("STEREO LEVELING AMPLIFIER", logoX + 2.0f, logoY + 20.0f);
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.0f, juce::Font::plain)));
        g.drawSingleLineText ("MODEL 670 STYLE", logoX + 2.0f, logoY + 31.0f);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        g.drawSingleLineText ("ON", getWidth() - 71.0f, 222.0f);
        g.drawSingleLineText ("POWER", getWidth() - 82.0f, 310.0f);
    }
    else if (mode == 3) // VCA (SSL style)
    {
        const float logoX = 45.0f;
        const float logoY = 110.0f;

        g.setColour (juce::Colour (45, 50, 55));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", 23.0f, juce::Font::bold)));
        g.drawSingleLineText ("VALVANE VCA", logoX, logoY);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.5f, juce::Font::bold)));
        g.drawSingleLineText ("STEREO BUS COMPRESSOR", logoX + 2.0f, logoY + 20.0f);
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.0f, juce::Font::plain)));
        g.drawSingleLineText ("CLASSIC CONSOLE CONCEPTS", logoX + 2.0f, logoY + 31.0f);

        g.setFont (juce::Font (juce::FontOptions ("Arial", 10.0f, juce::Font::bold)));
        g.drawSingleLineText ("AUTO", getWidth() - 81.0f, 132.0f);
        g.drawSingleLineText ("RELEASE", getWidth() - 92.0f, 218.0f);

        g.drawSingleLineText ("ON", getWidth() - 71.0f, 222.0f);
        g.drawSingleLineText ("POWER", getWidth() - 82.0f, 310.0f);
    }

    // 3. Paint top bar border
    auto topBar = getLocalBounds().removeFromTop (60);
    g.setColour (juce::Colour (170, 170, 165).withAlpha (0.4f));
    g.fillRect (topBar);
    g.setColour (juce::Colour (140, 140, 135));
    g.drawLine (0.0f, 60.0f, static_cast<float> (getWidth()), 60.0f, 1.0f);

    // 4. Paint bottom utility strip (Dark anodized aluminum look)
    auto bottomBar = getLocalBounds().removeFromBottom (100);
    g.setColour (juce::Colour (35, 35, 38)); // Black/dark grey
    g.fillRect (bottomBar);

    // Horizontal brush texture for anodized look
    {
        juce::Random rng (2026);
        for (int y = bottomBar.getY(); y < bottomBar.getBottom(); ++y)
        {
            float alpha = rng.nextFloat() * 0.05f;
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());
        }
    }

    // Border line separating main panel and utility strip
    g.setColour (juce::Colour (15, 15, 17));
    g.drawLine (0.0f, (float) bottomBar.getY(), static_cast<float> (getWidth()), (float) bottomBar.getY(), 1.5f);
    g.setColour (juce::Colour (255, 255, 255).withAlpha (0.06f));
    g.drawLine (0.0f, (float) bottomBar.getY() + 1.0f, static_cast<float> (getWidth()), (float) bottomBar.getY() + 1.0f, 0.8f);

    // Utility panel labels group text
    g.setColour (juce::Colour (180, 180, 182));
    g.setFont (juce::Font (juce::FontOptions ("Arial", 8.0f, juce::Font::bold)));
    g.drawText ("UTILITY CONTROLS", 12, bottomBar.getY() + 4, 150, 12, juce::Justification::left);

    // Draw rack screws in the four corners of the bottom strip
    auto drawScrew = [&g] (float x, float y) {
        juce::ColourGradient screwGrad (juce::Colour (140, 140, 142), x - 2.0f, y - 2.0f,
                                        juce::Colour (60, 60, 62), x + 2.0f, y + 2.0f, true);
        g.setGradientFill (screwGrad);
        g.fillEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colour (25, 25, 27));
        g.drawEllipse (x - 5.0f, y - 5.0f, 10.0f, 10.0f, 0.8f);
        g.drawLine (x - 3.0f, y - 1.0f, x + 3.0f, y + 1.0f, 1.0f); // slot
    };

    drawScrew (8.0f, bottomBar.getY() + 8.0f);
    drawScrew ((float) getWidth() - 8.0f, bottomBar.getY() + 8.0f);
    drawScrew (8.0f, (float) getWidth() - 8.0f); // bottom left (height-8)
    drawScrew ((float) getWidth() - 8.0f, (float) getHeight() - 8.0f);
}

// ═══════════════════════════════════════════════════════════════════════════
// Layout — dynamically positions all controls based on current size
// ═══════════════════════════════════════════════════════════════════════════
void ValvaneAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const int w = bounds.getWidth();
    const int h = bounds.getHeight();

    // ── Top bar: branding + selectors ────────────────────────────────────
    auto topBar = bounds.removeFromTop (60);
    titleLabel.setBounds (topBar.removeFromLeft (160).reduced (10, 8));
    subtitleLabel.setBounds (10, 32, 150, 20);

    auto selectorArea = topBar.removeFromRight (w / 2);
    presetSelector.setBounds (selectorArea.removeFromRight (200).reduced (8, 16));
    characterSelector.setBounds (selectorArea.removeFromRight (140).reduced (8, 16));

    // ── Bottom bar: utility panel (100px high) ───────────────────────────
    auto bottomBar = bounds.removeFromBottom (100);

    // Lay out bottom utility controls in a neat row
    const float sectionW = (float) w / 8.5f;
    const int utilityY = bottomBar.getY() + 15;
    const int knobSize = 54;
    const int labelOffsetY = 56;

    // Mix (rotary)
    mixSlider.setBounds (juce::roundToInt (sectionW * 0.3f), utilityY, knobSize, knobSize);
    mixLabel.setBounds (mixSlider.getX() - 10, mixSlider.getY() + labelOffsetY, knobSize + 20, 16);

    // SC HPF (rotary)
    scHpfSlider.setBounds (juce::roundToInt (sectionW * 1.3f), utilityY, knobSize, knobSize);
    scHpfLabel.setBounds (scHpfSlider.getX() - 10, scHpfSlider.getY() + labelOffsetY, knobSize + 20, 16);

    // Det. Blend (rotary)
    detectorBlendSlider.setBounds (juce::roundToInt (sectionW * 2.3f), utilityY, knobSize, knobSize);
    detBlendLabel.setBounds (detectorBlendSlider.getX() - 10, detectorBlendSlider.getY() + labelOffsetY, knobSize + 20, 16);

    // Detector Type (ComboBox)
    detectorTypeSelector.setBounds (juce::roundToInt (sectionW * 3.4f), utilityY + 12, juce::roundToInt (sectionW * 0.9f), 22);
    detTypeLabel.setBounds (detectorTypeSelector.getX(), detectorTypeSelector.getY() - 18, detectorTypeSelector.getWidth(), 16);

    // Toggle Buttons (Stereo Link, Auto Makeup, Soft Clip)
    const int toggleGroupX = juce::roundToInt (sectionW * 4.5f);
    const int toggleW = 75;
    stereoLinkButton.setBounds (toggleGroupX, utilityY - 5, toggleW, 70);
    autoMakeupButton.setBounds (toggleGroupX + toggleW, utilityY - 5, toggleW, 70);
    softClipButton.setBounds   (toggleGroupX + toggleW * 2, utilityY - 5, toggleW, 70);

    // A/B Compare and Copy buttons on the far right
    abButton.setBounds (w - 150, utilityY + 15, 60, 26);
    copyABButton.setBounds (w - 80, utilityY + 15, 70, 26);

    // ── Main Area: Centered VU Meter & Flanking Controls ────────────────
    const int meterW = 340;
    const int meterH = 220;
    const int meterX = (w - meterW) / 2;
    const int meterY = 60 + (bounds.getHeight() - meterH) / 2;

    vuMeter.setBounds (meterX, meterY, meterW, meterH);

    // Left/Right control column definitions
    const int leftX = 0;
    const int leftW = meterX;
    const int rightX = meterX + meterW;
    const int rightW = w - rightX;

    auto leftArea  = juce::Rectangle<int> (leftX, 60, leftW, bounds.getHeight());
    auto rightArea = juce::Rectangle<int> (rightX, 60, rightW, bounds.getHeight());

    // Helper lambda to position a slider and its label below it
    auto positionSlider = [] (juce::Slider& s, juce::Label& l, int cx, int cy, int size)
    {
        s.setBounds (cx - size / 2, cy - (size + 15) / 2, size, size + 15);
        l.setBounds (cx - size / 2 - 10, s.getBottom() + 4, size + 20, 16);
    };

    int mode = static_cast<int> (processorRef.getAPVTS()
                   .getRawParameterValue (ParamIDs::CHARACTER_MODE)->load());

    // ── Mode-specific layout rules ───────────────────────────────────────
    if (mode == 0) // Opto Mode (LA-2A replication)
    {
        // GAIN knob on the left
        positionSlider (optoGainSlider, optoGainLabel, leftArea.getCentreX() + 30, leftArea.getCentreY(), 110);
        
        // PEAK REDUCTION knob on the right
        positionSlider (optoPeakReductionSlider, optoPRLabel, rightArea.getCentreX() - 30, rightArea.getCentreY(), 110);
        
        // Limit/Compress toggle on the far left
        optoCompLimitButton.setBounds (60 - 35, leftArea.getCentreY() + 40, 70, 60);
        
        // Power/Bypass toggle on the far right (behaves as main bypass)
        bypassButton.setBounds (w - 75, rightArea.getCentreY() + 40, 60, 60);
    }
    else if (mode == 1) // FET Mode (1176 style)
    {
        // Large INPUT on the left
        positionSlider (fetInputSlider, fetInLabel, leftArea.getX() + 85, leftArea.getCentreY(), 90);
        
        // Large OUTPUT on the right
        positionSlider (fetOutputSlider, fetOutLabel, rightArea.getRight() - 85, rightArea.getCentreY(), 90);
        
        // Smaller ATTACK and RELEASE in the middle columns
        positionSlider (fetAttackSlider, fetAtkLabel, leftArea.getRight() - 55, leftArea.getCentreY() - 30, 64);
        positionSlider (fetReleaseSlider, fetRelLabel, rightArea.getX() + 55, rightArea.getCentreY() - 30, 64);
        
        // Ratio selector combobox on the far right
        fetRatioSelector.setBounds (w - 95, rightArea.getCentreY() - 65, 75, 22);
        fetRatioLabel.setBounds (w - 95, rightArea.getCentreY() - 83, 75, 16);
        
        // All Buttons toggle on the far left
        fetAllButtonsToggle.setBounds (60 - 35, leftArea.getCentreY() + 40, 70, 60);
        
        // Power/Bypass toggle on the far right
        bypassButton.setBounds (w - 75, rightArea.getCentreY() + 40, 60, 60);
    }
    else if (mode == 2) // Vari-Mu Mode (Fairchild style)
    {
        // Large INPUT on the left
        positionSlider (variInputSlider, variInLabel, leftArea.getX() + 85, leftArea.getCentreY(), 90);
        
        // Large OUTPUT on the right
        positionSlider (variOutputSlider, variOutLabel, rightArea.getRight() - 85, rightArea.getCentreY(), 90);
        
        // Smaller THRESHOLD and TIME CONSTANT
        positionSlider (variThresholdSlider, variThreshLabel, leftArea.getRight() - 55, leftArea.getCentreY() - 30, 64);
        positionSlider (variTimeConstantSlider, variTCLabel, rightArea.getX() + 55, rightArea.getCentreY() - 30, 64);
        
        // Power/Bypass toggle on the far right
        bypassButton.setBounds (w - 75, rightArea.getCentreY() + 40, 60, 60);
    }
    else if (mode == 3) // VCA Mode (SSL console style)
    {
        // Left side of meter: THRESHOLD, RATIO, ATTACK (triangular cluster)
        positionSlider (vcaThresholdSlider, vcaThreshLabel, leftArea.getX() + 80, leftArea.getCentreY() - 75, 70);
        positionSlider (vcaRatioSlider, vcaRatioLabel, leftArea.getRight() - 60, leftArea.getCentreY(), 70);
        positionSlider (vcaAttackSlider, vcaAtkLabel, leftArea.getX() + 80, leftArea.getCentreY() + 75, 70);
        
        // Right side of meter: RELEASE, KNEE, MAKEUP (triangular cluster)
        positionSlider (vcaReleaseSlider, vcaRelLabel, rightArea.getRight() - 80, rightArea.getCentreY() - 75, 70);
        positionSlider (vcaKneeSlider, vcaKneeLabel, rightArea.getX() + 60, rightArea.getCentreY(), 70);
        positionSlider (vcaMakeupSlider, vcaMakeupLabel, rightArea.getRight() - 80, rightArea.getCentreY() + 75, 70);
        
        // Far Right: Auto Release switch & Bypass switch
        vcaAutoReleaseButton.setBounds (w - 85, rightArea.getCentreY() - 55, 70, 60);
        bypassButton.setBounds (w - 75, rightArea.getCentreY() + 40, 60, 60);
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
