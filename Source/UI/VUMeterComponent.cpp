#include "VUMeterComponent.h"

namespace valvane
{

//==============================================================================
// Colour constants local to this TU
//==============================================================================

namespace MeterColours
{
    static const juce::Colour kFaceTop     { 248, 240, 218 };
    static const juce::Colour kFaceBottom  { 230, 218, 190 };
    static const juce::Colour kNeedle      { 180,  30,  30 };
    static const juce::Colour kArcLine     {  70,  60,  50 };
    static const juce::Colour kTextDark    {  50,  45,  40 };
    static const juce::Colour kBezelDark   {  80,  78,  75 };
    static const juce::Colour kBezelLight  { 160, 158, 155 };
    static const juce::Colour kBarGreen    {  60, 180,  90 };
    static const juce::Colour kBarYellow   { 220, 190,  50 };
    static const juce::Colour kBarRed      { 200,  50,  40 };
    static const juce::Colour kBarBg       {  50,  48,  45 };
}

//==============================================================================
// Construction / Destruction
//==============================================================================

VUMeterComponent::VUMeterComponent()
{
    startTimerHz (60);  // 60 fps physics & repaint
}

VUMeterComponent::~VUMeterComponent()
{
    stopTimer();
}

//==============================================================================
// Public setters
//==============================================================================

void VUMeterComponent::setGainReductionDb (float grDb)
{
    displayGrDb  = grDb;
    needleTarget = grDbToAngle (grDb);
}

void VUMeterComponent::setInputLevel (float levelDb)
{
    inputLevelDb = levelDb;
}

void VUMeterComponent::setOutputLevel (float levelDb)
{
    outputLevelDb = levelDb;
}

//==============================================================================
// Timer — physics integration
//==============================================================================

void VUMeterComponent::timerCallback()
{
    // Semi-implicit Euler integration step
    const float dt = 1.0f / 60.0f;  // ~16.67 ms

    const float springForce  = kSpringK * (needleTarget - needlePosition);
    const float dampingForce = kDamping * needleVelocity;

    needleVelocity += dt * (springForce - dampingForce) / kMass;
    needlePosition += dt * needleVelocity;

    // Clamp to valid range
    needlePosition = juce::jlimit (0.0f, 1.0f, needlePosition);

    repaint();
}

//==============================================================================
// GR dB → normalised angle  (0 dB → 0.0, −20 dB → 1.0)
//==============================================================================

float VUMeterComponent::grDbToAngle (float grDb) const
{
    // GR scale: 0 dB = far left (0.0),  20 dB = far right (1.0)
    // grDb is positive = amount of reduction
    return juce::jlimit (0.0f, 1.0f, grDb / 20.0f);
}

//==============================================================================
// Paint
//==============================================================================

void VUMeterComponent::paint (juce::Graphics& g)
{
    auto localBounds = getLocalBounds().toFloat();

    // Decide layout: meter face on top, small level bars at the bottom
    const float barH   = juce::jmax (16.0f, localBounds.getHeight() * 0.10f);
    const float barGap = 4.0f;

    auto meterBounds = localBounds.withTrimmedBottom (barH * 2.0f + barGap * 3.0f);
    auto barArea     = localBounds;
    barArea.setTop (meterBounds.getBottom() + barGap);

    // Draw the meter face & needle
    drawMeterFace (g, meterBounds);

    // Convert normalised position to a sweep angle for the needle
    // Arc spans roughly −40° to +40° from vertical
    const float arcStart = juce::degreesToRadians (-40.0f);
    const float arcEnd   = juce::degreesToRadians ( 40.0f);
    const float needleAngle = arcStart + needlePosition * (arcEnd - arcStart);

    drawNeedle (g, meterBounds, needleAngle);

    // Level bar meters
    float halfW = (barArea.getWidth() - barGap) * 0.5f;
    auto inputBar  = juce::Rectangle<float> (barArea.getX(), barArea.getY(),
                                             halfW, barH);
    auto outputBar = juce::Rectangle<float> (barArea.getX() + halfW + barGap,
                                             barArea.getY(), halfW, barH);

    drawLevelBar (g, inputBar,  inputLevelDb,  "IN",  true);
    drawLevelBar (g, outputBar, outputLevelDb, "OUT", false);

    // dB readout below the needle
    {
        g.setColour (MeterColours::kTextDark);
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (11.0f, meterBounds.getHeight() * 0.075f), juce::Font::bold)));

        juce::String grText;
        if (displayGrDb < 0.05f)
            grText = "0.0 dB";
        else
            grText = juce::String (displayGrDb, 1) + " dB";

        auto readoutArea = meterBounds;
        readoutArea.setTop (meterBounds.getBottom() - meterBounds.getHeight() * 0.12f);
        g.drawFittedText (grText, readoutArea.toNearestInt(),
                          juce::Justification::centred, 1);
    }
}

//==============================================================================
// Meter face
//==============================================================================

void VUMeterComponent::drawMeterFace (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const float cornerRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.06f;

    //--- Outer bezel / frame ---
    {
        // Dark outer bezel
        g.setColour (MeterColours::kBezelDark);
        g.fillRoundedRectangle (bounds, cornerRadius);

        // Lighter inner bezel for 3D recess
        auto inner = bounds.reduced (2.5f);
        g.setColour (MeterColours::kBezelLight);
        g.fillRoundedRectangle (inner, cornerRadius * 0.8f);

        // Inner shadow
        {
            juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.12f),
                                         inner.getX(), inner.getY(),
                                         juce::Colours::transparentBlack,
                                         inner.getX(), inner.getY() + 8.0f,
                                         false);
            g.setGradientFill (shadow);
            g.fillRoundedRectangle (inner, cornerRadius * 0.8f);
        }
    }

    //--- Meter face (cream area) ---
    auto face = bounds.reduced (bounds.getWidth() * 0.06f, bounds.getHeight() * 0.08f);
    {
        juce::ColourGradient faceGrad (MeterColours::kFaceTop,
                                       face.getX(), face.getY(),
                                       MeterColours::kFaceBottom,
                                       face.getX(), face.getBottom(),
                                       false);
        g.setGradientFill (faceGrad);
        g.fillRoundedRectangle (face, cornerRadius * 0.5f);
    }

    //--- Arc and dB markings ---
    {
        const float arcCX   = face.getCentreX();
        const float arcCY   = face.getBottom() - face.getHeight() * 0.15f;
        const float arcR    = face.getWidth() * 0.40f;
        const float arcStart = juce::degreesToRadians (-40.0f);
        const float arcEnd   = juce::degreesToRadians ( 40.0f);

        // Draw the arc line
        {
            juce::Path arc;
            arc.addCentredArc (arcCX, arcCY, arcR, arcR,
                               0.0f, arcStart, arcEnd, true);
            g.setColour (MeterColours::kArcLine.withAlpha (0.7f));
            g.strokePath (arc, juce::PathStrokeType (1.5f));
        }

        // dB markings
        struct TickLabel
        {
            float grDb;
            const char* label;
        };
        const TickLabel ticks[] = {
            {  0.0f, "0"   },
            {  3.0f, "3"   },
            {  6.0f, "6"   },
            { 10.0f, "10"  },
            { 20.0f, "20"  },
        };

        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, face.getHeight() * 0.065f), juce::Font::plain)));

        for (auto& t : ticks)
        {
            float norm  = t.grDb / 20.0f;
            float angle = arcStart + norm * (arcEnd - arcStart);

            float cosA = std::cos (angle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin (angle - juce::MathConstants<float>::halfPi);

            float innerR = arcR - 5.0f;
            float outerR = arcR + 3.0f;

            float x1 = arcCX + innerR * cosA;
            float y1 = arcCY + innerR * sinA;
            float x2 = arcCX + outerR * cosA;
            float y2 = arcCY + outerR * sinA;

            g.setColour (MeterColours::kArcLine);
            g.drawLine (x1, y1, x2, y2, 1.2f);

            // Label
            float labelR = arcR + 10.0f;
            float lx = arcCX + labelR * cosA;
            float ly = arcCY + labelR * sinA;

            g.setColour (MeterColours::kTextDark);
            g.drawSingleLineText (t.label,
                                  juce::roundToInt (lx) - 6,
                                  juce::roundToInt (ly) + 4);
        }
    }

    //--- "GR" label ---
    {
        g.setColour (MeterColours::kTextDark.withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (11.0f, face.getHeight() * 0.09f), juce::Font::bold)));

        auto labelRect = face;
        labelRect.setTop (face.getY() + face.getHeight() * 0.08f);
        labelRect.setBottom (face.getY() + face.getHeight() * 0.25f);
        g.drawFittedText ("GR", labelRect.toNearestInt(),
                          juce::Justification::centred, 1);
    }

    //--- "dB" subtitle ---
    {
        g.setColour (MeterColours::kTextDark.withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, face.getHeight() * 0.06f), juce::Font::plain)));

        auto subRect = face;
        subRect.setTop (face.getY() + face.getHeight() * 0.24f);
        subRect.setBottom (face.getY() + face.getHeight() * 0.34f);
        g.drawFittedText ("dB", subRect.toNearestInt(),
                          juce::Justification::centred, 1);
    }
}

//==============================================================================
// Needle
//==============================================================================

void VUMeterComponent::drawNeedle (juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   float angle)
{
    auto face = bounds.reduced (bounds.getWidth() * 0.06f, bounds.getHeight() * 0.08f);

    const float pivotX = face.getCentreX();
    const float pivotY = face.getBottom() - face.getHeight() * 0.15f;
    const float needleLen = face.getWidth() * 0.40f;

    // Needle tip position
    float tipX = pivotX + std::sin (angle) * needleLen;
    float tipY = pivotY - std::cos (angle) * needleLen;

    // Tapered needle — thick at pivot, thin at tip
    {
        const float baseHalf = juce::jmax (2.0f, needleLen * 0.018f);
        const float tipHalf  = juce::jmax (0.5f, needleLen * 0.004f);

        // Perpendicular direction for width
        float perpX = std::cos (angle);
        float perpY = std::sin (angle);

        juce::Path needlePath;
        needlePath.startNewSubPath (pivotX - perpX * baseHalf,
                                    pivotY - perpY * baseHalf);
        needlePath.lineTo          (tipX   - perpX * tipHalf,
                                    tipY   - perpY * tipHalf);
        needlePath.lineTo          (tipX   + perpX * tipHalf,
                                    tipY   + perpY * tipHalf);
        needlePath.lineTo          (pivotX + perpX * baseHalf,
                                    pivotY + perpY * baseHalf);
        needlePath.closeSubPath();

        // Needle shadow
        g.setColour (juce::Colours::black.withAlpha (0.18f));
        g.fillPath (needlePath, juce::AffineTransform::translation (1.0f, 1.5f));

        // Needle body
        g.setColour (MeterColours::kNeedle);
        g.fillPath (needlePath);
    }

    // Pivot circle
    {
        const float pivotR = juce::jmax (3.5f, needleLen * 0.045f);

        // Shadow
        g.setColour (juce::Colours::black.withAlpha (0.20f));
        g.fillEllipse (pivotX - pivotR + 0.5f, pivotY - pivotR + 1.0f,
                       pivotR * 2.0f, pivotR * 2.0f);

        // Dark pivot cap
        juce::ColourGradient capGrad (juce::Colour (60, 58, 55),
                                      pivotX, pivotY - pivotR,
                                      juce::Colour (30, 28, 25),
                                      pivotX, pivotY + pivotR,
                                      false);
        g.setGradientFill (capGrad);
        g.fillEllipse (pivotX - pivotR, pivotY - pivotR,
                       pivotR * 2.0f, pivotR * 2.0f);

        // Highlight
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawEllipse (pivotX - pivotR, pivotY - pivotR,
                       pivotR * 2.0f, pivotR * 2.0f, 0.8f);
    }
}

//==============================================================================
// Small level bar
//==============================================================================

void VUMeterComponent::drawLevelBar (juce::Graphics& g,
                                     juce::Rectangle<float> bounds,
                                     float levelDb,
                                     const juce::String& label,
                                     bool isInput)
{
    juce::ignoreUnused (isInput);

    const float cornerR = 2.0f;

    // Background
    g.setColour (MeterColours::kBarBg);
    g.fillRoundedRectangle (bounds, cornerR);

    // Level mapping: -60 dB → 0, 0 dB → 1
    float norm = juce::jlimit (0.0f, 1.0f, (levelDb + 60.0f) / 60.0f);

    if (norm > 0.0f)
    {
        auto filled = bounds;
        filled.setWidth (bounds.getWidth() * norm);

        // Colour gradient: green → yellow → red
        juce::Colour barCol;
        if (norm < 0.6f)
            barCol = MeterColours::kBarGreen;
        else if (norm < 0.85f)
            barCol = MeterColours::kBarGreen.interpolatedWith (MeterColours::kBarYellow,
                         (norm - 0.6f) / 0.25f);
        else
            barCol = MeterColours::kBarYellow.interpolatedWith (MeterColours::kBarRed,
                         (norm - 0.85f) / 0.15f);

        g.setColour (barCol);
        g.fillRoundedRectangle (filled, cornerR);

        // Subtle inner highlight
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.fillRoundedRectangle (filled.removeFromTop (filled.getHeight() * 0.35f), cornerR);
    }

    // Label text
    g.setColour (juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, bounds.getHeight() * 0.60f), juce::Font::bold)));
    g.drawFittedText (label, bounds.reduced (4.0f, 0.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);

    // dB readout on the right
    {
        juce::String dbText = (levelDb <= -59.0f)
                              ? "-inf"
                              : juce::String (levelDb, 1);

        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.0f, bounds.getHeight() * 0.50f), juce::Font::plain)));
        g.drawFittedText (dbText, bounds.reduced (4.0f, 0.0f).toNearestInt(),
                          juce::Justification::centredRight, 1);
    }
}

//==============================================================================
void VUMeterComponent::resized()
{
    // Nothing to lay out — all drawing is relative to component bounds.
}

} // namespace valvane
