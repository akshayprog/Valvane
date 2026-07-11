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
// GR dB → normalised angle  (0 dB on right, −20 dB on left)
//==============================================================================

float VUMeterComponent::grDbToAngle (float grDb) const
{
    // grDb is positive (amount of reduction).
    // In GR mode, 0 dB of reduction is on the right side of the scale (corresponding to 0 dB),
    // and deflecting to the left (negative dB values) represents compression.
    return dbToNormalized (-grDb);
}

float VUMeterComponent::dbToNormalized (float db) const
{
    if (db <= -20.0f) return 0.0f;
    if (db >= 3.0f)   return 1.0f;

    if (db < -10.0f)
    {
        // Map -20 to -10 -> 0.0 to 0.35 (wider spacing at the left)
        return 0.0f + 0.35f * (db + 20.0f) / 10.0f;
    }
    else if (db < 0.0f)
    {
        // Map -10 to 0 -> 0.35 to 0.85 (logarithmic-like stretching around 0 dB)
        return 0.35f + 0.50f * (db + 10.0f) / 10.0f;
    }
    else
    {
        // Map 0 to +3 -> 0.85 to 1.0 (positive overshoot region)
        return 0.85f + 0.15f * db / 3.0f;
    }
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
    const float cornerRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.05f;

    //--- Outer bezel / frame (Deeply recessed 3D hardware look) ---
    {
        // Dark outer bezel
        g.setColour (juce::Colour (55, 55, 57));
        g.fillRoundedRectangle (bounds, cornerRadius);

        // Lighter inner bezel for 3D recess highlight
        auto inner = bounds.reduced (2.0f);
        g.setColour (juce::Colour (100, 100, 103));
        g.fillRoundedRectangle (inner, cornerRadius * 0.8f);

        // Inner shadow inside the bezel to create deep recess
        auto innerFace = bounds.reduced (5.0f);
        g.setColour (juce::Colour (35, 35, 37));
        g.fillRoundedRectangle (innerFace, cornerRadius * 0.7f);
    }

    //--- Meter face (warm cream/amber vintage backlit area) ---
    auto face = bounds.reduced (bounds.getWidth() * 0.06f, bounds.getHeight() * 0.08f);
    {
        // Radial-like gradient for glowing lightbulb effect
        juce::ColourGradient faceGrad (juce::Colour (255, 238, 185), // Warm glow center
                                       face.getCentreX(), face.getY() + 10.0f,
                                       juce::Colour (225, 200, 130), // Richer amber bottom
                                       face.getCentreX(), face.getBottom(),
                                       false);
        g.setGradientFill (faceGrad);
        g.fillRoundedRectangle (face, cornerRadius * 0.5f);

        // Very subtle dirt / shadow at the face edges
        g.setColour (juce::Colours::black.withAlpha (0.05f));
        g.drawRoundedRectangle (face, cornerRadius * 0.5f, 1.0f);
    }

    //--- Constants for Arc drawing ---
    const float arcCX    = face.getCentreX();
    const float arcCY    = face.getBottom() - face.getHeight() * 0.12f;
    const float arcR     = face.getWidth() * 0.44f;
    const float arcStart = juce::degreesToRadians (-40.0f);
    const float arcEnd   = juce::degreesToRadians ( 40.0f);

    //--- Draw the two arc lines (Upper dB scale & Lower % modulation scale) ---
    {
        juce::Path upperArc;
        upperArc.addCentredArc (arcCX, arcCY, arcR, arcR, 0.0f, arcStart, arcEnd, true);
        g.setColour (MeterColours::kArcLine.withAlpha (0.7f));
        g.strokePath (upperArc, juce::PathStrokeType (1.5f));

        juce::Path lowerArc;
        float lowerR = arcR * 0.85f;
        lowerArc.addCentredArc (arcCX, arcCY, lowerR, lowerR, 0.0f, arcStart, arcEnd, true);
        g.setColour (MeterColours::kArcLine.withAlpha (0.5f));
        g.strokePath (lowerArc, juce::PathStrokeType (1.0f));
    }

    //--- Upper Scale: Ticks and dB Labels (-20 to +3 dB) ---
    {
        struct MajorTick
        {
            float db;
            const char* label;
            bool isRed;
        };

        const MajorTick majorTicks[] = {
            { -20.0f, "20",  false },
            { -10.0f, "10",  false },
            {  -7.0f, "7",   false },
            {  -5.0f, "5",   false },
            {  -3.0f, "3",   false },
            {  -2.0f, "2",   false },
            {  -1.0f, "1",   false },
            {   0.0f, "0",   true  },
            {   1.0f, "+1",  true  },
            {   2.0f, "+2",  true  },
            {   3.0f, "+3",  true  }
        };

        // Draw Minor Ticks (High Density for professional production look)
        g.setColour (MeterColours::kArcLine.withAlpha (0.4f));
        for (float db = -20.0f; db <= 3.0f; db += 0.5f)
        {
            // Skip major ticks to avoid duplicate drawings
            bool isMajor = false;
            for (auto& mt : majorTicks)
            {
                if (std::abs (db - mt.db) < 0.1f)
                {
                    isMajor = true;
                    break;
                }
            }
            if (isMajor) continue;

            float norm = dbToNormalized (db);
            float angle = arcStart + norm * (arcEnd - arcStart);

            float cosA = std::cos (angle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin (angle - juce::MathConstants<float>::halfPi);

            float innerR = arcR - (db >= 0.0f ? 3.0f : 2.5f);
            float outerR = arcR;

            g.setColour (db >= 0.0f ? MeterColours::kBarRed.withAlpha (0.7f) : MeterColours::kArcLine.withAlpha (0.4f));
            g.drawLine (arcCX + innerR * cosA, arcCY + innerR * sinA,
                        arcCX + outerR * cosA, arcCY + outerR * sinA, 0.8f);
        }

        // Draw Major Ticks and Text Labels
        for (auto& mt : majorTicks)
        {
            float norm = dbToNormalized (mt.db);
            float angle = arcStart + norm * (arcEnd - arcStart);

            float cosA = std::cos (angle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin (angle - juce::MathConstants<float>::halfPi);

            float innerR = arcR - 6.5f;
            float outerR = arcR + 2.0f;

            // Tick lines
            g.setColour (mt.isRed ? MeterColours::kBarRed : MeterColours::kArcLine);
            g.drawLine (arcCX + innerR * cosA, arcCY + innerR * sinA,
                        arcCX + outerR * cosA, arcCY + outerR * sinA, 1.5f);

            // Labels
            float labelR = arcR + 11.0f;
            float lx = arcCX + labelR * cosA;
            float ly = arcCY + labelR * sinA;

            g.setColour (mt.isRed ? MeterColours::kBarRed : MeterColours::kTextDark);
            g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, face.getHeight() * 0.07f), juce::Font::bold)));
            
            auto textWidth = g.getCurrentFont().getStringWidth (mt.label);
            auto textHeight = g.getCurrentFont().getHeight();
            g.drawText (mt.label,
                        juce::roundToInt (lx - (float) textWidth * 0.5f),
                        juce::roundToInt (ly - (float) textHeight * 0.5f),
                        textWidth, textHeight,
                        juce::Justification::centred, false);
        }
    }

    //--- Lower Scale: Modulation / Percentage (0 to 100%) ---
    {
        const int pctTicks[] = { 0, 20, 40, 60, 80, 100 };
        float lowerR = arcR * 0.85f;

        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.5f, face.getHeight() * 0.055f), juce::Font::plain)));
        g.setColour (MeterColours::kTextDark.withAlpha (0.6f));

        for (int pct : pctTicks)
        {
            float norm = (float) pct / 100.0f;
            float angle = arcStart + norm * (arcEnd - arcStart);

            float cosA = std::cos (angle - juce::MathConstants<float>::halfPi);
            float sinA = std::sin (angle - juce::MathConstants<float>::halfPi);

            // Ticks pointing inwards
            float innerR = lowerR - 4.0f;
            float outerR = lowerR;
            g.drawLine (arcCX + innerR * cosA, arcCY + innerR * sinA,
                        arcCX + outerR * cosA, arcCY + outerR * sinA, 1.0f);

            // Labels below lower arc
            float labelR = lowerR - 10.0f;
            float lx = arcCX + labelR * cosA;
            float ly = arcCY + labelR * sinA;

            juce::String lbl (pct);
            auto textWidth = g.getCurrentFont().getStringWidth (lbl);
            auto textHeight = g.getCurrentFont().getHeight();
            g.drawText (lbl,
                        juce::roundToInt (lx - (float) textWidth * 0.5f),
                        juce::roundToInt (ly - (float) textHeight * 0.5f),
                        textWidth, textHeight,
                        juce::Justification::centred, false);
        }
    }

    //--- "VU LEVEL INDICATOR" subtitle at the top ---
    {
        g.setColour (MeterColours::kTextDark.withAlpha (0.6f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.0f, face.getHeight() * 0.055f), juce::Font::bold)));
        
        auto labelRect = face;
        labelRect.setTop (face.getY() + face.getHeight() * 0.06f);
        labelRect.setBottom (face.getY() + face.getHeight() * 0.16f);
        
        // Custom spaced letter rendering for "V U   L E V E L   I N D I C A T O R"
        g.drawFittedText ("V U   L E V E L   I N D I C A T O R", labelRect.toNearestInt(),
                          juce::Justification::centred, 1);
    }

    //--- Large brand logo "VALVANE" and "by AJaudio" inside the face ---
    {
        g.setColour (MeterColours::kTextDark.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", juce::jmax (13.0f, face.getHeight() * 0.10f), juce::Font::bold)));

        auto logoRect = face;
        logoRect.setTop (face.getCentreY() + face.getHeight() * 0.02f);
        logoRect.setBottom (face.getCentreY() + face.getHeight() * 0.18f);
        g.drawFittedText ("VALVANE", logoRect.toNearestInt(), juce::Justification::centred, 1);

        g.setColour (MeterColours::kTextDark.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.5f, face.getHeight() * 0.05f), juce::Font::plain)));
        
        auto subLogoRect = face;
        subLogoRect.setTop (face.getCentreY() + face.getHeight() * 0.16f);
        subLogoRect.setBottom (face.getCentreY() + face.getHeight() * 0.28f);
        g.drawFittedText ("by AJaudio", subLogoRect.toNearestInt(), juce::Justification::centred, 1);
    }

    //--- Left/Right "VU" stamps ---
    {
        g.setColour (MeterColours::kTextDark.withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions ("Georgia", juce::jmax (11.0f, face.getHeight() * 0.08f), juce::Font::italic | juce::Font::bold)));

        g.drawSingleLineText ("VU", juce::roundToInt (face.getX() + 8.0f), juce::roundToInt (face.getCentreY() - 10.0f));
        g.drawSingleLineText ("VU", juce::roundToInt (face.getRight() - 26.0f), juce::roundToInt (face.getCentreY() - 10.0f));
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
