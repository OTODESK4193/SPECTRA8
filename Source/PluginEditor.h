// ==========================================
// File: PluginEditor.h
// SPECTRA8 エディター層 (4タブ + HUD / Granular 準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GUI/VocoderPanel.h"
#include "GUI/ExcitationPanel.h"
#include "GUI/ModPanel.h"
#include "GUI/BandsEqPanel.h"

class SPECTRA8AudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer 
{
public:
    SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor&);
    ~SPECTRA8AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void selectTab(int tabIndex);

    SPECTRA8AudioProcessor& audioProcessor;
    
    // タブ選択ボタン (4つ)
    juce::TextButton mTabVocoderBtn   { "VOCODER" };
    juce::TextButton mTabExcitationBtn { "EXCITATION" };
    juce::TextButton mTabModBtn        { "MOD MATRIX" };
    juce::TextButton mTabBandsEqBtn    { "BANDS EQ" };

    int mActiveTab = 0; // 0: VOCODER, 1: EXCITATION, 2: MOD, 3: BANDS EQ

    // タブパネルの実体
    VocoderPanel mVocoderPanel;
    ExcitationPanel mExcitationPanel;
    ModPanel mModPanel;
    BandsEqPanel mBandsEqPanel;

    // HUD (デバッグ・ステータス表示用)
    juce::Label mDebugLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessorEditor)
};
