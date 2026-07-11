#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "GUI/ColorPalette.h"

SPECTRA8AudioProcessorEditor::SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mVocoderPanel(p.apvts),
      mExcitationPanel(p.apvts),
      mBandsEqPanel(p)
{
    // 各パネルを子コンポーネントとして追加
    addChildComponent(mVocoderPanel);
    addChildComponent(mExcitationPanel);
    addChildComponent(mBandsEqPanel);

    // タブボタン設定
    auto setupTabButton = [this](juce::TextButton& btn, const juce::String& text, int tabIdx)
    {
        addAndMakeVisible(btn);
        btn.setButtonText(text);
        btn.setRadioGroupId(1001);
        btn.setClickingTogglesState(true);
        btn.setColour(juce::TextButton::textColourOffId, GUI::ColorPalette::textMuted);
        btn.setColour(juce::TextButton::buttonOnColourId, GUI::ColorPalette::panelBg);
        btn.setColour(juce::TextButton::buttonColourId, GUI::ColorPalette::background);
        btn.setColour(juce::ComboBox::outlineColourId, GUI::ColorPalette::panelBorder);

        btn.onClick = [this, tabIdx]
        {
            mActiveTab = tabIdx;
            updateTabVisibility();
            resized();
        };
    };

    setupTabButton(mTabVocoderBtn, "VOCODER", 0);
    setupTabButton(mTabExcitationBtn, "EXCITATION", 1);
    setupTabButton(mTabBandsEqBtn, "BANDS EQ", 2);

    // デフォルトでVOCODERを選択状態にする
    mTabVocoderBtn.setToggleState(true, juce::sendNotificationSync);

    // デバッグ用ラベルの設定
    addAndMakeVisible(mDebugLabel);
    mDebugLabel.setColour(juce::Label::backgroundColourId, juce::Colours::black.withAlpha(0.8f));
    mDebugLabel.setColour(juce::Label::textColourId, juce::Colours::red.brighter());
    mDebugLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    mDebugLabel.setJustificationType(juce::Justification::centred);
    mDebugLabel.setText("No errors. Running fine.", juce::dontSendNotification);
    mDebugLabel.setInterceptsMouseClicks(false, false);

    startTimer(100);
    setSize(800, 480);
}

SPECTRA8AudioProcessorEditor::~SPECTRA8AudioProcessorEditor()
{
    stopTimer();
}

void SPECTRA8AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(GUI::ColorPalette::background);

    // タブボタンの下に仕切り線を描画
    g.setColour(GUI::ColorPalette::panelBorder);
    g.drawHorizontalLine(36, 0.0f, static_cast<float>(getWidth()));
}

void SPECTRA8AudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    mDebugLabel.setBounds(bounds.removeFromBottom(30)); // 下部30ピクセルはデバッグ表示

    // 上部36ピクセルをタブボタン領域に
    auto tabArea = bounds.removeFromTop(36);
    int tabW = getWidth() / 3;
    mTabVocoderBtn.setBounds(tabArea.removeFromLeft(tabW));
    mTabExcitationBtn.setBounds(tabArea.removeFromLeft(tabW));
    mTabBandsEqBtn.setBounds(tabArea);

    // 残りの領域にアクティブなパネルを配置
    mVocoderPanel.setBounds(bounds);
    mExcitationPanel.setBounds(bounds);
    mBandsEqPanel.setBounds(bounds);
}

void SPECTRA8AudioProcessorEditor::timerCallback()
{
    mDebugLabel.setText(audioProcessor.getDebugMessage(), juce::dontSendNotification);
}

void SPECTRA8AudioProcessorEditor::updateTabVisibility()
{
    mVocoderPanel.setVisible(mActiveTab == 0);
    mExcitationPanel.setVisible(mActiveTab == 1);
    mBandsEqPanel.setVisible(mActiveTab == 2);

    // ボタンのテキスト色をテーマに合わせて更新
    mTabVocoderBtn.setColour(juce::TextButton::textColourOnId, GUI::ColorPalette::lavender);
    mTabExcitationBtn.setColour(juce::TextButton::textColourOnId, GUI::ColorPalette::pink);
    mTabBandsEqBtn.setColour(juce::TextButton::textColourOnId, GUI::ColorPalette::mint);
}
