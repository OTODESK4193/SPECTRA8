// ==========================================
// File: VocoderPanel.h
// 「VOCODER」タブ・パネル (Granular 準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ArcDial.h"

class VocoderPanel : public juce::Component
{
public:
    VocoderPanel(juce::AudioProcessorValueTreeState& state);
    ~VocoderPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // ノブ 11基
    ValueKnob mKnobCharacter;
    ValueKnob mKnobBands;
    ValueKnob mKnobFmtShift;
    ValueKnob mKnobFmtStretch;
    ValueKnob mKnobTracking;
    ValueKnob mKnobAttack;
    ValueKnob mKnobDecay;
    ValueKnob mKnobSustain;
    ValueKnob mKnobRelease;
    ValueKnob mKnobMix;
    ValueKnob mKnobOutLevel;
    ValueKnob mKnobPitchQuantize; // 新設: ケロケロ

    // コンボ 7種
    juce::ComboBox mComboVocoderMode;
    juce::ComboBox mComboVoicingMode;
    juce::ComboBox mComboLimiter;
    juce::ComboBox mComboAnalysisWindow;
    juce::ComboBox mComboLpcInterpolation;
    juce::ComboBox mComboFilterbankType;
    juce::ComboBox mComboLpcOrder;        // フェーズ2: LPC次数
    juce::ComboBox mComboFrameRate;       // フェーズ2 M5: フレームレート
    juce::ComboBox mComboQuantBits;       // フェーズ2 M5: k量子化ビット数

    // vocoderMode に応じた有効/無効表示 (LPC選択時のみ ORDER 等を有効化)
    void updateEnablement();

    // ボタン
    GlowToggle mBtnFormantFreeze;

    // ラベル
    juce::Label mLblCharacter { {}, "CHARACTER" };
    juce::Label mLblBands { {}, "BANDS" };
    juce::Label mLblFmtShift { {}, "FMT SHIFT" };
    juce::Label mLblFmtStretch { {}, "FMT STRETCH" };
    juce::Label mLblTracking { {}, "TRACKING" };
    juce::Label mLblAttack { {}, "ATTACK" };
    juce::Label mLblDecay { {}, "DECAY" };
    juce::Label mLblSustain { {}, "SUSTAIN" };
    juce::Label mLblRelease { {}, "RELEASE" };
    juce::Label mLblMix { {}, "MIX" };
    juce::Label mLblOutLevel { {}, "OUT LEVEL" };
    juce::Label mLblPitchQuantize { {}, "PITCH Q" }; // 新設: ケロケロ

    // アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentCharacter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentBands;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentFmtShift;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentFmtStretch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentTracking;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentAttack;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentDecay;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentSustain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentRelease;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentMix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentOutLevel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPitchQuantize; // 新設: ケロケロ

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentVocoderMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentVoicingMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentLimiter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentAnalysisWindow;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentLpcInterpolation;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentFilterbankType;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentLpcOrder;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentFrameRate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentQuantBits;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mAttachmentFormantFreeze;

    ArcDialLookAndFeel mArcLookAndFeel;
};
