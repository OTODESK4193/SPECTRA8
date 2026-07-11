#include "PluginProcessor.h"
#include "PluginEditor.h"

SPECTRA8AudioProcessorEditor::SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mMainPanel(p.apvts)
{
    addAndMakeVisible(mMainPanel);

    // デバッグ用ラベルの設定
    addAndMakeVisible(mDebugLabel);
    mDebugLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.8f));
    mDebugLabel.setColour(juce::Label::textColourId, juce::Colours::red.brighter());
    mDebugLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    mDebugLabel.setJustificationType(juce::Justification::centred);
    mDebugLabel.setText("No errors. Running fine.", juce::dontSendNotification);
    mDebugLabel.setInterceptsMouseClicks(false, false); // ラベルがノブのクリックを妨げないようにする

    // 100msごとにプロセッサのエラーメッセージを確認するタイマーを起動
    startTimer(100);

    // ウィンドウサイズを 800 x 480 に設定
    setSize(800, 480);
}

SPECTRA8AudioProcessorEditor::~SPECTRA8AudioProcessorEditor()
{
    stopTimer();
}

void SPECTRA8AudioProcessorEditor::paint(juce::Graphics& g)
{
    // MainPanelがエディタ全体をカバーします
}

void SPECTRA8AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    mDebugLabel.setBounds(bounds.removeFromBottom(30)); // 下部30ピクセルをデバッグ領域に
    mMainPanel.setBounds(bounds);
}

void SPECTRA8AudioProcessorEditor::timerCallback()
{
    // プロセッサから最新のエラーメッセージを取得して表示を更新
    mDebugLabel.setText(audioProcessor.getDebugMessage(), juce::dontSendNotification);
}
