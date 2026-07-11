#include "PluginProcessor.h"
#include "PluginEditor.h"

SPECTRA8AudioProcessorEditor::SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mMainPanel(p.apvts)
{
    addAndMakeVisible(mMainPanel);

    // ウィンドウサイズを 800 x 480 に設定
    setSize(800, 480);
}

void SPECTRA8AudioProcessorEditor::paint(juce::Graphics& g)
{
    // MainPanelがエディタ全体を完全に覆うため、ここでの描画は不要です
}

void SPECTRA8AudioProcessorEditor::resized()
{
    mMainPanel.setBounds(getLocalBounds());
}
