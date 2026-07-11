#pragma once
#include <JuceHeader.h>

namespace valvane
{

//==============================================================================
/// Custom LookAndFeel providing a vintage brushed-aluminum hardware aesthetic.
/// Draws knurled rotary knobs, 3-D toggle switches, a brushed-metal panel
/// background, and clean vintage-style labels/combos.
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();
    ~CustomLookAndFeel() override;

    //--- Rotary knob with knurled skirt, pointer & tick marks ----------------
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    //--- 3-D toggle switch ---------------------------------------------------
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    //--- Clean label ---------------------------------------------------------
    void drawLabel (juce::Graphics&, juce::Label&) override;

    //--- Vintage combo box ---------------------------------------------------
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    //--- Popup menu item -----------------------------------------------------
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon,
                            const juce::Colour* textColour) override;

    //--- Brushed-metal panel background (static helper) ----------------------
    static void paintBrushedMetalBackground (juce::Graphics& g,
                                             juce::Rectangle<int> bounds);

    //==========================================================================
    // Colour palette constants
    //==========================================================================
    static inline const juce::Colour kPanelBase       { 195, 195, 190 };
    static inline const juce::Colour kKnobBodyTop     {  60,  60,  65 };
    static inline const juce::Colour kKnobBodyBottom  {  40,  40,  45 };
    static inline const juce::Colour kKnobPointer     { 240, 240, 240 };
    static inline const juce::Colour kTextLabel        {  50,  50,  50 };
    static inline const juce::Colour kTextValue        {  80,  80,  80 };
    static inline const juce::Colour kAccentAmber      { 200, 160,  60 };
    static inline const juce::Colour kMeterNeedle      { 180,  30,  30 };
    static inline const juce::Colour kMeterFace        { 245, 235, 210 };

private:
    juce::Font defaultFont;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomLookAndFeel)
};

} // namespace valvane
