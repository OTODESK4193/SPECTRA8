// ==========================================
// File: ExcitationPanel.h
// 「EXCITATION」タブ・パネル (Granular 準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "ArcDial.h"

class ExcitationPanel : public juce::Component
{
public:
    ExcitationPanel(juce::AudioProcessorValueTreeState& state);
    ~ExcitationPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // ノブ 6基
    ValueKnob mKnobWtPos;
    ValueKnob mKnobPulseWidth;
    ValueKnob mKnobDetune;
    ValueKnob mKnobNoise;
    ValueKnob mKnobLofi;
    ValueKnob mKnobPorta;

    // コンボ 1種
    juce::ComboBox mComboWaveform;

    // ラベル
    juce::Label mLblWtPos { {}, "WT POS" };
    juce::Label mLblPulseWidth { {}, "PULSE WIDTH" };
    juce::Label mLblDetune { {}, "DETUNE" };
    juce::Label mLblNoise { {}, "NOISE MIX" };
    juce::Label mLblLofi { {}, "LOFI" };
    juce::Label mLblPorta { {}, "PORTA" };

    // アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentWtPos;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPulseWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentDetune;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentNoise;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentLofi;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPorta;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentWaveform;

    ArcDialLookAndFeel mArcLookAndFeel;
};
