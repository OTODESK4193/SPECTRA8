// ==========================================
// File: ArcDial.h
// パステル・アークダイアル LookAndFeel（Granular 準拠）
// モジュレーションレンジ帯とライブ値ドットの描画に対応。
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
