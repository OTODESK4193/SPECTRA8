#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "MainPanel.h"
#include "BandEditorPanel.h"

class SPECTRA8AudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer {
public:
    SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor&);
    ~SPECTRA8AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void updateTabVisibility();

    SPECTRA8AudioProcessor& audioProcessor;
    
    // タブ選択ボタン
    juce::TextButton mTabVocoderBtn;
    juce::TextButton mTabExcitationBtn;
    juce::TextButton mTabBandsEqBtn;

    int mActiveTab = 0; // 0: VOCODER, 1: EXCITATION, 2: BANDS EQ

    // タブパネル
    GUI::VocoderTabPanel mVocoderPanel;
    GUI::ExcitationTabPanel mExcitationPanel;
    GUI::BandEditorPanel mBandsEqPanel;

    // デバッグ情報表示用ラベル
    juce::Label mDebugLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessorEditor)
};
