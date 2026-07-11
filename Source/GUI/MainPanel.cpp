#include "MainPanel.h"

namespace GUI {

    MainPanel::MainPanel(juce::AudioProcessorValueTreeState& state)
        : mState(state)
    {
        // 1. ノブの追加 (VOCAL PROFILE セクション: 5個のパラメータ)
        addDial("character", "CHARACTER");
        addDial("bandCount", "BAND COUNT"); // ★新規追加ノブ
        addDial("formantShift", "FMT SHIFT");
        addDial("formantStretch", "FMT STRETCH");
        addDial("tracking", "TRACKING");

        // 2. ノブの追加 (EXCITATION セクション: 4個のパラメータ)
        addDial("wavetablePosition", "WT POS");
        addDial("pulseWidth", "PULSE WIDTH");
        addDial("detune", "DETUNE");
        addDial("noise", "NOISE MIX");

        // 3. ノブの追加 (ADSR & OUTPUT セクション: 6個のパラメータ)
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
        g.fillAll(ColorPalette::background);

        // セクション1: PROFILER 枠
        g.setColour(ColorPalette::panelBorder);
        g.drawRoundedRectangle(15.0f, 40.0f, 470.0f, 260.0f, 6.0f, 2.0f);
        g.setColour(ColorPalette::textHeader);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText("ZDF SVF FILTER BANK PROFILER", 25, 15, 300, 20, juce::Justification::left);

        // セクション2: EXCITATION OSCILLATOR 枠
        g.setColour(ColorPalette::panelBorder);
        g.drawRoundedRectangle(500.0f, 40.0f, 285.0f, 260.0f, 6.0f, 2.0f);
        g.setColour(ColorPalette::textHeader);
        g.drawText("EXCITATION OSCILLATOR", 510, 15, 250, 20, juce::Justification::left);

        // セクション3: ADSR / MIX OUT 枠
        g.setColour(ColorPalette::panelBorder);
        g.drawRoundedRectangle(15.0f, 310.0f, 770.0f, 150.0f, 6.0f, 2.0f);
        g.setColour(ColorPalette::textHeader);
        g.drawText("ENVELOPE / OUTPUT MIX", 25, 315, 250, 20, juce::Justification::left);

        g.setColour(ColorPalette::textMuted);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));

        // 全15個のノブのラベルを描画
        for (size_t i = 0; i < mDials.size(); ++i)
        {
            auto bounds = mDials[i].slider->getBounds();
            g.drawText(mDials[i].label, bounds.getX(), bounds.getY() - 15, bounds.getWidth(), 12, juce::Justification::centred);
        }
    }

    void MainPanel::resized()
    {
        // セクション1 (Dials 0〜4: 5個のノブをバランス良く配置)
        int startX = 32;
        int startY = 70;
        int dialSize = 75;
        int spacingX = 85;
        int spacingY = 40;

        // 1行目に3個
        for (int i = 0; i < 3; ++i)
        {
            mDials[i].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonCyan);
            mDials[i].slider->setBounds(startX + i * (dialSize + spacingX), startY, dialSize, dialSize + 16);
        }
        // 2行目に2個
        for (int i = 3; i < 5; ++i)
        {
            int idx = i - 3;
            mDials[i].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonCyan);
            mDials[i].slider->setBounds(startX + 40 + idx * (dialSize + spacingX + 20), startY + dialSize + spacingY, dialSize, dialSize + 16);
        }

        // セクション2: コンボボックス配置
        mWaveformLabel.setBounds(520, 55, 120, 18);
        mWaveformCombo.setBounds(520, 75, 120, 24);

        mModeLabel.setBounds(650, 55, 120, 18);
        mModeCombo.setBounds(650, 75, 120, 24);

        // Dials 5〜8 (WT POS, PULSE WIDTH, DETUNE, NOISE MIX)
        int oscDialSize = 58;
        mDials[5].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
        mDials[5].slider->setBounds(520, 115, oscDialSize, oscDialSize + 16);

        mDials[6].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
        mDials[6].slider->setBounds(660, 115, oscDialSize, oscDialSize + 16);

        mDials[7].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
        mDials[7].slider->setBounds(520, 205, oscDialSize, oscDialSize + 16);

        mDials[8].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPink);
        mDials[8].slider->setBounds(660, 205, oscDialSize, oscDialSize + 16);

        // セクション3: Dials 9〜14 (ADSR & MIX & OUT LEVEL)
        int adsrStartX = 30;
        int adsrStartY = 350;
        int adsrSpacingX = 49;

        for (int i = 9; i < 15; ++i)
        {
            int idx = i - 9;
            mDials[i].slider->setColour(juce::Slider::rotarySliderFillColourId, ColorPalette::neonPurple);
            mDials[i].slider->setBounds(adsrStartX + idx * (dialSize + adsrSpacingX), adsrStartY, dialSize, dialSize + 16);
        }
    }

} // namespace GUI