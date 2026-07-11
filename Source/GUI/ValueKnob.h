// ==========================================
// File: ValueKnob.h
// 右クリックで数値を直接入力できるロータリースライダー
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class ValueKnob : public juce::Slider
{
public:
    ValueKnob() = default;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isRightButtonDown())
        {
            showTextEntry();
            return;
        }
        juce::Slider::mouseDown(e);
    }

private:
    void showTextEntry()
    {
        auto editor = std::make_unique<juce::TextEditor>();
        editor->setSize(96, 26);
        editor->setJustification(juce::Justification::centred);
        editor->setColour(juce::TextEditor::backgroundColourId, GUI::ColorPalette::panelBg);
        editor->setColour(juce::TextEditor::textColourId, GUI::ColorPalette::textBody);
        editor->setColour(juce::TextEditor::outlineColourId, GUI::ColorPalette::panelBorder);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, GUI::ColorPalette::neonGreen.withAlpha(0.7f));
        editor->setInputRestrictions(12, "0123456789.-");
        editor->setText(juce::String(getValue(), 3), juce::dontSendNotification);
        editor->setSelectAllWhenFocused(true);
        editor->setWantsKeyboardFocus(true);

        auto* edPtr = editor.get();
        auto& box = juce::CallOutBox::launchAsynchronously(std::move(editor),
                                                           getScreenBounds(), nullptr);

        edPtr->onReturnKey = [this, edPtr, &box]
        {
            const double v = edPtr->getText().getDoubleValue();
            setValue(v, juce::sendNotificationSync); // Sliderが自動でレンジにクランプ
            box.dismiss();
        };
        edPtr->onEscapeKey = [&box] { box.dismiss(); };
        edPtr->onFocusLost = [&box] { box.dismiss(); };

        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::TextEditor>(edPtr)]
        {
            if (safe != nullptr) safe->grabKeyboardFocus();
        });
    }
};
