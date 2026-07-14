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
    setupCombo(mComboLpcInterpolation, { "Step Interp", "LSP Interp", "LAR Interp" });
    setupCombo(mComboFilterbankType, { "BPF Bank", "Subtractive LR4" });
    setupCombo(mComboLpcOrder, { "Order 8", "Order 10", "Order 12", "Order 16" });
    setupCombo(mComboFrameRate, { "8 Hz", "15 Hz", "25 Hz", "50 Hz", "80 Hz" });
    setupCombo(mComboQuantBits, { "K: Off", "K: 6bit", "K: 5bit", "K: 4bit", "K: 3bit" });
    // 低次数プリセット(マクロ)。先頭はプレースホルダ。選択で複数paramを一括設定し先頭へ戻る。
    setupCombo(mComboPreset, { "PRESET...", "Hi-Fi 16", "Vintage 12", "Retro 10", "Toy 8", "BitSpeek 3bit" });

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
    mAttachmentLpcOrder        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "lpcOrder", mComboLpcOrder);
    mAttachmentFrameRate       = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "frameRate", mComboFrameRate);
    mAttachmentQuantBits       = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "lpcQuantBits", mComboQuantBits);

    // モード変更に応じた有効/無効表示の連動
    // (ComboBoxAttachment は Listener 経由なので onChange ラムダとは競合しない)
    mComboVocoderMode.onChange = [this] { updateEnablement(); };

    // プリセット選択 → 一括適用 → プレースホルダ(先頭)へ戻す
    mComboPreset.onChange = [this]
    {
        const int sel = mComboPreset.getSelectedItemIndex(); // 0=プレースホルダ
        if (sel > 0) applyPreset(sel);
        mComboPreset.setSelectedItemIndex(0, juce::dontSendNotification);
    };

    updateEnablement();
}

void VocoderPanel::applyPreset(int idx)
{
    // paramを0..1正規化で設定するヘルパー(アタッチ済みコンボ/ノブにも反映される)
    auto setChoice = [this](const char* id, int index)
    {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1((float)index));
    };

    // 各preset: {vocoderMode(1=LPC), lpcOrder(0..3=8/10/12/16), frameRate(0..4=8/15/25/50/80Hz),
    //            lpcQuantBits(0..4=Off/6/5/4/3bit), windowType(0=Hann),
    //            interp(0=Step/1=LSP/2=LAR)}  ※Hi-Fi系は滑らかなLSP、レトロ系はカクつくStep
    struct P { int mode, order, rate, quant, window, interp; };
    static const P table[5] = {
        { 1, 3, 3, 0, 0, 1 }, // Hi-Fi 16   : order16, 50Hz, K:Off,  LSP補間
        { 1, 2, 3, 1, 0, 1 }, // Vintage 12 : order12, 50Hz, K:6bit, LSP補間
        { 1, 1, 2, 2, 0, 0 }, // Retro 10   : order10, 25Hz, K:5bit, Step
        { 1, 0, 1, 3, 0, 0 }, // Toy 8      : order8,  15Hz, K:4bit, Step
        { 1, 1, 1, 4, 0, 0 }, // BitSpeek   : order10, 15Hz, K:3bit, Step
    };
    const P& pr = table[juce::jlimit(0, 4, idx - 1)];
    setChoice("vocoderMode",       pr.mode);
    setChoice("lpcOrder",          pr.order);
    setChoice("frameRate",         pr.rate);
    setChoice("lpcQuantBits",      pr.quant);
    setChoice("windowType",        pr.window);
    setChoice("interpolationMode", pr.interp);
    updateEnablement();
}

void VocoderPanel::updateEnablement()
{
    const bool lpc = (mComboVocoderMode.getSelectedItemIndex() == 1);

    // LPCモード専用
    mComboLpcOrder.setEnabled(lpc);
    mComboAnalysisWindow.setEnabled(lpc);       // 分析窓はLPCモードで初めて音に効く
    mComboFrameRate.setEnabled(lpc);            // M5: レトロ層はLPC専用
    mComboQuantBits.setEnabled(lpc);

    // Filterbankモード専用
    mComboFilterbankType.setEnabled(!lpc);

    // M4: フルホップ補間(Step/LSP/LAR)はLPCモード専用
    mComboLpcInterpolation.setEnabled(lpc);
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
    const int comboH = 22;
    const int comboW = leftArea.getWidth() - 16;
    const int cx = leftArea.getX() + 8;
    const int step = 27;                 // M5でコンボが増えたため行間を圧縮
    int cy = leftArea.getY() + 6;

    // コンボボックス配置 (上から: プリセット → モード系 → LPC分析系 → M5レトロ系 → 補間/リミッタ)
    mComboPreset.setBounds(cx, cy, comboW, comboH);            cy += step;  // M5 プリセット
    mComboVocoderMode.setBounds(cx, cy, comboW, comboH);       cy += step;
    mComboVoicingMode.setBounds(cx, cy, comboW, comboH);       cy += step;
    mComboFilterbankType.setBounds(cx, cy, comboW, comboH);    cy += step;
    mComboAnalysisWindow.setBounds(cx, cy, comboW, comboH);    cy += step;
    mComboFrameRate.setBounds(cx, cy, comboW, comboH);         cy += step;  // M5
    mComboQuantBits.setBounds(cx, cy, comboW, comboH);         cy += step;  // M5
    mComboLpcInterpolation.setBounds(cx, cy, comboW, comboH);  cy += step;
    mComboLimiter.setBounds(cx, cy, comboW, comboH);           cy += step;

    // フリーズボタン + LPC次数コンボ (左右分割)
    const int halfW = (comboW - 8) / 2;
    mBtnFormantFreeze.setBounds(cx, cy, halfW, comboH + 4);
    mComboLpcOrder.setBounds(cx + halfW + 8, cy + 2, halfW, comboH);

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
