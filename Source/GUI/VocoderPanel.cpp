// ==========================================
// File: VocoderPanel.cpp
// 「VOCODER」タブ・パネル (Granular 準拠)
// ==========================================
#include "VocoderPanel.h"

VocoderPanel::VocoderPanel(juce::AudioProcessorValueTreeState& state)
    : apvts(state),
      mBtnFormantFreeze("FREEZE", SpectraColors::accentVocoder)
{
    // LookAndFeel の一括設定とコンポーネントの初期設定関数
    auto setupKnob = [this](ValueKnob& k, juce::Label& l, const juce::String& suffix = "")
    {
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        k.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentVocoder);
        k.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
        k.setTextValueSuffix(suffix);
        k.setLookAndFeel(&mArcLookAndFeel);
        addAndMakeVisible(k);

        l.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, SpectraColors::textDim);
        addAndMakeVisible(l);
    };

    setupKnob(mKnobCharacter, mLblCharacter);
    setupKnob(mKnobBands, mLblBands);
    setupKnob(mKnobFmtShift, mLblFmtShift, " st");
    setupKnob(mKnobFmtStretch, mLblFmtStretch, "x");
    setupKnob(mKnobTracking, mLblTracking, "%");
    setupKnob(mKnobAttack, mLblAttack, "s");
    setupKnob(mKnobDecay, mLblDecay, "s");
    setupKnob(mKnobSustain, mLblSustain);
    setupKnob(mKnobRelease, mLblRelease, "s");
    setupKnob(mKnobMix, mLblMix, "%");
    setupKnob(mKnobOutLevel, mLblOutLevel, " dB");
    setupKnob(mKnobPitchQuantize, mLblPitchQuantize, "%");

    // コンボボックス初期化
    auto setupCombo = [this](juce::ComboBox& c, const juce::StringArray& items)
    {
        c.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        c.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        c.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        c.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
        c.setJustificationType(juce::Justification::centred);
        for (int i = 0; i < items.size(); ++i)
            c.addItem(items[i], i + 1);
        addAndMakeVisible(c);
    };

    setupCombo(mComboVocoderMode, { "Filterbank", "LPC Mode" });
    setupCombo(mComboVoicingMode, { "Auto Mode", "MIDI Mode" });
    setupCombo(mComboLimiter, { "Limiter OFF", "Limiter ON" });
    setupCombo(mComboAnalysisWindow, { "Hann Window", "Hamming Window", "Blackman Window" });
    setupCombo(mComboLpcInterpolation, { "LSP Interp", "LAR Interp" });
    setupCombo(mComboFilterbankType, { "BPF Bank", "Subtractive LR4" });

    addAndMakeVisible(mBtnFormantFreeze);

    // アタッチメント作成
    mAttachmentCharacter  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "character", mKnobCharacter);
    mAttachmentBands      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bandCount", mKnobBands);
    mAttachmentFmtShift   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "formantShift", mKnobFmtShift);
    mAttachmentFmtStretch = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "formantStretch", mKnobFmtStretch);
    mAttachmentTracking   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "tracking", mKnobTracking);
    mAttachmentAttack     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", mKnobAttack);
    mAttachmentDecay      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", mKnobDecay);
    mAttachmentSustain    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sustain", mKnobSustain);
    mAttachmentRelease    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", mKnobRelease);
    mAttachmentMix        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mKnobMix);
    mAttachmentOutLevel   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputLevel", mKnobOutLevel);
    mAttachmentPitchQuantize = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pitchQuantize", mKnobPitchQuantize);

    mAttachmentVocoderMode     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "vocoderMode", mComboVocoderMode);
    mAttachmentVoicingMode     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", mComboVoicingMode);
    mAttachmentLimiter         = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "limiterEnable", mComboLimiter);
    mAttachmentAnalysisWindow  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "windowType", mComboAnalysisWindow);
    mAttachmentLpcInterpolation = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "interpolationMode", mComboLpcInterpolation);
    mAttachmentFilterbankType  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "filterbankType", mComboFilterbankType);

    mAttachmentFormantFreeze   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "formantFreeze", mBtnFormantFreeze);
}

VocoderPanel::~VocoderPanel()
{
    mKnobCharacter.setLookAndFeel(nullptr);
    mKnobBands.setLookAndFeel(nullptr);
    mKnobFmtShift.setLookAndFeel(nullptr);
    mKnobFmtStretch.setLookAndFeel(nullptr);
    mKnobTracking.setLookAndFeel(nullptr);
    mKnobAttack.setLookAndFeel(nullptr);
    mKnobDecay.setLookAndFeel(nullptr);
    mKnobSustain.setLookAndFeel(nullptr);
    mKnobRelease.setLookAndFeel(nullptr);
    mKnobMix.setLookAndFeel(nullptr);
    mKnobOutLevel.setLookAndFeel(nullptr);
    mKnobPitchQuantize.setLookAndFeel(nullptr);
}

void VocoderPanel::paint(juce::Graphics& g)
{
    // 全体背景
    g.fillAll(SpectraColors::bg);

    // セクション分けの補助枠の描画
    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // 縦セクションの区切り線
    const float w = r.getWidth();
    g.setColour(SpectraColors::panelLine);
    g.drawVerticalLine((int)(r.getX() + w * 0.35f), r.getY() + 10.0f, r.getBottom() - 10.0f);
    g.drawVerticalLine((int)(r.getX() + w * 0.70f), r.getY() + 10.0f, r.getBottom() - 10.0f);
}

void VocoderPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    const int w = r.getWidth();

    // 左セクション: 設定 & 分析モード (width: 35%)
    auto leftArea = r.removeFromLeft((int)(w * 0.35f));
    const int comboH = 26;
    const int comboW = leftArea.getWidth() - 16;
    
    // コンボボックス配置
    mComboVocoderMode.setBounds(leftArea.getX() + 8, leftArea.getY() + 12, comboW, comboH);
    mComboVoicingMode.setBounds(leftArea.getX() + 8, leftArea.getY() + 48, comboW, comboH);
    mComboFilterbankType.setBounds(leftArea.getX() + 8, leftArea.getY() + 84, comboW, comboH);
    mComboAnalysisWindow.setBounds(leftArea.getX() + 8, leftArea.getY() + 120, comboW, comboH);
    mComboLpcInterpolation.setBounds(leftArea.getX() + 8, leftArea.getY() + 156, comboW, comboH);
    mComboLimiter.setBounds(leftArea.getX() + 8, leftArea.getY() + 192, comboW, comboH);

    // フリーズボタン
    mBtnFormantFreeze.setBounds(leftArea.getX() + 8, leftArea.getY() + 234, comboW, comboH + 4);

    // 中央セクション: フォルマント & トラッキング (width: 35%)
    auto midArea = r.removeFromLeft((int)(w * 0.35f));
    const int knobSize = 64;
    const int labelH = 14;

    // 上段 3個ノブ (CHARACTER, BANDS, TRACKING)
    int midY1 = midArea.getY() + 24;
    int midX = midArea.getX() + (midArea.getWidth() - knobSize * 3) / 4;
    
    mKnobCharacter.setBounds(midX, midY1, knobSize, knobSize);
    mLblCharacter.setBounds(midX - 10, midY1 + knobSize, knobSize + 20, labelH);
    
    midX += knobSize + (midArea.getWidth() - knobSize * 3) / 4;
    mKnobBands.setBounds(midX, midY1, knobSize, knobSize);
    mLblBands.setBounds(midX - 10, midY1 + knobSize, knobSize + 20, labelH);
    
    midX += knobSize + (midArea.getWidth() - knobSize * 3) / 4;
    mKnobTracking.setBounds(midX, midY1, knobSize, knobSize);
    mLblTracking.setBounds(midX - 10, midY1 + knobSize, knobSize + 20, labelH);

    // 下段 3個ノブ (FMT SHIFT, FMT STRETCH, PITCH Q)
    int midY2 = midArea.getY() + 144;
    midX = midArea.getX() + (midArea.getWidth() - knobSize * 3) / 4;
    
    mKnobFmtShift.setBounds(midX, midY2, knobSize, knobSize);
    mLblFmtShift.setBounds(midX - 10, midY2 + knobSize, knobSize + 20, labelH);
    
    midX += knobSize + (midArea.getWidth() - knobSize * 3) / 4;
    mKnobFmtStretch.setBounds(midX, midY2, knobSize, knobSize);
    mLblFmtStretch.setBounds(midX - 10, midY2 + knobSize, knobSize + 20, labelH);

    midX += knobSize + (midArea.getWidth() - knobSize * 3) / 4;
    mKnobPitchQuantize.setBounds(midX, midY2, knobSize, knobSize);
    mLblPitchQuantize.setBounds(midX - 10, midY2 + knobSize, knobSize + 20, labelH);

    // 右セクション: ADSR / MIX / OUT (width: 30%)
    auto rightArea = r;
    
    // ADSR (グリッド風に並べる)
    const int adsrX1 = rightArea.getX() + 16;
    const int adsrX2 = rightArea.getX() + rightArea.getWidth() / 2 + 4;
    const int adsrY1 = rightArea.getY() + 24;
    const int adsrY2 = rightArea.getY() + 120;

    mKnobAttack.setBounds(adsrX1, adsrY1, knobSize, knobSize);
    mLblAttack.setBounds(adsrX1 - 10, adsrY1 + knobSize, knobSize + 20, labelH);
    
    mKnobDecay.setBounds(adsrX2, adsrY1, knobSize, knobSize);
    mLblDecay.setBounds(adsrX2 - 10, adsrY1 + knobSize, knobSize + 20, labelH);
    
    mKnobSustain.setBounds(adsrX1, adsrY2, knobSize, knobSize);
    mLblSustain.setBounds(adsrX1 - 10, adsrY2 + knobSize, knobSize + 20, labelH);
    
    mKnobRelease.setBounds(adsrX2, adsrY2, knobSize, knobSize);
    mLblRelease.setBounds(adsrX2 - 10, adsrY2 + knobSize, knobSize + 20, labelH);

    // 最終段 Mix & Outputノブ (最下部)
    const int outY = rightArea.getY() + 216;
    mKnobMix.setBounds(adsrX1, outY, knobSize, knobSize);
    mLblMix.setBounds(adsrX1 - 10, outY + knobSize, knobSize + 20, labelH);
    
    mKnobOutLevel.setBounds(adsrX2, outY, knobSize, knobSize);
    mLblOutLevel.setBounds(adsrX2 - 10, outY + knobSize, knobSize + 20, labelH);
}
