// ==========================================
// File: GlowToggle.h
// LED点灯式トグルボタン（ON時にアクセント色でグロー）
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class GlowToggle : public juce::ToggleButton
{
public:
    GlowToggle(const juce::String& text, juce::Colour accentColour)
        : juce::ToggleButton(text), accent(accentColour) {}

    void paintButton(juce::Graphics& g, bool highlighted, bool /*down*/) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        const bool on = getToggleState();

        // 背景
        g.setColour(on ? accent.withAlpha(0.16f)
                       : (highlighted ? GUI::ColorPalette::sliderTrack.brighter(0.15f) : GUI::ColorPalette::sliderTrack));
        g.fillRoundedRectangle(r, 6.0f);

        // 枠
        g.setColour(on ? accent.withAlpha(0.9f) : GUI::ColorPalette::panelBorder);
        g.drawRoundedRectangle(r, 6.0f, on ? 1.5f : 1.0f);

        // LEDインジケーター
        const float ledX = r.getX() + 11.0f;
        const float ledY = r.getCentreY();
        if (on)
        {
            g.setColour(accent.withAlpha(0.35f)); // グロー
            g.fillEllipse(ledX - 6.5f, ledY - 6.5f, 13.0f, 13.0f);
        }
        g.setColour(on ? accent : GUI::ColorPalette::textMuted.withAlpha(0.45f));
        g.fillEllipse(ledX - 3.0f, ledY - 3.0f, 6.0f, 6.0f);

        // テキスト
        g.setColour(on ? GUI::ColorPalette::textBody : GUI::ColorPalette::textMuted);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(getButtonText(), (int)ledX + 9, 0, getWidth() - (int)ledX - 11, getHeight(),
                   juce::Justification::centredLeft);
    }

private:
    juce::Colour accent;
};
