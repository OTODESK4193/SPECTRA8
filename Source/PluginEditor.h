#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "MainPanel.h"

class SPECTRA8AudioProcessorEditor : public juce::AudioProcessorEditor,
                                     public juce::Timer {
public:
    SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor&);
    ~SPECTRA8AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    SPECTRA8AudioProcessor& audioProcessor;
    
    // メイン操作パネル
    GUI::MainPanel mMainPanel;

    // デバッグ情報表示用ラベル
    juce::Label mDebugLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessorEditor)
};
