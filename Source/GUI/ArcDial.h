// ==========================================
// File: ArcDial.h
// パステル・アークダイアル LookAndFeel (HighPrecisionEQ 由来)
// ==========================================
#pragma once

#include <JuceHeader.h>

class ArcDialLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ArcDialLookAndFeel();
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
};
