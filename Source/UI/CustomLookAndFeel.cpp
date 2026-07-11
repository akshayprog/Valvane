#include "CustomLookAndFeel.h"

namespace valvane
{

//==============================================================================
// Construction / Destruction
//==============================================================================

CustomLookAndFeel::CustomLookAndFeel()
{
    // Use a clean, legible sans-serif font as default.
    defaultFont = juce::Font (juce::FontOptions ("Arial", 14.0f, juce::Font::plain));

    // Global colour overrides
    setColour (juce::Slider::textBoxTextColourId,      kTextLabel);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               kTextLabel);
    setColour (juce::ComboBox::backgroundColourId,      kPanelBase.darker (0.05f));
    setColour (juce::ComboBox::textColourId,            kTextLabel);
    setColour (juce::ComboBox::outlineColourId,         kTextLabel.withAlpha (0.3f));
    setColour (juce::PopupMenu::backgroundColourId,     kPanelBase.brighter (0.05f));
    setColour (juce::PopupMenu::textColourId,           kTextLabel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccentAmber.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId,       kTextLabel);

    setDefaultSansSerifTypeface (defaultFont.getTypefacePtr());
}

CustomLookAndFeel::~CustomLookAndFeel() = default;

//==============================================================================
// Brushed-metal panel background
//==============================================================================

void CustomLookAndFeel::paintBrushedMetalBackground (juce::Graphics& g,
                                                     juce::Rectangle<int> bounds)
{
    // Base fill — warm light gray
    g.setColour (kPanelBase);
    g.fillRect (bounds);

    // Subtle top-to-bottom gradient for gentle curvature illusion
    {
        juce::ColourGradient grad (kPanelBase.brighter (0.06f),
                                   (float) bounds.getX(), (float) bounds.getY(),
                                   kPanelBase.darker  (0.06f),
                                   (float) bounds.getX(), (float) bounds.getBottom(),
                                   false);
        g.setGradientFill (grad);
        g.fillRect (bounds);
    }

    // Horizontal brush-stroke lines
    {
        juce::Random rng (42);   // deterministic seed for consistent texture
        const int top    = bounds.getY();
        const int bottom = bounds.getBottom();
        const int left   = bounds.getX();
        const int right  = bounds.getRight();

        for (int y = top; y < bottom; y += 1)
        {
            float alpha = rng.nextFloat() * 0.045f;  // very subtle
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawHorizontalLine (y, (float) left, (float) right);
        }

        // A second pass with slightly darker strokes for depth
        rng.setSeed (97);
        for (int y = top; y < bottom; y += 2)
        {
            float alpha = rng.nextFloat() * 0.03f;
            g.setColour (juce::Colours::black.withAlpha (alpha));
            g.drawHorizontalLine (y, (float) left, (float) right);
        }
    }

    // Speckle noise for grit
    {
        juce::Random rng2 (137);
        const int numSpeckles = (bounds.getWidth() * bounds.getHeight()) / 40;
        for (int i = 0; i < numSpeckles; ++i)
        {
            float sx = (float) bounds.getX() + rng2.nextFloat() * (float) bounds.getWidth();
            float sy = (float) bounds.getY() + rng2.nextFloat() * (float) bounds.getHeight();
            float alpha = rng2.nextFloat() * 0.04f;
            g.setColour ((rng2.nextBool() ? juce::Colours::white : juce::Colours::black)
                             .withAlpha (alpha));
            g.fillRect (sx, sy, 1.0f, 1.0f);
        }
    }

    // Very subtle inner shadow at top and bottom edges
    {
        juce::ColourGradient topShadow (juce::Colours::black.withAlpha (0.07f),
                                        (float) bounds.getX(), (float) bounds.getY(),
                                        juce::Colours::transparentBlack,
                                        (float) bounds.getX(), (float) bounds.getY() + 6.0f,
                                        false);
        g.setGradientFill (topShadow);
        g.fillRect (bounds.getX(), bounds.getY(), bounds.getWidth(), 6);

        juce::ColourGradient btmShadow (juce::Colours::transparentBlack,
                                        (float) bounds.getX(), (float) bounds.getBottom() - 6.0f,
                                        juce::Colours::black.withAlpha (0.07f),
                                        (float) bounds.getX(), (float) bounds.getBottom(),
                                        false);
        g.setGradientFill (btmShadow);
        g.fillRect (bounds.getX(), bounds.getBottom() - 6, bounds.getWidth(), 6);
    }
}

//==============================================================================
// Rotary Slider — knurled knob
//==============================================================================

void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                          int x, int y, int width, int height,
                                          float sliderPos,
                                          float rotaryStartAngle,
                                          float rotaryEndAngle,
                                          juce::Slider& slider)
{
    juce::ignoreUnused (slider);

    const float margin   = juce::jmin (width, height) * 0.12f;
    const auto  area     = juce::Rectangle<float> ((float) x, (float) y,
                                                   (float) width, (float) height)
                               .reduced (margin);
    const float diameter = juce::jmin (area.getWidth(), area.getHeight());
    const float radius   = diameter * 0.5f;
    const float centreX  = area.getCentreX();
    const float centreY  = area.getCentreY();
    const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float outerRadius = radius;
    const float knobRadius  = radius * 0.78f;

    //--- Tick marks and dB labels around the arc ---
    {
        const float tickInner = outerRadius + 1.0f;
        const float tickOuter = outerRadius + 6.0f;
        const int   numTicks  = 11;

        g.setColour (kTextLabel.withAlpha (0.55f));
        for (int i = 0; i <= numTicks; ++i)
        {
            float t     = (float) i / (float) numTicks;
            float tAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);

            float cosA = std::cos (tAngle);
            float sinA = std::sin (tAngle);

            float x1 = centreX + tickInner * sinA;
            float y1 = centreY - tickInner * cosA;
            float x2 = centreX + tickOuter * sinA;
            float y2 = centreY - tickOuter * cosA;

            float thickness = (i % 5 == 0) ? 1.5f : 0.8f;
            g.drawLine (x1, y1, x2, y2, thickness);
        }
    }

    //--- Drop shadow under the knob ---
    {
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.30f),
                                     centreX, centreY,
                                     juce::Colours::transparentBlack,
                                     centreX, centreY + knobRadius * 1.15f,
                                     true);
        g.setGradientFill (shadow);
        g.fillEllipse (centreX - knobRadius * 1.05f,
                       centreY - knobRadius * 1.05f + 2.0f,
                       knobRadius * 2.1f,
                       knobRadius * 2.1f);
    }

    //--- Outer knurled ring ---
    {
        // Ring body
        juce::ColourGradient ringGrad (kKnobBodyTop.brighter (0.15f),
                                       centreX, centreY - outerRadius,
                                       kKnobBodyBottom,
                                       centreX, centreY + outerRadius,
                                       false);
        g.setGradientFill (ringGrad);
        g.fillEllipse (centreX - outerRadius, centreY - outerRadius,
                       outerRadius * 2.0f, outerRadius * 2.0f);

        // Knurled ridges around the circumference
        const int numRidges = juce::jmax (24, (int) (outerRadius * 1.5f));
        const float ridgeInner = outerRadius * 0.88f;

        for (int i = 0; i < numRidges; ++i)
        {
            float rAngle = juce::MathConstants<float>::twoPi * (float) i / (float) numRidges;
            float cosR = std::cos (rAngle);
            float sinR = std::sin (rAngle);

            float x1 = centreX + ridgeInner * cosR;
            float y1 = centreY + ridgeInner * sinR;
            float x2 = centreX + outerRadius * cosR;
            float y2 = centreY + outerRadius * sinR;

            // Alternate brightness for 3D ridge effect
            float alpha = (i % 2 == 0) ? 0.35f : 0.15f;
            g.setColour (juce::Colours::white.withAlpha (alpha));
            g.drawLine (x1, y1, x2, y2, 1.0f);
        }
    }

    //--- Inner knob face (recessed look) ---
    {
        juce::ColourGradient innerGrad (kKnobBodyTop,
                                        centreX, centreY - knobRadius,
                                        kKnobBodyBottom.darker (0.15f),
                                        centreX, centreY + knobRadius,
                                        false);
        g.setGradientFill (innerGrad);
        g.fillEllipse (centreX - knobRadius, centreY - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f);

        // Subtle highlight arc on top-left
        juce::ColourGradient highlight (juce::Colours::white.withAlpha (0.12f),
                                        centreX - knobRadius * 0.4f,
                                        centreY - knobRadius * 0.5f,
                                        juce::Colours::transparentBlack,
                                        centreX + knobRadius * 0.3f,
                                        centreY + knobRadius * 0.3f,
                                        true);
        g.setGradientFill (highlight);
        g.fillEllipse (centreX - knobRadius, centreY - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f);

        // Thin bright rim
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (centreX - knobRadius, centreY - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);
    }

    //--- Pointer / indicator line ---
    {
        const float pointerLength = knobRadius * 0.75f;
        const float pointerWidth  = juce::jmax (2.0f, knobRadius * 0.08f);

        juce::Path pointerPath;
        pointerPath.addRoundedRectangle (-pointerWidth * 0.5f, -pointerLength,
                                         pointerWidth, pointerLength,
                                         pointerWidth * 0.35f);

        g.setColour (kKnobPointer);
        g.fillPath (pointerPath, juce::AffineTransform::rotation (angle)
                                      .translated (centreX, centreY));

        // Small dot at the tip for visibility
        float tipX = centreX + std::sin (angle) * (knobRadius * 0.68f);
        float tipY = centreY - std::cos (angle) * (knobRadius * 0.68f);
        g.setColour (kKnobPointer);
        g.fillEllipse (tipX - 2.5f, tipY - 2.5f, 5.0f, 5.0f);
    }

    //--- Centre cap ---
    {
        const float capRadius = knobRadius * 0.18f;
        juce::ColourGradient capGrad (kKnobBodyTop.brighter (0.25f),
                                      centreX, centreY - capRadius,
                                      kKnobBodyBottom,
                                      centreX, centreY + capRadius,
                                      false);
        g.setGradientFill (capGrad);
        g.fillEllipse (centreX - capRadius, centreY - capRadius,
                       capRadius * 2.0f, capRadius * 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawEllipse (centreX - capRadius, centreY - capRadius,
                       capRadius * 2.0f, capRadius * 2.0f, 0.8f);
    }
}

//==============================================================================
// Toggle Button — 3-D physical switch
//==============================================================================

void CustomLookAndFeel::drawToggleButton (juce::Graphics& g,
                                          juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (4.0f);
    const bool isOn   = button.getToggleState();

    // Switch track dimensions
    const float trackW = juce::jmin (bounds.getWidth() * 0.55f, 44.0f);
    const float trackH = trackW * 0.50f;
    const auto  trackArea = juce::Rectangle<float> (0, 0, trackW, trackH)
                                .withCentre ({ bounds.getCentreX(),
                                               bounds.getCentreY() - bounds.getHeight() * 0.05f });

    // Track shadow
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRoundedRectangle (trackArea.translated (0.0f, 2.0f), trackH * 0.35f);

    // Track body
    {
        juce::Colour trackCol = isOn ? kAccentAmber.darker (0.15f) : juce::Colour (90, 90, 95);
        juce::ColourGradient trackGrad (trackCol.brighter (0.15f),
                                        trackArea.getX(), trackArea.getY(),
                                        trackCol.darker  (0.2f),
                                        trackArea.getX(), trackArea.getBottom(),
                                        false);
        g.setGradientFill (trackGrad);
        g.fillRoundedRectangle (trackArea, trackH * 0.35f);

        // Inner recessed groove
        g.setColour (juce::Colours::black.withAlpha (0.20f));
        auto groove = trackArea.reduced (2.5f);
        g.fillRoundedRectangle (groove, groove.getHeight() * 0.35f);
    }

    // Thumb / switch knob
    {
        const float thumbDiam = trackH * 0.80f;
        const float thumbR    = thumbDiam * 0.5f;
        const float travel    = trackW - thumbDiam - 6.0f;

        float thumbX = trackArea.getX() + 3.0f + (isOn ? travel : 0.0f);
        float thumbY = trackArea.getCentreY() - thumbR;

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillEllipse (thumbX + 1.0f, thumbY + 2.0f, thumbDiam, thumbDiam);

        // Thumb gradient
        juce::ColourGradient tGrad (juce::Colour (220, 220, 220),
                                     thumbX + thumbR, thumbY,
                                     juce::Colour (170, 170, 170),
                                     thumbX + thumbR, thumbY + thumbDiam,
                                     false);
        g.setGradientFill (tGrad);
        g.fillEllipse (thumbX, thumbY, thumbDiam, thumbDiam);

        // Highlight rim
        g.setColour (juce::Colours::white.withAlpha (shouldDrawButtonAsHighlighted ? 0.25f : 0.10f));
        g.drawEllipse (thumbX, thumbY, thumbDiam, thumbDiam, 1.0f);
    }

    // Label text below the switch
    {
        g.setColour (kTextLabel);
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmin (13.0f, bounds.getHeight() * 0.18f), juce::Font::plain)));

        auto textArea = bounds;
        textArea.setTop (trackArea.getBottom() + 4.0f);
        g.drawFittedText (button.getButtonText(), textArea.toNearestInt(),
                          juce::Justification::centredTop, 1);
    }
}

//==============================================================================
// Label — clean dark text on transparent background
//==============================================================================

void CustomLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const float alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font font (getLabelFont (label));

        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (font);

        auto textArea = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), textArea,
                          label.getJustificationType(),
                          juce::jmax (1, (int) ((float) textArea.getHeight() / font.getHeight())),
                          label.getMinimumHorizontalScale());
    }
}

//==============================================================================
// ComboBox — vintage styled
//==============================================================================

void CustomLookAndFeel::drawComboBox (juce::Graphics& g,
                                      int width, int height, bool isButtonDown,
                                      int /*buttonX*/, int /*buttonY*/,
                                      int /*buttonW*/, int /*buttonH*/,
                                      juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    const float cornerSize = 4.0f;

    // Background
    {
        juce::ColourGradient bg (kPanelBase.brighter (0.08f),
                                 0.0f, 0.0f,
                                 kPanelBase.darker (0.05f),
                                 0.0f, (float) height,
                                 false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (bounds, cornerSize);
    }

    // Border
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);

    // Arrow area
    {
        const float arrowZoneW = (float) height * 0.7f;
        auto arrowZone = bounds.removeFromRight (arrowZoneW).reduced (6.0f);

        juce::Path arrow;
        arrow.addTriangle (arrowZone.getCentreX() - arrowZone.getWidth() * 0.35f,
                           arrowZone.getCentreY() - 2.0f,
                           arrowZone.getCentreX() + arrowZone.getWidth() * 0.35f,
                           arrowZone.getCentreY() - 2.0f,
                           arrowZone.getCentreX(),
                           arrowZone.getCentreY() + 4.0f);

        g.setColour (kTextLabel.withAlpha (isButtonDown ? 0.9f : 0.6f));
        g.fillPath (arrow);
    }
}

//==============================================================================
// Popup Menu Item
//==============================================================================

void CustomLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                           const juce::Rectangle<int>& area,
                                           bool isSeparator,
                                           bool isActive,
                                           bool isHighlighted,
                                           bool isTicked,
                                           bool /*hasSubMenu*/,
                                           const juce::String& text,
                                           const juce::String& /*shortcutKeyText*/,
                                           const juce::Drawable* /*icon*/,
                                           const juce::Colour* textColourPtr)
{
    if (isSeparator)
    {
        auto sepArea = area.reduced (5, 0);
        g.setColour (kTextLabel.withAlpha (0.15f));
        g.fillRect (sepArea.getX(), sepArea.getCentreY(), sepArea.getWidth(), 1);
        return;
    }

    if (isHighlighted && isActive)
    {
        g.setColour (kAccentAmber.withAlpha (0.20f));
        g.fillRect (area);
    }

    juce::Colour textCol = textColourPtr != nullptr ? *textColourPtr : kTextLabel;
    if (! isActive)
        textCol = textCol.withAlpha (0.4f);

    auto textArea = area.reduced (10, 0);

    // Tick mark
    if (isTicked)
    {
        auto tickArea = textArea.removeFromLeft (textArea.getHeight());
        g.setColour (kAccentAmber);
        const float pad = (float) tickArea.getHeight() * 0.3f;
        juce::Path tick;
        tick.startNewSubPath (tickArea.getX() + pad, tickArea.getCentreY());
        tick.lineTo (tickArea.getCentreX(), tickArea.getBottom() - pad);
        tick.lineTo (tickArea.getRight() - pad, tickArea.getY() + pad);
        g.strokePath (tick, juce::PathStrokeType (1.8f));
    }

    g.setColour (textCol);
    g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmin (15.0f, (float) area.getHeight() * 0.55f), juce::Font::plain)));
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);
}

} // namespace valvane
