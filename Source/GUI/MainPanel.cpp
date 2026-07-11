#include "MainPanel.h"

namespace GUI {

MainPanel::MainPanel(juce::AudioProcessorValueTreeState& state)
    : mState(state)
{
    // 1. ノブの追加 (LPC/フォルマントセクション)
    addDial("character", "CHARACTER");
    addDial("frameRate", "FRAME RATE");
    addDial("lpcOrder", "LPC ORDER");
    addDial("formantShift", "FMT SHIFT");
    addDial("formantStretch", "FMT STRETCH");
    addDial("tracking", "TRACKING");

    // 2. ノブの追加 (オシレーターセクション)
    addDial("wavetablePosition", "WT POS");
    addDial("pulseWidth", "PULSE WIDTH");
    addDial("detune", "DETUNE");
    addDial("noise", "NOISE MIX");

    // 3. ノブの追加 (ADSR & 音量)
    addDial("attack", "ATTACK");
    addDial("decay", "DECAY");
    addDial("sustain", "SUSTAIN");
    addDial("release", "RELEASE");
    addDial("mix", "MIX");
    addDial("outputLevel", "OUT LEVEL");

    // 4. 波形選択コンボボックスの追加
    addAndMakeVisible(mWaveformCombo);
    mWaveformCombo.addItem("Sawtooth", 1);
    mWaveformCombo.addItem("Pulse", 2);
    mWaveformCombo.addItem("Wavetable", 3);
    mWaveformCombo.setEditableText(false);
    mWaveformCombo.setJustificationType(juce::Justification::centred);
    
    // コンボボックスのカラー設定
    mWaveformCombo.setColour(juce::ComboBox::backgroundColourId, ColorPalette::panelBg);
    mWaveformCombo.setColour(juce::ComboBox::outlineColourId, ColorPalette::panelBorder);
    mWaveformCombo.setColour(juce::ComboBox::textColourId, ColorPalette::textBody);
    
    mWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "waveform", mWaveformCombo);

    mWaveformLabel.setText("EXCITATION", juce::dontSendNotification);
    mWaveformLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    mWaveformLabel.setJustificationType(juce::Justification::centred);
    mWaveformLabel.setColour(juce::Label::textColourId, ColorPalette::textBody);
    addAndMakeVisible(mWaveformLabel);

    // 5. 動作モードコンボボックスの追加
    addAndMakeVisible(mModeCombo);
    mModeCombo.addItem("Auto (Vocal)", 1);
    mModeCombo.addItem("MIDI Mode", 2);
    mModeCombo.setEditableText(false);
    mModeCombo.setJustificationType(juce::Justification::centred);
    
    mModeCombo.setColour(juce::ComboBox::backgroundColourId, ColorPalette::panelBg);
    mModeCombo.setColour(juce::ComboBox::outlineColourId, ColorPalette::panelBorder);
    mModeCombo.setColour(juce::ComboBox::textColourId, ColorPalette::textBody);
    
    mModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "mode", mModeCombo);

    mModeLabel.setText("MODE", juce::dontSendNotification);
    mModeLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    mModeLabel.setJustificationType(juce::Justification::centred);
    mModeLabel.setColour(juce::Label::textColourId, ColorPalette::textBody);
    addAndMakeVisible(mModeLabel);
}

void MainPanel::addDial(const juce::String& paramID, const juce::String& label)
{
    DialInfo info;
    info.paramID = paramID;
    info.label = label;

    info.slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    info.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 16);
    info.slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonCyan);
    info.slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    info.slider->setColour(juce::Slider::textBoxTextColourId, ColorPalette::textBody);
    addAndMakeVisible(*info.slider);

    info.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mState, paramID, *info.slider);

    mDials.push_back(std::move(info));
}

void MainPanel::paint(juce::Graphics& g)
{
    // 背景の描画
    g.fillAll(ColorPalette::background);

    // タイトルと各パネル境界の描画
    g.setColour(ColorPalette::panelBorder);
    
    // セクション境界
    g.drawRoundedRectangle(15.0f, 40.0f, 470.0f, 260.0f, 6.0f, 2.0f);
    g.setColour(ColorPalette::textHeader);
    g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    g.drawText("LPC / VOCAL FORMANT PROFILER", 25, 15, 300, 20, juce::Justification::left);

    // 励振源 (オシレーター) 枠
    g.setColour(ColorPalette::panelBorder);
    g.drawRoundedRectangle(500.0f, 40.0f, 285.0f, 260.0f, 6.0f, 2.0f);
    g.setColour(ColorPalette::textHeader);
    g.drawText("EXCITATION OSCILLATOR", 510, 15, 250, 20, juce::Justification::left);

    // ADSR / OUT 枠
    g.setColour(ColorPalette::panelBorder);
    g.drawRoundedRectangle(15.0f, 310.0f, 770.0f, 150.0f, 6.0f, 2.0f);
    g.setColour(ColorPalette::textHeader);
    g.drawText("ENVELOPE / OUTPUT MIX", 25, 315, 250, 20, juce::Justification::left);

    // ノブの上にラベルを描画
    g.setColour(ColorPalette::textMuted);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

    // LPCセクションのラベル
    for (int i = 0; i < 6; ++i)
    {
        auto bounds = mDials[i].slider->getBounds();
        g.drawText(mDials[i].label, bounds.getX(), bounds.getY() - 15, bounds.getWidth(), 12, juce::Justification::centred);
    }

    // オシレーターセクションのラベル
    for (int i = 6; i < 10; ++i)
    {
        auto bounds = mDials[i].slider->getBounds();
        g.drawText(mDials[i].label, bounds.getX(), bounds.getY() - 15, bounds.getWidth(), 12, juce::Justification::centred);
    }

    // ADSR / Volume セクションのラベル
    for (int i = 10; i < 16; ++i)
    {
        auto bounds = mDials[i].slider->getBounds();
        g.drawText(mDials[i].label, bounds.getX(), bounds.getY() - 15, bounds.getWidth(), 12, juce::Justification::centred);
    }
}

void MainPanel::resized()
{
    // LPC / フォルマント (Dials 0〜5)
    // 2行3列で配置
    int startX = 35;
    int startY = 70;
    int dialSize = 75;
    int spacingX = 65;
    int spacingY = 40;

    for (int i = 0; i < 6; ++i)
    {
        int row = i / 3;
        int col = i % 3;
        
        mDials[i].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonCyan);
        mDials[i].slider->setBounds(startX + col * (dialSize + spacingX), startY + row * (dialSize + spacingY + 16), dialSize, dialSize + 16);
    }

    // オシレーター
    mWaveformLabel.setBounds(520, 55, 120, 18);
    mWaveformCombo.setBounds(520, 75, 120, 24);

    mModeLabel.setBounds(650, 55, 120, 18);
    mModeCombo.setBounds(650, 75, 120, 24);

    // Dials 6, 7 (Wavetable Pos, Pulse Width)
    int oscDialSize = 58;
    mDials[6].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
    mDials[6].slider->setBounds(520, 115, oscDialSize, oscDialSize + 16);
    
    mDials[7].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
    mDials[7].slider->setBounds(660, 115, oscDialSize, oscDialSize + 16);

    // Dials 8, 9 (Detune, Noise Mix)
    mDials[8].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
    mDials[8].slider->setBounds(520, 205, oscDialSize, oscDialSize + 16);
    
    mDials[9].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
    mDials[9].slider->setBounds(660, 205, oscDialSize, oscDialSize + 16);

    // ADSR / Volume (Dials 10〜15)
    // 横一列に6個配置
    int adsrStartX = 30;
    int adsrStartY = 350;
    int adsrSpacingX = 49;

    for (int i = 10; i < 16; ++i)
    {
        int idx = i - 10;
        mDials[i].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPurple);
        mDials[i].slider->setBounds(adsrStartX + idx * (dialSize + adsrSpacingX), adsrStartY, dialSize, dialSize + 16);
    }
}

} // namespace GUI
