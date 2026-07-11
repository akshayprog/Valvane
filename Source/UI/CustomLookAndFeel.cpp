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

    const float margin   = juce::jmin (width, height) * 0.16f; // slightly larger margin for faceplate numbers
    const auto  area     = juce::Rectangle<float> ((float) x, (float) y,
                                                   (float) width, (float) height)
                               .reduced (margin);
    const float diameter = juce::jmin (area.getWidth(), area.getHeight());
    const float radius   = diameter * 0.5f;
    const float centreX  = area.getCentreX();
    const float centreY  = area.getCentreY();
    const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float outerRadius = radius;
    const float knobRadius  = radius * 0.76f;

    //--- Tick marks and numbers around the faceplate arc ---
    {
        const float tickInner = outerRadius + 1.0f;
        const float tickOuter = outerRadius + 5.0f;
        const float labelRadius = outerRadius + 13.0f;
        const int   numTicks  = 10; // 0 to 10 scale (11 steps total)

        g.setColour (kTextLabel.withAlpha (0.75f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.5f, radius * 0.18f), juce::Font::bold)));

        for (int i = 0; i <= numTicks; ++i)
        {
            float t = (float) i / (float) numTicks;
            // Angle mapping (swapped sin/cos to make 12 o'clock the top center)
            float tAngle = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);

            float cosA = std::cos (tAngle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin (tAngle - juce::MathConstants<float>::halfPi);

            // Draw tick line
            float x1 = centreX + tickInner * cosA;
            float y1 = centreY + tickInner * sinA;
            float x2 = centreX + tickOuter * cosA;
            float y2 = centreY + tickOuter * sinA;
            g.drawLine (x1, y1, x2, y2, (i % 5 == 0) ? 1.5f : 0.8f);

            // Draw numbers (e.g. 0, 10, 20... 100 for large knobs, or 0...10 for small)
            juce::String labelStr;
            if (width >= 90)
                labelStr = juce::String (i * 10);
            else
                labelStr = juce::String (i);

            float lx = centreX + labelRadius * cosA;
            float ly = centreY + labelRadius * sinA;

            auto textWidth = g.getCurrentFont().getStringWidth (labelStr);
            auto textHeight = g.getCurrentFont().getHeight();
            
            g.drawText (labelStr,
                        juce::roundToInt (lx - (float) textWidth * 0.5f),
                        juce::roundToInt (ly - (float) textHeight * 0.5f) + 1, // small visual tweak
                        textWidth, textHeight,
                        juce::Justification::centred, false);
        }
    }

    //--- Drop shadow under the knob ---
    {
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.35f),
                                     centreX, centreY,
                                     juce::Colours::transparentBlack,
                                     centreX, centreY + outerRadius * 1.15f,
                                     true);
        g.setGradientFill (shadow);
        g.fillEllipse (centreX - outerRadius * 1.05f,
                       centreY - outerRadius * 1.05f + 3.0f,
                       outerRadius * 2.1f,
                       outerRadius * 2.1f);
    }

    //--- Outer fluted ring body (Black/Charcoal matte) ---
    {
        juce::Colour baseColor (22, 22, 24);
        juce::ColourGradient ringGrad (baseColor.brighter (0.1f),
                                       centreX, centreY - outerRadius,
                                       baseColor.darker (0.15f),
                                       centreX, centreY + outerRadius,
                                       false);
        g.setGradientFill (ringGrad);
        g.fillEllipse (centreX - outerRadius, centreY - outerRadius,
                       outerRadius * 2.0f, outerRadius * 2.0f);

        // Fluted ridges around the circumference
        const int numRidges = 36;
        const float ridgeInner = outerRadius * 0.88f;

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        for (int i = 0; i < numRidges; ++i)
        {
            float rAngle = juce::MathConstants<float>::twoPi * (float) i / (float) numRidges;
            float cosR = std::cos (rAngle);
            float sinR = std::sin (rAngle);

            // Draw dark valley line
            g.drawLine (centreX + ridgeInner * cosR, centreY + ridgeInner * sinR,
                        centreX + outerRadius * cosR, centreY + outerRadius * sinR, 1.2f);
        }
    }

    //--- Inner knob face (recessed fluted cylinder) ---
    {
        juce::Colour baseColor (28, 28, 30);
        juce::ColourGradient innerGrad (baseColor,
                                        centreX, centreY - knobRadius,
                                        baseColor.darker (0.25f),
                                        centreX, centreY + knobRadius,
                                        false);
        g.setGradientFill (innerGrad);
        g.fillEllipse (centreX - knobRadius, centreY - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f);

        // 3D edge rim highlight
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawEllipse (centreX - knobRadius, centreY - knobRadius,
                       knobRadius * 2.0f, knobRadius * 2.0f, 1.0f);
    }

    //--- Pointer / indicator line ---
    {
        const float pointerLength = knobRadius * 0.95f;
        const float pointerWidth  = juce::jmax (1.8f, knobRadius * 0.07f);

        juce::Path pointerPath;
        pointerPath.addRectangle (-pointerWidth * 0.5f, -pointerLength,
                                  pointerWidth, pointerLength);

        g.setColour (kKnobPointer);
        g.fillPath (pointerPath, juce::AffineTransform::rotation (angle)
                                      .translated (centreX, centreY));
    }

    //--- Center cap (Large brushed-metal silver cap, typical of LA-2A) ---
    {
        const float capRadius = knobRadius * 0.44f;
        juce::ColourGradient capGrad (juce::Colour (245, 245, 245),
                                      centreX - capRadius * 0.4f, centreY - capRadius * 0.4f,
                                      juce::Colour (180, 182, 185),
                                      centreX + capRadius * 0.4f, centreY + capRadius * 0.4f,
                                      true);
        g.setGradientFill (capGrad);
        g.fillEllipse (centreX - capRadius, centreY - capRadius,
                       capRadius * 2.0f, capRadius * 2.0f);

        // Center cap black ring outline
        g.setColour (juce::Colour (30, 30, 32));
        g.drawEllipse (centreX - capRadius, centreY - capRadius,
                       capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // High gloss highlight
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawEllipse (centreX - capRadius + 1.0f, centreY - capRadius + 1.0f,
                       (capRadius - 1.0f) * 2.0f, (capRadius - 1.0f) * 2.0f, 0.8f);
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
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto bounds = button.getLocalBounds().toFloat();
    const bool isOn   = button.getToggleState();

    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY() - 6.0f; // Shift up to leave room for label

    //--- Threaded mounting ring (metal finish) ---
    {
        const float nutRadius = 11.5f;

        // Shadow under the ring
        g.setColour (juce::Colours::black.withAlpha (0.15f));
        g.fillEllipse (centreX - nutRadius, centreY - nutRadius + 1.5f, nutRadius * 2.0f, nutRadius * 2.0f);

        // Metal base ring gradient
        juce::ColourGradient nutGrad (juce::Colour (210, 210, 212),
                                      centreX - nutRadius * 0.5f, centreY - nutRadius * 0.5f,
                                      juce::Colour (135, 135, 138),
                                      centreX + nutRadius * 0.5f, centreY + nutRadius * 0.5f,
                                      true);
        g.setGradientFill (nutGrad);
        g.fillEllipse (centreX - nutRadius, centreY - nutRadius, nutRadius * 2.0f, nutRadius * 2.0f);

        // Highlight ring
        g.setColour (juce::Colours::white.withAlpha (0.3f));
        g.drawEllipse (centreX - nutRadius + 1.0f, centreY - nutRadius + 1.0f, (nutRadius - 1.0f) * 2.0f, (nutRadius - 1.0f) * 2.0f, 0.8f);

        // Outer dark edge
        g.setColour (juce::Colour (90, 90, 92));
        g.drawEllipse (centreX - nutRadius, centreY - nutRadius, nutRadius * 2.0f, nutRadius * 2.0f, 1.0f);
    }

    //--- Inner dark switch hole ---
    {
        const float holeRadius = 5.0f;
        g.setColour (juce::Colour (20, 20, 22));
        g.fillEllipse (centreX - holeRadius, centreY - holeRadius, holeRadius * 2.0f, holeRadius * 2.0f);
    }

    //--- Metal Switch Lever (vertical bat shape) ---
    {
        const float leverLength = 15.5f;
        const float leverWidthBottom = 3.5f;
        const float leverWidthTop = 2.2f;

        // Up for active, down for inactive (simulate real toggle click)
        float leverAngle = isOn ? juce::degreesToRadians (-14.0f) : juce::degreesToRadians (14.0f);

        juce::Path lever;
        lever.startNewSubPath (-leverWidthBottom * 0.5f, 0.0f);
        lever.lineTo (-leverWidthTop * 0.5f, -leverLength);
        lever.quadraticTo (0.0f, -leverLength - 2.0f, leverWidthTop * 0.5f, -leverLength);
        lever.lineTo (leverWidthBottom * 0.5f, 0.0f);
        lever.closeSubPath();

        // Rotate and place
        lever.applyTransform (juce::AffineTransform::rotation (leverAngle).translated (centreX, centreY));

        // Lever shadow
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillPath (lever, juce::AffineTransform::translation (1.5f, 2.5f));

        // Lever metal gradient
        juce::ColourGradient leverGrad (juce::Colour (255, 255, 255),
                                        centreX, centreY - leverLength,
                                        juce::Colour (130, 132, 135),
                                        centreX, centreY,
                                        false);
        g.setGradientFill (leverGrad);
        g.fillPath (lever);

        // Light specular edge highlight
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.strokePath (lever, juce::PathStrokeType (0.5f));
    }

    //--- Label text below the switch ---
    {
        g.setColour (kTextLabel);
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.5f, bounds.getHeight() * 0.16f), juce::Font::bold)));

        auto labelArea = bounds;
        labelArea.setTop (centreY + 14.0f);
        g.drawFittedText (button.getButtonText(), labelArea.toNearestInt(),
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
