// ==========================================
// File: ModPanel.h
// 「MOD MATRIX」タブ・パネル (Granular 準拠)
//  - 上段: LFO / ENV のサブタブ切替 (マクロは廃止)
//      LFO ×3 : Wave / Sync / Rate / Sync Rate
//      ENV ×2 : A D S R + Loop
//  - 下段: 6スロット (Source → Dest / Amount / Uni)
//  変調先ノブ側の「レンジ帯 + ライブ位置」表示は GUI/ModRing.h が担当。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>
#include "../DSP/ModMatrix.h"
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "GlowToggle.h"
#include "ArcDial.h"

class ModPanel : public juce::Component
{
public:
    explicit ModPanel(juce::AudioProcessorValueTreeState& state);
    ~ModPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void setSourceTab(int t);          // 0 = LFO, 1 = ENV
    void styleTab(juce::TextButton& b, bool active);
    void setupKnob(ValueKnob& k, const juce::String& paramID);
    void setupCombo(juce::ComboBox& c, const juce::StringArray& items, const juce::String& paramID);
    void setupToggle(std::unique_ptr<GlowToggle>& b, const juce::String& text,
                     juce::Colour accent, const juce::String& paramID);
    void setupSmallLabel(juce::Label& l, bool bold);

    juce::AudioProcessorValueTreeState& apvts;

    // --- ソース・サブタブ ---
    juce::TextButton mLfoTabBtn { "LFO" };
    juce::TextButton mEnvTabBtn { "ENV" };
    int mActiveSrcTab = 0;

    // --- LFO ×3 ---
    struct LfoGui
    {
        juce::Label label;
        juce::ComboBox waveBox;
        juce::ComboBox syncRateBox;
        ValueKnob rateKnob;
        juce::Label rateLbl { {}, "RATE" };
        std::unique_ptr<GlowToggle> syncBtn;
    };
    std::array<LfoGui, ModMatrix::kNumLfos> mLfos;

    // --- ENV ×2 ---
    struct EnvGui
    {
        juce::Label label;
        ValueKnob a, d, s, r;
        juce::Label la { {}, "A" }, ld { {}, "D" }, ls { {}, "S" }, lr { {}, "R" };
        std::unique_ptr<GlowToggle> loopBtn;
    };
    std::array<EnvGui, ModMatrix::kNumEnvs> mEnvs;

    // --- スロット ×6 ---
    struct SlotGui
    {
        juce::Label rowLabel;
        juce::ComboBox srcBox;
        juce::ComboBox dstBox;
        ValueKnob amtKnob;
        std::unique_ptr<GlowToggle> uniBtn;
    };
    std::array<SlotGui, ModMatrix::kNumSlots> mSlots;
    juce::Label mSlotHdr { {}, "SOURCE                    DESTINATION                 AMT" };

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>   mSliderAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> mComboAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>   mButtonAttach;

    ArcDialLookAndFeel mArcLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModPanel)
};
