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
#include "GUI/FxPanel.h"
#include "GUI/HelpComboBox.h"   // MenuHelpBus (ポップアップ項目のヘルプ受け取り用)

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
    
    // タブ選択ボタン (5つ)
    juce::TextButton mTabVocoderBtn   { "VOCODER" };
    juce::TextButton mTabExcitationBtn { "EXCITATION" };
    juce::TextButton mTabModBtn        { "MOD MATRIX" };
    juce::TextButton mTabFxBtn         { "FX" };
    juce::TextButton mTabBandsEqBtn    { "BANDS EQ" };

    int mActiveTab = 0; // 0: VOCODER, 1: EXCITATION, 2: MOD, 3: FX, 4: BANDS EQ

    // タブパネルの実体
    VocoderPanel mVocoderPanel;
    ExcitationPanel mExcitationPanel;
    ModPanel mModPanel;
    FxPanel mFxPanel;
    BandsEqPanel mBandsEqPanel;

    // 下部ステータス行。通常はモード表示、マウスオーバー中はそのコントロールの英語説明。
    juce::Label mDebugLabel;
    bool mShowingHelp = false;   // 文字色切替のための現在状態

    // インフォバー本文の左余白。コンボのポップアップ(左端に出る)の幅より広く取り、
    // メニューを開いても本文が隠れないようにする。
    static constexpr int kInfoTextInset = 200;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessorEditor)
};
