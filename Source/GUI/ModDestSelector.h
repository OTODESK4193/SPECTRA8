// ==========================================
// File: ModDestSelector.h
// MODマトリクスの Destination 選択用コントロール (PicoSampler 準拠)
// 通常の ComboBox の代わりに、クリックすると系統別 (Vocoder / Excitation / FX) に
// 分類されたツリー状の PopupMenu を表示する。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <memory>
#include "ColorPalette.h"
#include "../DSP/ModMatrix.h"

class ModDestSelector : public juce::Component,
                        public juce::SettableTooltipClient
{
public:
    ModDestSelector() = default;
    ~ModDestSelector() override = default;

    void setTooltip(const juce::String& newTooltip) override { mTooltip = newTooltip; }
    juce::String getTooltip() override                      { return mTooltip; }

    std::function<juce::PopupMenu(int currentDst)> buildMenu;

    void bindTo(juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        attachment.reset();
        param = state.getParameter(paramID);

        if (param == nullptr)
        {
            jassertfalse;
            return;
        }

        attachment = std::make_unique<juce::ParameterAttachment>(
            *param,
            [this](float newDenormalisedValue) { updateFromValue(newDenormalisedValue); },
            nullptr);

        attachment->sendInitialUpdate();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();

        g.setColour(SpectraColors::panel);
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(isMouseOver() ? SpectraColors::mint : SpectraColors::knobTrack);
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);

        auto textArea = b.reduced(6.0f, 0.0f);
        textArea.removeFromRight(14.0f);

        g.setColour(SpectraColors::text);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(currentText, textArea, juce::Justification::centredLeft, true);

        // 小さい下向き矢印
        g.setColour(SpectraColors::mint);
        const float ax = b.getRight() - 10.0f;
        const float ay = b.getCentreY();
        juce::Path arrow;
        arrow.addTriangle(ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 3.0f);
        g.fillPath(arrow);
    }

    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&)  override { repaint(); }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (buildMenu == nullptr || attachment == nullptr) return;

        juce::PopupMenu menu = buildMenu(currentDst);
        juce::Component::SafePointer<ModDestSelector> safeThis(this);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this),
            [safeThis](int result)
            {
                if (safeThis == nullptr || result <= 0) return;
                if (safeThis->attachment == nullptr) return;

                safeThis->attachment->setValueAsCompleteGesture((float)(result - 1));
            });
    }

private:
    void updateFromValue(float denormalisedValue)
    {
        const int dst = juce::roundToInt(denormalisedValue);
        static const auto names = ModMatrix::getDestNames();

        currentDst  = (dst >= 0 && dst < names.size()) ? dst : 0;
        currentText = names[currentDst];
        repaint();
    }

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    int currentDst = 0;
    juce::String currentText { "None" };
    juce::String mTooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModDestSelector)
};
