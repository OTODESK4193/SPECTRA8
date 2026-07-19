// ==========================================
// File: ArcDial.cpp
// ==========================================
#include "ArcDial.h"
#include "ColorPalette.h"
#include <cmath>

ArcDialLookAndFeel::ArcDialLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, SpectraColors::text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::ComboBox::backgroundColourId, SpectraColors::panel);
    setColour(juce::ComboBox::textColourId, SpectraColors::text);
    setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
    setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
    setColour(juce::PopupMenu::backgroundColourId, SpectraColors::panel);
    setColour(juce::PopupMenu::textColourId, SpectraColors::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, SpectraColors::lavender.withAlpha(0.3f));
    setColour(juce::PopupMenu::highlightedTextColourId, SpectraColors::text);
    setColour(juce::ToggleButton::textColourId, SpectraColors::text);
    setColour(juce::ToggleButton::tickColourId, SpectraColors::mint);
    setColour(juce::ToggleButton::tickDisabledColourId, SpectraColors::textDim);
    setColour(juce::Label::textColourId, SpectraColors::textDim);
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
    g.setColour(SpectraColors::knobTrack);
    g.drawEllipse(rx, ry, rw, rw, arcThickness);

    // 1.5 モジュレーション・レンジ帯（値アークの下の白いハロー）
    //     GUI側が mod_active / mod_min / mod_max / mod_live プロパティを毎フレーム更新
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

            // 外側にソフトなグローを敷いてから本体を描く (暗い背景でも輪郭が立つ)
            g.setColour(SpectraColors::modRange.withAlpha(0.28f));
            g.strokePath(band, juce::PathStrokeType(arcThickness + 8.0f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(SpectraColors::modRange.withAlpha(0.95f));
            g.strokePath(band, juce::PathStrokeType(arcThickness + 3.5f,
                         juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // 2. 値アーク（セクション色ベースのパステルグラデーション）
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
    g.setColour(SpectraColors::text);
    g.fillPath(p2);

    // 5. ライブ変調ドット（変調後の現在値をアーク上の白点で表示）
    if (modActive)
    {
        const float live = juce::jlimit(0.0f, 1.0f, (float)props.getWithDefault("mod_live", sliderPos));
        const auto aLive = rotaryStartAngle + live * (rotaryEndAngle - rotaryStartAngle);
        const float dotX = centreX + std::sin(aLive) * radius;
        const float dotY = centreY - std::cos(aLive) * radius;
        g.setColour(SpectraColors::modRange.withAlpha(0.40f));
        g.fillEllipse(dotX - 6.0f, dotY - 6.0f, 12.0f, 12.0f);
        g.setColour(SpectraColors::modLive);
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);
    }
}
