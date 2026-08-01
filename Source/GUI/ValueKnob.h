// ==========================================
// File: ValueKnob.h
// 右クリックで数値を直接入力できるロータリースライダー（Granular 準拠）
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
        editor->setColour(juce::TextEditor::backgroundColourId, SpectraColors::panel);
        editor->setColour(juce::TextEditor::textColourId, SpectraColors::text);
        editor->setColour(juce::TextEditor::outlineColourId, SpectraColors::panelLine);
        editor->setColour(juce::TextEditor::focusedOutlineColourId, SpectraColors::mint.withAlpha(0.7f));
        editor->setInputRestrictions(12, "0123456789.-");
        editor->setText(juce::String(getValue(), 3), juce::dontSendNotification);
        editor->setSelectAllWhenFocused(true);
        editor->setWantsKeyboardFocus(true);

        auto* edPtr = editor.get();
        auto& boxRef = juce::CallOutBox::launchAsynchronously(std::move(editor),
                                                              getScreenBounds(), nullptr);

        // 【重要】CallOutBox は dismiss() で自分自身を delete する。
        //  生ポインタ(旧実装の &box キャプチャ)のままだと、
        //   onReturnKey → dismiss() で解放 → 瀕死のエディタから onFocusLost が発火
        //   → 解放済みメモリへ dismiss()  という use-after-free になり、
        //  Enter 確定時に確率的にクラッシュしていた。
        //  SafePointer で保持し、生きているときだけ dismiss する。
        juce::Component::SafePointer<juce::CallOutBox> box(&boxRef);

        auto closeBox = [box]
        {
            if (auto* b = box.getComponent())
                b->dismiss();
        };

        edPtr->onReturnKey = [this, edPtr, closeBox]
        {
            const double v = edPtr->getText().getDoubleValue();
            setValue(v, juce::sendNotificationSync); // Sliderが自動でレンジにクランプ
            closeBox();
        };
        edPtr->onEscapeKey = closeBox;
        edPtr->onFocusLost = closeBox;

        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<juce::TextEditor>(edPtr)]
        {
            if (safe != nullptr) safe->grabKeyboardFocus();
        });
    }
};
