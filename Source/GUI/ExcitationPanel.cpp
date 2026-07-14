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
    setupKnob(mKnobPorta, mLblPorta, "s");

    mComboWaveform.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
    mComboWaveform.setColour(juce::ComboBox::textColourId, SpectraColors::text);
    mComboWaveform.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
    mComboWaveform.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
    mComboWaveform.setJustificationType(juce::Justification::centred);
    mComboWaveform.addItem("Sawtooth", 1);
    mComboWaveform.addItem("Pulse", 2);
    mComboWaveform.addItem("Wavetable", 3);
    addAndMakeVisible(mComboWaveform);

    addAndMakeVisible(mWaveDisplay);

    // アタッチメント作成
    mAttachmentWtPos      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wavetablePosition", mKnobWtPos);
    mAttachmentPulseWidth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pulseWidth", mKnobPulseWidth);
    mAttachmentDetune     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "detune", mKnobDetune);
    mAttachmentPorta      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "porta", mKnobPorta);

    mAttachmentWaveform   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "waveform", mComboWaveform);

    // 波形表示を各パラメータ変更に追従させる（アタッチメントのListenerとは別枠のコールバック）
    mComboWaveform.onChange   = [this] { refreshWaveformDisplay(); };
    mKnobPulseWidth.onValueChange = [this] { refreshWaveformDisplay(); };
    mKnobWtPos.onValueChange      = [this] { refreshWaveformDisplay(); };

    refreshWaveformDisplay();
}

ExcitationPanel::~ExcitationPanel()
{
    mKnobWtPos.setLookAndFeel(nullptr);
    mKnobPulseWidth.setLookAndFeel(nullptr);
    mKnobDetune.setLookAndFeel(nullptr);
    mKnobPorta.setLookAndFeel(nullptr);
}

void ExcitationPanel::refreshWaveformDisplay()
{
    const int type = (int)apvts.getRawParameterValue("waveform")->load();
    const float pw  = apvts.getRawParameterValue("pulseWidth")->load() * 0.01f;   // 5..95% → 0.05..0.95
    const float wt  = apvts.getRawParameterValue("wavetablePosition")->load();     // 0..1
    mWaveDisplay.setParams(type, pw, wt);
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

    // --- 左: 波形コンボ + 2D波形表示 ---
    const int comboH = 26;
    const int comboW = 160;
    const int leftX = r.getX() + 16;
    mComboWaveform.setBounds(leftX, r.getY() + 20, comboW, comboH);

    const int dispW = 288;
    const int dispH = 160;
    mWaveDisplay.setBounds(leftX, r.getY() + 20 + comboH + 12, dispW, dispH);

    // --- 右: ノブ 4基 (2×2 グリッド) ---
    const int knobSize = 64;
    const int labelH = 14;
    const int rightX = r.getX() + 360;
    const int stepX = (r.getRight() - rightX) / 2;
    const int rowY1 = r.getY() + 40;
    const int rowY2 = r.getY() + 160;

    auto place = [&](ValueKnob& k, juce::Label& l, int gx, int gy)
    {
        const int kx = rightX + gx * stepX + (stepX - knobSize) / 2;
        k.setBounds(kx, gy, knobSize, knobSize);
        l.setBounds(kx - 10, gy + knobSize, knobSize + 20, labelH);
    };

    place(mKnobWtPos,      mLblWtPos,      0, rowY1);
    place(mKnobPulseWidth, mLblPulseWidth, 1, rowY1);
    place(mKnobDetune,     mLblDetune,     0, rowY2);
    place(mKnobPorta,      mLblPorta,      1, rowY2);
}
