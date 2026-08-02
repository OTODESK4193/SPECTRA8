// ==========================================
// File: VocoderPanel.h
// 「VOCODER」タブ・パネル (Granular 準拠)
//  - コンボは左1列に集約（幅を短縮）。VOCODER MODE でモード別に表示切替。
//  - ノブエリアは上下二段（上=2行 / 下=1行のADSR+MIX+OUT+ボタン）。
//  - Limiter / Freeze は点灯式トグル(GlowToggle)。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <utility>
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "HelpComboBox.h"
#include "GlowToggle.h"
#include "ArcDial.h"
#include "ModRing.h"
#include "../DSP/ScaleSnap.h"

class SPECTRA8AudioProcessor;

class VocoderPanel : public juce::Component,
                     private juce::Timer
{
public:
    explicit VocoderPanel(SPECTRA8AudioProcessor& proc);
    ~VocoderPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // 30Hzで各ノブへ変調レンジ帯とライブ位置を反映
    void timerCallback() override;

    SPECTRA8AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // --- ノブ 16基 ---
    // 上段ノブエリア（共通 + EXCITATIONから移設した4種 + BANDS(FB専用)）
    ValueKnob mKnobCharacter;
    ValueKnob mKnobTracking;
    ValueKnob mKnobPitchQuantize;
    ValueKnob mKnobFmtShift;
    ValueKnob mKnobFmtStretch;
    ValueKnob mKnobLofi;        // 移設: EXCITATION → VOCODER
    ValueKnob mKnobBasePitch;   // 移設
    ValueKnob mKnobNoiseColor;  // 移設
    ValueKnob mKnobNoise;       // 移設 (NOISE MIX)
    ValueKnob mKnobBands;       // FilterBank専用
    ValueKnob mKnobResonance;   // FilterBank専用 (BPF Bankバンド幅スケール)
    ValueKnob mKnobWidth;       // FilterBank専用 (帯域交互パンニング幅)
    // 下段ノブエリア（ADSR + MIX + OUT）
    ValueKnob mKnobAttack;
    ValueKnob mKnobDecay;
    ValueKnob mKnobSustain;
    ValueKnob mKnobRelease;
    ValueKnob mKnobMix;
    ValueKnob mKnobMasterPitch;   // MIXとOUTの間。PitchQ併用でスケールにスナップする移調
    ValueKnob mKnobAir;
    ValueKnob mKnobOutLevel;

    // --- コンボ 8種 (左1列) ---
    HelpComboBox mComboVocoderMode;
    HelpComboBox mComboVoicingMode;
    HelpComboBox mComboTrackResponse;   // Tracking応答速度 (両モード共通)
    HelpComboBox mComboPitchQKey;       // PITCH Q キー (両モード共通)
    HelpComboBox mComboPitchQScale;     // PITCH Q スケール (両モード共通)
    HelpComboBox mComboLpcOrder;        // LPC専用
    HelpComboBox mComboAnalysisWindow;  // LPC専用
    HelpComboBox mComboFrameRate;       // LPC専用
    HelpComboBox mComboQuantBits;       // LPC専用
    HelpComboBox mComboLpcInterpolation;// LPC専用

    // --- 点灯式トグルボタン ---
    GlowToggle mBtnLimiter;         // LIMIT (Outノブ近く)
    GlowToggle mBtnFormantFreeze;   // FREEZE (LPC専用)

    // vocoderMode に応じた表示切替 + 再レイアウト
    void updateEnablement();

    // --- ラベル ---
    juce::Label mLblCharacter { {}, "CHARACTER" };
    juce::Label mLblTracking { {}, "TRACKING" };
    juce::Label mLblPitchQuantize { {}, "PITCH Q" };
    juce::Label mLblFmtShift { {}, "FMT SHIFT" };
    juce::Label mLblFmtStretch { {}, "FMT STRETCH" };
    juce::Label mLblLofi { {}, "LOFI" };
    juce::Label mLblBasePitch { {}, "BASE PITCH" };
    juce::Label mLblNoiseColor { {}, "NOISE COLOR" };
    juce::Label mLblNoise { {}, "NOISE MIX" };
    juce::Label mLblBands { {}, "BANDS" };
    juce::Label mLblResonance { {}, "RESONANCE" };
    juce::Label mLblWidth { {}, "WIDTH" };
    juce::Label mLblAttack { {}, "ATTACK" };
    juce::Label mLblDecay { {}, "DECAY" };
    juce::Label mLblSustain { {}, "SUSTAIN" };
    juce::Label mLblRelease { {}, "RELEASE" };
    juce::Label mLblMix { {}, "MIX" };
    juce::Label mLblAir { {}, "AIR" };
    juce::Label mLblMasterPitch { {}, "M.PITCH" };
    juce::Label mLblOutLevel { {}, "OUT LEVEL" };

    // --- アタッチメント ---
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentCharacter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentTracking;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPitchQuantize;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentFmtShift;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentFmtStretch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentLofi;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentBasePitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentNoiseColor;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentNoise;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentBands;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentResonance;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentAttack;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentDecay;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentSustain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentRelease;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentMix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentAir;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentMasterPitch;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentOutLevel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentVocoderMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentVoicingMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentTrackResponse;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentPitchQKey;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentPitchQScale;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentLpcOrder;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentAnalysisWindow;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentFrameRate;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentQuantBits;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentLpcInterpolation;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mAttachmentLimiter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mAttachmentFormantFreeze;

    ArcDialLookAndFeel mArcLookAndFeel;
};
