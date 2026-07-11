#pragma once
#include <JuceHeader.h>

namespace valvane
{

//==============================================================================
/// Analog VU-style needle meter for gain-reduction display.
///
/// Uses a second-order mass-spring-damper system to drive the needle angle,
/// producing a realistic, slightly under-damped swing.  A 60 fps timer
/// drives the physics integration and repaint cycle.
class VUMeterComponent : public juce::Component,
                         private juce::Timer
{
public:
    VUMeterComponent();
    ~VUMeterComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /// Set the target gain-reduction value in dB (positive = reduction).
    void setGainReductionDb (float grDb);

    /// Set the current input level in dB for the small bar meter.
    void setInputLevel (float levelDb);

    /// Set the current output level in dB for the small bar meter.
    void setOutputLevel (float levelDb);

private:
    void timerCallback() override;

    //--- Spring-mass-damper state -------------------------------------------
    float needlePosition = 0.0f;   // current angle in normalised 0-1 range
    float needleVelocity = 0.0f;
    float needleTarget   = 0.0f;

    //--- Physics constants --------------------------------------------------
    static constexpr float kSpringK  = 150.0f;
    static constexpr float kDamping  =  18.0f;  // slightly under-damped
    static constexpr float kMass     =   1.0f;

    //--- Level / display state ----------------------------------------------
    float inputLevelDb  = -60.0f;
    float outputLevelDb = -60.0f;
    float displayGrDb   =   0.0f;

    //--- Drawing helpers ----------------------------------------------------
    void  drawMeterFace (juce::Graphics& g, juce::Rectangle<float> bounds);
    void  drawNeedle    (juce::Graphics& g, juce::Rectangle<float> bounds, float angle);
    void  drawLevelBar  (juce::Graphics& g, juce::Rectangle<float> bounds,
                         float levelDb, const juce::String& label, bool isInput);
    float grDbToAngle   (float grDb) const;
    float dbToNormalized (float db) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VUMeterComponent)
};

} // namespace valvane
