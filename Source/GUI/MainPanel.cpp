#include "MainPanel.h"

namespace GUI {

// ============================================================================
//  VocoderTabPanel Implementation
// ============================================================================

VocoderTabPanel::VocoderTabPanel(juce::AudioProcessorValueTreeState& state)
    : mState(state)
{
    // 1. ノブの追加
    // ボコーダーパラメータ (lavender)
    addDial("character", "CHARACTER", ColorPalette::lavender);
    addDial("bandCount", "BANDS", ColorPalette::lavender);
    addDial("formantShift", "FMT SHIFT", ColorPalette::lavender);
    addDial("formantStretch", "FMT STRETCH", ColorPalette::lavender);
    addDial("tracking", "TRACKING", ColorPalette::lavender);

    // ADSR (peach)
    addDial("attack", "ATTACK", ColorPalette::peach);
    addDial("decay", "DECAY", ColorPalette::peach);
    addDial("sustain", "SUSTAIN", ColorPalette::peach);
    addDial("release", "RELEASE", ColorPalette::peach);

    // マスター出力 (rose)
    addDial("mix", "MIX", ColorPalette::rose);
    addDial("outputLevel", "OUT LEVEL", ColorPalette::rose);

    // 2. ボコーダーモード ComboBox
    addAndMakeVisible(mVocoderModeCombo);
    mVocoderModeCombo.addItem("Filterbank", 1);
    mVocoderModeCombo.addItem("LPC Mode", 2);
    mVocoderModeCombo.setEditableText(false);
    mVocoderModeCombo.setJustificationType(juce::Justification::centred);
    mVocoderModeCombo.setLookAndFeel(&mArcLookAndFeel);
    mVocoderModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "vocoderMode", mVocoderModeCombo);

    mVocoderModeLabel.setText("LPC / FILTERBANK", juce::dontSendNotification);
    mVocoderModeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mVocoderModeLabel.setJustificationType(juce::Justification::centred);
    mVocoderModeLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mVocoderModeLabel);

    // 3. 動作モード ComboBox
    addAndMakeVisible(mModeCombo);
    mModeCombo.addItem("Auto (Vocal)", 1);
    mModeCombo.addItem("MIDI Mode", 2);
    mModeCombo.setEditableText(false);
    mModeCombo.setJustificationType(juce::Justification::centred);
    mModeCombo.setLookAndFeel(&mArcLookAndFeel);
    mModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "mode", mModeCombo);

    mModeLabel.setText("VOICING MODE", juce::dontSendNotification);
    mModeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mModeLabel.setJustificationType(juce::Justification::centred);
    mModeLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mModeLabel);

    // 4. リミッター ComboBox
    addAndMakeVisible(mLimiterCombo);
    mLimiterCombo.addItem("Limiter Off", 1);
    mLimiterCombo.addItem("Limiter On", 2);
    mLimiterCombo.setEditableText(false);
    mLimiterCombo.setJustificationType(juce::Justification::centred);
    mLimiterCombo.setLookAndFeel(&mArcLookAndFeel);
    mLimiterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "limiterEnable", mLimiterCombo);

    mLimiterLabel.setText("LIMITER", juce::dontSendNotification);
    mLimiterLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mLimiterLabel.setJustificationType(juce::Justification::centred);
    mLimiterLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mLimiterLabel);

    // 5. Window Type ComboBox
    addAndMakeVisible(mWindowTypeCombo);
    mWindowTypeCombo.addItem("Hann Window", 1);
    mWindowTypeCombo.addItem("Hamming Window", 2);
    mWindowTypeCombo.addItem("Blackman Window", 3);
    mWindowTypeCombo.setEditableText(false);
    mWindowTypeCombo.setJustificationType(juce::Justification::centred);
    mWindowTypeCombo.setLookAndFeel(&mArcLookAndFeel);
    mWindowTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "windowType", mWindowTypeCombo);

    mWindowTypeLabel.setText("ANALYSIS WINDOW", juce::dontSendNotification);
    mWindowTypeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mWindowTypeLabel.setJustificationType(juce::Justification::centred);
    mWindowTypeLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mWindowTypeLabel);

    // 6. Interpolation Mode ComboBox
    addAndMakeVisible(mInterpolationModeCombo);
    mInterpolationModeCombo.addItem("LSP Interpolate", 1);
    mInterpolationModeCombo.addItem("LAR Interpolate", 2);
    mInterpolationModeCombo.setEditableText(false);
    mInterpolationModeCombo.setJustificationType(juce::Justification::centred);
    mInterpolationModeCombo.setLookAndFeel(&mArcLookAndFeel);
    mInterpolationModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "interpolationMode", mInterpolationModeCombo);

    mInterpolationModeLabel.setText("LPC INTERPOLATION", juce::dontSendNotification);
    mInterpolationModeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mInterpolationModeLabel.setJustificationType(juce::Justification::centred);
    mInterpolationModeLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mInterpolationModeLabel);

    // 7. Filterbank Type ComboBox
    addAndMakeVisible(mFilterbankTypeCombo);
    mFilterbankTypeCombo.addItem("BPF Bank", 1);
    mFilterbankTypeCombo.addItem("Subtractive LR4", 2);
    mFilterbankTypeCombo.setEditableText(false);
    mFilterbankTypeCombo.setJustificationType(juce::Justification::centred);
    mFilterbankTypeCombo.setLookAndFeel(&mArcLookAndFeel);
    mFilterbankTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "filterbankType", mFilterbankTypeCombo);

    mFilterbankTypeLabel.setText("FILTERBANK TYPE", juce::dontSendNotification);
    mFilterbankTypeLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    mFilterbankTypeLabel.setJustificationType(juce::Justification::centred);
    mFilterbankTypeLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mFilterbankTypeLabel);
}

void VocoderTabPanel::addDial(const juce::String& paramID, const juce::String& label, juce::Colour fillColour)
{
    DialInfo info;
    info.paramID = paramID;
    info.label = label;

    info.slider = std::make_unique<ValueKnob>();
    info.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    info.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 15);
    info.slider->setColour(juce::Slider::rotarySliderFillColourId, fillColour);
    info.slider->setLookAndFeel(&mArcLookAndFeel);
    addAndMakeVisible(*info.slider);

    info.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mState, paramID, *info.slider);

    mDials.push_back(std::move(info));
}

void VocoderTabPanel::paint(juce::Graphics& g)
{
    g.fillAll(ColorPalette::background);

    g.setColour(ColorPalette::panelBorder);

    // 左セクション: PROFILER & FILTER 枠
    g.drawRoundedRectangle(15.0f, 15.0f, 370.0f, 395.0f, 6.0f, 1.5f);
    g.setColour(ColorPalette::textHeader);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("VOCAL PROFILE & ANALYZER", 25, 25, 250, 18, juce::Justification::left);

    // 右上セクション: ADSR 枠
    g.setColour(ColorPalette::panelBorder);
    g.drawRoundedRectangle(400.0f, 15.0f, 385.0f, 180.0f, 6.0f, 1.5f);
    g.setColour(ColorPalette::textHeader);
    g.drawText("ENVELOPE (MIDI MODE)", 410, 25, 250, 18, juce::Justification::left);

    // 右下セクション: MASTER OUT 枠
    g.setColour(ColorPalette::panelBorder);
    g.drawRoundedRectangle(400.0f, 210.0f, 385.0f, 200.0f, 6.0f, 1.5f);
    g.setColour(ColorPalette::textHeader);
    g.drawText("MASTER OUTPUT", 410, 220, 250, 18, juce::Justification::left);

    // ラベルの描画 (ノブの上部)
    g.setColour(ColorPalette::textMuted);
    g.setFont(juce::Font(10.0f, juce::Font::bold));

    for (size_t i = 0; i < mDials.size(); ++i)
    {
        auto bounds = mDials[i].slider->getBounds();
        g.drawText(mDials[i].label, bounds.getX(), bounds.getY() - 15, bounds.getWidth(), 12, juce::Justification::centred);
    }
}

void VocoderTabPanel::resized()
{
    // 左セクション: コンボボックス配置
    mVocoderModeLabel.setBounds(30, 55, 160, 16);
    mVocoderModeCombo.setBounds(30, 75, 160, 24);

    mModeLabel.setBounds(210, 55, 160, 16);
    mModeCombo.setBounds(210, 75, 160, 24);

    // 左セクション: ノブ配置 (Dials 0〜4)
    int startX = 35;
    int startY = 140;
    int dialSize = 75;
    int spacingX = 40;
    int spacingY = 45;

    // 行1: 3つ (character, bandCount, formantShift)
    for (int i = 0; i < 3; ++i)
    {
        mDials[i].slider->setBounds(startX + i * (dialSize + spacingX), startY, dialSize, dialSize + 15);
    }
    // 行2: 2つ (formantStretch, tracking)
    for (int i = 3; i < 5; ++i)
    {
        int col = i - 3;
        mDials[i].slider->setBounds(startX + 55 + col * (dialSize + spacingX + 15), startY + dialSize + spacingY, dialSize, dialSize + 15);
    }

    // 右上セクション: ADSRノブ配置 (Dials 5〜8)
    int adsrStartX = 415;
    int adsrStartY = 80;
    int adsrSpacingX = 18;
    for (int i = 5; i < 9; ++i)
    {
        int col = i - 5;
        mDials[i].slider->setBounds(adsrStartX + col * (dialSize + adsrSpacingX), adsrStartY, dialSize, dialSize + 15);
    }

    // 右下セクション: マスターコンボ＆ノブ配置 (Dials 9〜10)
    // カラム1 (X=415, 幅=155)
    mLimiterLabel.setBounds(415, 235, 155, 16);
    mLimiterCombo.setBounds(415, 255, 155, 24);

    mFilterbankTypeLabel.setBounds(415, 290, 155, 16);
    mFilterbankTypeCombo.setBounds(415, 310, 155, 24);

    mWindowTypeLabel.setBounds(415, 345, 155, 16);
    mWindowTypeCombo.setBounds(415, 365, 155, 24);

    // カラム2 (X=585, 幅=180)
    mInterpolationModeLabel.setBounds(585, 235, 180, 16);
    mInterpolationModeCombo.setBounds(585, 255, 180, 24);

    int masterStartX = 585;
    int masterStartY = 300;
    mDials[9].slider->setBounds(masterStartX, masterStartY, dialSize, dialSize + 15);         // mix
    mDials[10].slider->setBounds(masterStartX + dialSize + 15, masterStartY, dialSize, dialSize + 15); // outputLevel
}


// ============================================================================
//  ExcitationTabPanel Implementation
// ============================================================================

ExcitationTabPanel::ExcitationTabPanel(juce::AudioProcessorValueTreeState& state)
    : mState(state)
{
    // 1. ノブの追加 (カラーは pink)
    addDial("wavetablePosition", "WT POS", ColorPalette::pink);
    addDial("pulseWidth", "PULSE WIDTH", ColorPalette::pink);
    addDial("detune", "DETUNE", ColorPalette::pink);
    addDial("noise", "NOISE MIX", ColorPalette::pink);

    // 2. 波形選択 ComboBox
    addAndMakeVisible(mWaveformCombo);
    mWaveformCombo.addItem("Sawtooth", 1);
    mWaveformCombo.addItem("Pulse", 2);
    mWaveformCombo.addItem("Wavetable", 3);
    mWaveformCombo.setEditableText(false);
    mWaveformCombo.setJustificationType(juce::Justification::centred);
    mWaveformCombo.setLookAndFeel(&mArcLookAndFeel);
    mWaveformAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        mState, "waveform", mWaveformCombo);

    mWaveformLabel.setText("EXCITATION WAVEFORM", juce::dontSendNotification);
    mWaveformLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    mWaveformLabel.setJustificationType(juce::Justification::centred);
    mWaveformLabel.setColour(juce::Label::textColourId, ColorPalette::textMuted);
    addAndMakeVisible(mWaveformLabel);
}

void ExcitationTabPanel::addDial(const juce::String& paramID, const juce::String& label, juce::Colour fillColour)
{
    DialInfo info;
    info.paramID = paramID;
    info.label = label;

    info.slider = std::make_unique<ValueKnob>();
    info.slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    info.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 15);
    info.slider->setColour(juce::Slider::rotarySliderFillColourId, fillColour);
    info.slider->setLookAndFeel(&mArcLookAndFeel);
    addAndMakeVisible(*info.slider);

    info.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        mState, paramID, *info.slider);

    mDials.push_back(std::move(info));
}

void ExcitationTabPanel::paint(juce::Graphics& g)
{
    g.fillAll(ColorPalette::background);

    g.setColour(ColorPalette::panelBorder);

    // 中央パネル枠
    g.drawRoundedRectangle(150.0f, 20.0f, 500.0f, 380.0f, 6.0f, 1.5f);
    g.setColour(ColorPalette::textHeader);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("EXCITATION ENGINE", 170, 35, 300, 18, juce::Justification::left);

    // ラベルの描画
    g.setColour(ColorPalette::textMuted);
    g.setFont(juce::Font(10.0f, juce::Font::bold));

    for (size_t i = 0; i < mDials.size(); ++i)
    {
        auto bounds = mDials[i].slider->getBounds();
        g.drawText(mDials[i].label, bounds.getX(), bounds.getY() - 15, bounds.getWidth(), 12, juce::Justification::centred);
    }
}

void ExcitationTabPanel::resized()
{
    // コンボボックス配置 (上部)
    mWaveformLabel.setBounds(250, 70, 300, 16);
    mWaveformCombo.setBounds(300, 92, 200, 24);

    // ノブ配置 (Dials 0〜3)
    int startX = 230;
    int startY = 160;
    int dialSize = 90;
    int spacingX = 160;
    int spacingY = 50;

    // 行1: WT POS, PULSE WIDTH
    mDials[0].slider->setBounds(startX, startY, dialSize, dialSize + 15);
    mDials[1].slider->setBounds(startX + spacingX, startY, dialSize, dialSize + 15);

    // 行2: DETUNE, NOISE MIX
    mDials[2].slider->setBounds(startX, startY + dialSize + spacingY, dialSize, dialSize + 15);
    mDials[3].slider->setBounds(startX + spacingX, startY + dialSize + spacingY, dialSize, dialSize + 15);
}

} // namespace GUI