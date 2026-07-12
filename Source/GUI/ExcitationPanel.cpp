// ==========================================
// File: ExcitationPanel.cpp
// 「EXCITATION」タブ・パネル (Granular 準拠)
// ==========================================
#include "ExcitationPanel.h"

ExcitationPanel::ExcitationPanel(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    auto setupKnob = [this](ValueKnob& k, juce::Label& l, const juce::String& suffix = "")
    {
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        k.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentExcitation);
        k.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
        k.setTextValueSuffix(suffix);
        k.setLookAndFeel(&mArcLookAndFeel);
        addAndMakeVisible(k);

        l.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, SpectraColors::textDim);
        addAndMakeVisible(l);
    };

    setupKnob(mKnobWtPos, mLblWtPos);
    setupKnob(mKnobPulseWidth, mLblPulseWidth, "%");
    setupKnob(mKnobDetune, mLblDetune, " cents");
    setupKnob(mKnobNoise, mLblNoise, "%");
    setupKnob(mKnobLofi, mLblLofi);
    setupKnob(mKnobPorta, mLblPorta, "s");
    setupKnob(mKnobBasePitch, mLblBasePitch, " Hz");
    setupKnob(mKnobNoiseColor, mLblNoiseColor, " Hz");

    mComboWaveform.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
    mComboWaveform.setColour(juce::ComboBox::textColourId, SpectraColors::text);
    mComboWaveform.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
    mComboWaveform.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
    mComboWaveform.setJustificationType(juce::Justification::centred);
    mComboWaveform.addItem("Sawtooth", 1);
    mComboWaveform.addItem("Pulse", 2);
    mComboWaveform.addItem("Wavetable", 3);
    addAndMakeVisible(mComboWaveform);

    // アタッチメント作成
    mAttachmentWtPos      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wavetablePosition", mKnobWtPos);
    mAttachmentPulseWidth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pulseWidth", mKnobPulseWidth);
    mAttachmentDetune     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "detune", mKnobDetune);
    mAttachmentNoise      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noise", mKnobNoise);
    mAttachmentLofi       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lofi", mKnobLofi);
    mAttachmentPorta      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "porta", mKnobPorta);
    mAttachmentBasePitch  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "basePitch", mKnobBasePitch);
    mAttachmentNoiseColor = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noiseColor", mKnobNoiseColor);

    mAttachmentWaveform   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "waveform", mComboWaveform);
}

ExcitationPanel::~ExcitationPanel()
{
    mKnobWtPos.setLookAndFeel(nullptr);
    mKnobPulseWidth.setLookAndFeel(nullptr);
    mKnobDetune.setLookAndFeel(nullptr);
    mKnobNoise.setLookAndFeel(nullptr);
    mKnobLofi.setLookAndFeel(nullptr);
    mKnobPorta.setLookAndFeel(nullptr);
    mKnobBasePitch.setLookAndFeel(nullptr);
    mKnobNoiseColor.setLookAndFeel(nullptr);
}

void ExcitationPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);
}

void ExcitationPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    const int w = r.getWidth();
    const int h = r.getHeight();

    // 左側: 波形選択コンボボックス
    const int comboH = 26;
    const int comboW = 128;
    mComboWaveform.setBounds(r.getX() + 32, r.getY() + 32, comboW, comboH);

    // 右側: パラメータノブ群を2段に配列
    const int knobSize = 64;
    const int labelH = 14;

    const int startX = r.getX() + 192;
    const int stepX = (w - 192) / 3;
    const int rowY1 = r.getY() + 32;
    const int rowY2 = r.getY() + 144;

    // 上段ノブ (WT POS, PULSE WIDTH, DETUNE)
    mKnobWtPos.setBounds(startX, rowY1, knobSize, knobSize);
    mLblWtPos.setBounds(startX - 10, rowY1 + knobSize, knobSize + 20, labelH);

    mKnobPulseWidth.setBounds(startX + stepX, rowY1, knobSize, knobSize);
    mLblPulseWidth.setBounds(startX + stepX - 10, rowY1 + knobSize, knobSize + 20, labelH);

    mKnobDetune.setBounds(startX + stepX * 2, rowY1, knobSize, knobSize);
    mLblDetune.setBounds(startX + stepX * 2 - 10, rowY1 + knobSize, knobSize + 20, labelH);

    // 下段ノブ (NOISE MIX, LOFI, PORTA)
    mKnobNoise.setBounds(startX, rowY2, knobSize, knobSize);
    mLblNoise.setBounds(startX - 10, rowY2 + knobSize, knobSize + 20, labelH);

    mKnobLofi.setBounds(startX + stepX, rowY2, knobSize, knobSize);
    mLblLofi.setBounds(startX + stepX - 10, rowY2 + knobSize, knobSize + 20, labelH);

    mKnobPorta.setBounds(startX + stepX * 2, rowY2, knobSize, knobSize);
    mLblPorta.setBounds(startX + stepX * 2 - 10, rowY2 + knobSize, knobSize + 20, labelH);

    // 左側下段: BASE PITCHノブ と NOISE COLORノブ を並べて配置 (波形選択コンボの下)
    const int leftX1 = r.getX() + 16;
    const int leftX2 = r.getX() + comboW - 16;
    
    mKnobBasePitch.setBounds(leftX1, rowY2, knobSize, knobSize);
    mLblBasePitch.setBounds(leftX1 - 10, rowY2 + knobSize, knobSize + 20, labelH);

    mKnobNoiseColor.setBounds(leftX2, rowY2, knobSize, knobSize);
    mLblNoiseColor.setBounds(leftX2 - 10, rowY2 + knobSize, knobSize + 20, labelH);
}
