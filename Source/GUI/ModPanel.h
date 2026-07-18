// ==========================================
// File: ModPanel.h
// 「MOD MATRIX」タブ・パネル (Granular 準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"

class ModPanel : public juce::Component
{
public:
    ModPanel(juce::AudioProcessorValueTreeState& state);
    ~ModPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    // LFO / ENV 設定用コンポーネント (タブ式または並列表示)
    // 今回はコンパクトにLFO 1〜4、ENV 1〜3の設定を上部に並列配置

    struct LfoGui
    {
        juce::Slider rateSlider;
        juce::ToggleButton syncButton { "SYNC" };
        juce::ComboBox rateSyncCombo;
        juce::ComboBox waveCombo;
        juce::Label label;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rateSyncAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> waveAttach;
    };

    struct EnvGui
    {
        juce::Slider attackSlider;
        juce::Slider decaySlider;
        juce::Slider sustainSlider;
        juce::Slider releaseSlider;
        juce::ToggleButton loopButton { "LOOP" };
        juce::Label label;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> aAttach, dAttach, sAttach, rAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> loopAttach;
    };

    std::array<LfoGui, 4> mLfoGuis;
    std::array<EnvGui, 3> mEnvGuis;

    // 12スロットのモジュレーションマトリクス (2列×6行)
    struct SlotGui
    {
        juce::ComboBox srcCombo;
        juce::ComboBox dstCombo;
        juce::Slider amtSlider;
        juce::ToggleButton uniButton { "UNI" };

        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> dstAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amtAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> uniAttach;
    };

    std::array<SlotGui, 12> mSlotGuis;   // ModMatrix::kNumSlots と一致 (2列×6行)

    // ビューポート (スロットが画面に収まりきらない場合に備えてスクロール可能にする)
    juce::Viewport mViewport;
    juce::Component mSlotContainer;
};
