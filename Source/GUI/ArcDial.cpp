// ==========================================
// File: ArcDial.cpp
// ==========================================
#include "ArcDial.h"
#include "ColorPalette.h"
#include <cmath>

ArcDialLookAndFeel::ArcDialLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, GUI::ColorPalette::textBody);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, GUI::ColorPalette::panelBg);
    setColour(juce::ComboBox::textColourId, GUI::ColorPalette::textBody);
    setColour(juce::ComboBox::outlineColourId, GUI::ColorPalette::panelBorder);
    setColour(juce::ComboBox::arrowColourId, GUI::ColorPalette::textMuted);
    setColour(juce::PopupMenu::backgroundColourId, GUI::ColorPalette::panelBg);
    setColour(juce::PopupMenu::textColourId, GUI::ColorPalette::textBody);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, GUI::ColorPalette::neonPurple.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, GUI::ColorPalette::textBody);
    setColour(juce::ToggleButton::textColourId, GUI::ColorPalette::textBody);
    setColour(juce::ToggleButton::tickColourId, GUI::ColorPalette::neonGreen);
    setColour(juce::ToggleButton::tickDisabledColourId, GUI::ColorPalette::textMuted);
    setColour(juce::Label::textColourId, GUI::ColorPalette::textMuted);
}

void ArcDialLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& slider)
{
    const auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
    const auto centreX = (float)x + (float)width * 0.5f;
    const auto centreY = (float)y + (float)height * 0.5f;
    const auto rx = centreX - radius;
    const auto ry = centreY - radius;
    const auto rw = radius * 2.0f;
    const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const auto arcThickness = 5.0f;

    // 1. 背景トラック
    g.setColour(GUI::ColorPalette::sliderTrack);
    g.drawEllipse(rx, ry, rw, rw, arcThickness);

    // 1.5 モジュレーション・レンジ帯（値アークの下にピンクのハロー）
    const auto& props = slider.getProperties();
    const bool modActive = props.getWithDefault("mod_active", false);
    if (modActive)
    {
        const float mMin = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_min", 0.0f));
        const float mMax = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_max", 1.0f));
        const auto aLo = rotaryStartAngle + juce::jmin(mMin, mMax) * (rotaryEndAngle - rotaryStartAngle);
        const auto aHi = rotaryStartAngle + juce::jmax(mMin, mMax) * (rotaryEndAngle - rotaryStartAngle);

        if (std::abs(aHi - aLo) > 0.001f)
        {
            juce::Path band;
            band.addArc(rx, ry, rw, rw, aLo, aHi, true);
            g.setColour(juce::Colours::white.withAlpha(0.5f));
            g.strokePath(band, juce::PathStrokeType(arcThickness + 3.0f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 2. 値アーク
    juce::Path p;
    p.addArc(rx, ry, rw, rw, rotaryStartAngle, angle, true);

    const auto baseColour = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const auto lightColour = baseColour.brighter(0.6f);
    const auto darkColour = baseColour.darker(0.35f);

    juce::ColourGradient gradient(darkColour, rx, centreY, lightColour, rx + rw, centreY, false);
    g.setGradientFill(gradient);
    g.strokePath(p, juce::PathStrokeType(arcThickness, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // 3. ソフトグロー
    g.setColour(baseColour.withAlpha(0.15f));
    g.strokePath(p, juce::PathStrokeType(arcThickness + 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 4. ポインター
    juce::Path p2;
    const auto pointerLength = radius * 0.4f;
    p2.addRoundedRectangle(-1.5f, -radius + 1.5f, 3.0f, pointerLength, 1.5f);
    p2.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    g.setColour(GUI::ColorPalette::textBody);
    g.fillPath(p2);

    // 5. ライブ変調ドット
    if (modActive)
    {
        const float live = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_live", sliderPos));
        const auto aLive = rotaryStartAngle + live * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centreX + std::sin(aLive) * radius;
        const float dotY = centreY - std::cos(aLive) * radius;
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(dotX - 2.6f, dotY - 2.6f, 5.2f, 5.2f);
    }
}
