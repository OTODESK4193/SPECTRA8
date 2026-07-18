// ==========================================
// File: VocoderPanel.cpp
// 「VOCODER」タブ・パネル (Granular 準拠)
// ==========================================
#include "VocoderPanel.h"

VocoderPanel::VocoderPanel(juce::AudioProcessorValueTreeState& state)
    : apvts(state),
      mBtnLimiter("LIMIT", SpectraColors::rose),
      mBtnFormantFreeze("FREEZE", SpectraColors::accentVocoder)
{
    // ノブの共通セットアップ
    auto setupKnob = [this](ValueKnob& k, juce::Label& l, const juce::String& suffix = "")
    {
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
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

    // 上段ノブ
    setupKnob(mKnobCharacter, mLblCharacter);
    setupKnob(mKnobTracking, mLblTracking, "%");
    setupKnob(mKnobPitchQuantize, mLblPitchQuantize, "%");
    setupKnob(mKnobFmtShift, mLblFmtShift, " st");
    setupKnob(mKnobFmtStretch, mLblFmtStretch, "x");
    setupKnob(mKnobLofi, mLblLofi);
    setupKnob(mKnobBasePitch, mLblBasePitch, " Hz");
    setupKnob(mKnobNoiseColor, mLblNoiseColor, " Hz");
    setupKnob(mKnobNoise, mLblNoise, "%");
    setupKnob(mKnobBands, mLblBands);
    setupKnob(mKnobResonance, mLblResonance, "x");
    // 下段ノブ
    setupKnob(mKnobAttack, mLblAttack, "s");
    setupKnob(mKnobDecay, mLblDecay, "s");
    setupKnob(mKnobSustain, mLblSustain);
    setupKnob(mKnobRelease, mLblRelease, "s");
    setupKnob(mKnobMix, mLblMix, "%");
    setupKnob(mKnobOutLevel, mLblOutLevel, " dB");

    // コンボボックス共通セットアップ
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
    setupCombo(mComboTrackResponse, { "Track: Fast", "Track: Natural", "Track: Smooth" });
    setupCombo(mComboFilterbankType, { "BPF Bank", "Subtractive LR4" });
    setupCombo(mComboLpcOrder, { "Order 8", "Order 10", "Order 12", "Order 16" });
    setupCombo(mComboAnalysisWindow, { "Hann Window", "Hamming Window", "Blackman Window" });
    setupCombo(mComboFrameRate, { "8 Hz", "15 Hz", "25 Hz", "50 Hz", "80 Hz" });
    setupCombo(mComboQuantBits, { "K: Off", "K: 6bit", "K: 5bit", "K: 4bit", "K: 3bit" });
    setupCombo(mComboLpcInterpolation, { "Step Interp", "LSP Interp", "LAR Interp" });

    // 点灯式トグルボタン
    addAndMakeVisible(mBtnLimiter);
    addAndMakeVisible(mBtnFormantFreeze);

    // --- アタッチメント ---
    mAttachmentCharacter     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "character", mKnobCharacter);
    mAttachmentTracking      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "tracking", mKnobTracking);
    mAttachmentPitchQuantize = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pitchQuantize", mKnobPitchQuantize);
    mAttachmentFmtShift      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "formantShift", mKnobFmtShift);
    mAttachmentFmtStretch    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "formantStretch", mKnobFmtStretch);
    mAttachmentLofi          = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lofi", mKnobLofi);
    mAttachmentBasePitch     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "basePitch", mKnobBasePitch);
    mAttachmentNoiseColor    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noiseColor", mKnobNoiseColor);
    mAttachmentNoise         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "noise", mKnobNoise);
    mAttachmentBands         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bandCount", mKnobBands);
    mAttachmentResonance     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "resonance", mKnobResonance);
    mAttachmentAttack        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", mKnobAttack);
    mAttachmentDecay         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", mKnobDecay);
    mAttachmentSustain       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sustain", mKnobSustain);
    mAttachmentRelease       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", mKnobRelease);
    mAttachmentMix           = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mKnobMix);
    mAttachmentOutLevel      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputLevel", mKnobOutLevel);

    mAttachmentVocoderMode      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "vocoderMode", mComboVocoderMode);
    mAttachmentVoicingMode      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", mComboVoicingMode);
    mAttachmentTrackResponse    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "trackResponse", mComboTrackResponse);
    mAttachmentFilterbankType   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "filterbankType", mComboFilterbankType);
    mAttachmentLpcOrder         = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "lpcOrder", mComboLpcOrder);
    mAttachmentAnalysisWindow   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "windowType", mComboAnalysisWindow);
    mAttachmentFrameRate        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "frameRate", mComboFrameRate);
    mAttachmentQuantBits        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "lpcQuantBits", mComboQuantBits);
    mAttachmentLpcInterpolation = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "interpolationMode", mComboLpcInterpolation);

    mAttachmentLimiter       = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "limiterEnable", mBtnLimiter);
    mAttachmentFormantFreeze = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "formantFreeze", mBtnFormantFreeze);

    // モード変更に応じた表示切替（ComboBoxAttachmentのListenerとは競合しない）
    mComboVocoderMode.onChange = [this] { updateEnablement(); };

    updateEnablement();
}

VocoderPanel::~VocoderPanel()
{
    for (ValueKnob* k : { &mKnobCharacter, &mKnobTracking, &mKnobPitchQuantize, &mKnobFmtShift,
                          &mKnobFmtStretch, &mKnobLofi, &mKnobBasePitch, &mKnobNoiseColor,
                          &mKnobNoise, &mKnobBands, &mKnobResonance, &mKnobAttack, &mKnobDecay,
                          &mKnobSustain, &mKnobRelease, &mKnobMix, &mKnobOutLevel })
        k->setLookAndFeel(nullptr);
}

void VocoderPanel::updateEnablement()
{
    const bool lpc = (mComboVocoderMode.getSelectedItemIndex() == 1);

    // LPC専用の表示
    mComboLpcOrder.setVisible(lpc);
    mComboAnalysisWindow.setVisible(lpc);
    mComboFrameRate.setVisible(lpc);
    mComboQuantBits.setVisible(lpc);
    mComboLpcInterpolation.setVisible(lpc);
    mBtnFormantFreeze.setVisible(lpc);   // FREEZE は LPC のみ有効

    // FilterBank専用の表示
    mComboFilterbankType.setVisible(!lpc);
    mKnobResonance.setVisible(!lpc);
    mLblResonance.setVisible(!lpc);

    // BANDS は両モードで表示。FilterBankでは帯域数(音色)、LPCではポストEQの帯域解像度を決める。
    // → LPCでもBANDSを上げれば最大48バンドのグラフィックEQ(BANDS EQタブ)を細かく設定できる。
    mKnobBands.setVisible(true);
    mLblBands.setVisible(true);

    resized(); // 表示状態に合わせて再レイアウト
}

void VocoderPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // コンボ列とノブエリアの区切り線
    auto inner = getLocalBounds().reduced(16);
    const int comboColW = 104;
    const float divX = (float)(inner.getX() + comboColW + 7);
    g.setColour(SpectraColors::panelLine);
    g.drawVerticalLine((int)divX, (float)inner.getY() + 4.0f, (float)inner.getBottom() - 4.0f);

    // 上段ノブエリアと下段ノブエリアの区切り線
    const int knobAreaX = inner.getX() + comboColW + 14;
    const float divY = (float)(inner.getY() + 6 + 2 * 82 - 2);
    g.drawHorizontalLine((int)divY, (float)knobAreaX, (float)inner.getRight() - 4.0f);
}

void VocoderPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    const bool lpc = (mComboVocoderMode.getSelectedItemIndex() == 1);

    // --- 左1列: コンボボックス（幅を短縮） ---
    const int comboW = 104, comboH = 22, comboStep = 26;
    const int cx = r.getX();
    int cy = r.getY() + 2;
    auto placeCombo = [&](juce::ComboBox& c) { c.setBounds(cx, cy, comboW, comboH); cy += comboStep; };

    placeCombo(mComboVocoderMode);
    placeCombo(mComboVoicingMode);
    placeCombo(mComboTrackResponse);
    if (lpc)
    {
        placeCombo(mComboLpcOrder);
        placeCombo(mComboAnalysisWindow);
        placeCombo(mComboFrameRate);
        placeCombo(mComboQuantBits);
        placeCombo(mComboLpcInterpolation);
    }
    else
    {
        placeCombo(mComboFilterbankType);
    }

    // --- ノブエリア ---
    const int knobAreaX = cx + comboW + 14;
    const int knobAreaW = r.getRight() - knobAreaX;
    const int knobSize = 58, labelH = 14;

    // 上段（2行）: 共通ノブ + 移設ノブ + BANDS(両モード) + RESONANCE(FB専用)
    std::vector<std::pair<ValueKnob*, juce::Label*>> upper = {
        { &mKnobCharacter,     &mLblCharacter },
        { &mKnobTracking,      &mLblTracking },
        { &mKnobPitchQuantize, &mLblPitchQuantize },
        { &mKnobFmtShift,      &mLblFmtShift },
        { &mKnobFmtStretch,    &mLblFmtStretch },
        { &mKnobLofi,          &mLblLofi },
        { &mKnobBasePitch,     &mLblBasePitch },
        { &mKnobNoiseColor,    &mLblNoiseColor },
        { &mKnobNoise,         &mLblNoise },
        { &mKnobBands,         &mLblBands },
    };
    if (!lpc)
        upper.push_back({ &mKnobResonance, &mLblResonance });

    // 11ノブ(FBモード)のときは6列に詰めて2行へ収める
    const int ucols = ((int)upper.size() > 10) ? 6 : 5;
    const int ucolW = knobAreaW / ucols;
    const int urowH = 82;
    const int uy0 = r.getY() + 6;
    for (int i = 0; i < (int)upper.size(); ++i)
    {
        const int col = i % ucols, row = i / ucols;
        const int kx = knobAreaX + col * ucolW + (ucolW - knobSize) / 2;
        const int ky = uy0 + row * urowH;
        upper[(size_t)i].first->setBounds(kx, ky, knobSize, knobSize);
        upper[(size_t)i].second->setBounds(kx - 10, ky + knobSize, knobSize + 20, labelH);
    }

    // 下段（1行）: ADSR + MIX + OUT + ボタン列。
    // 下段エリア = 区切り線(divY) 〜 パネル下端。その中でノブ/ボタンを縦中央に配置する。
    const int lcols = 7;
    const int lcolW = knobAreaW / lcols;
    const int lowerTop = uy0 + 2 * urowH - 2;        // paint()の区切り線と一致
    const int lowerBottom = r.getBottom();
    const int lowerAreaH = lowerBottom - lowerTop;

    // ノブ本体+ラベルの合計高で縦センタリング
    const int knobBlockH = knobSize + labelH;
    const int ly = lowerTop + (lowerAreaH - knobBlockH) / 2;

    ValueKnob* lower[6]   = { &mKnobAttack, &mKnobDecay, &mKnobSustain, &mKnobRelease, &mKnobMix, &mKnobOutLevel };
    juce::Label* lowerL[6] = { &mLblAttack, &mLblDecay, &mLblSustain, &mLblRelease, &mLblMix, &mLblOutLevel };
    for (int i = 0; i < 6; ++i)
    {
        const int kx = knobAreaX + i * lcolW + (lcolW - knobSize) / 2;
        lower[i]->setBounds(kx, ly, knobSize, knobSize);
        lowerL[i]->setBounds(kx - 10, ly + knobSize, knobSize + 20, labelH);
    }

    // ボタン列（OUTノブの右）: LIMIT と FREEZE を上下二段。二段スタックを縦センタリング。
    const int btnW = lcolW - 6;
    const int btnH = 24;
    const int btnGap = 8;
    const int stackH = btnH * 2 + btnGap;
    const int by = lowerTop + (lowerAreaH - stackH) / 2;
    const int bx = knobAreaX + 6 * lcolW + 3;
    mBtnLimiter.setBounds(bx, by, btnW, btnH);
    mBtnFormantFreeze.setBounds(bx, by + btnH + btnGap, btnW, btnH);
}
