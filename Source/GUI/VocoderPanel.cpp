// ==========================================
// File: VocoderPanel.cpp
// 「VOCODER」タブ・パネル (Granular 準拠)
// ==========================================
#include "VocoderPanel.h"
#include "../PluginProcessor.h"

VocoderPanel::VocoderPanel(SPECTRA8AudioProcessor& proc)
    : processor(proc),
      apvts(proc.apvts),
      mBtnLimiter("LIMIT", SpectraColors::rose),
      mBtnFormantFreeze("FREEZE", SpectraColors::accentVocoder)
{
    // ノブの共通セットアップ
    // tip = 下部ステータス行に出す英語の説明文 (PluginEditor::timerCallback が拾う)
    auto setupKnob = [this](ValueKnob& k, juce::Label& l, const juce::String& tip,
                            const juce::String& suffix = "")
    {
        k.setTooltip(tip);
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
    setupKnob(mKnobCharacter, mLblCharacter,
        "CHARACTER - how quickly each band follows the voice. High = crisp and articulate "
        "consonants; low = smooth, blurred and more synthetic. In LPC mode it also sharpens "
        "or softens the formant peaks.");
    setupKnob(mKnobTracking, mLblTracking,
        "TRACKING - how much the carrier pitch follows the pitch of the incoming voice. "
        "0% locks it to BASE PITCH (robot monotone), 100% follows the singer exactly.", "%");
    setupKnob(mKnobPitchQuantize, mLblPitchQuantize,
        "PITCH Q - snaps the carrier pitch to the KEY and SCALE selected on the left. "
        "Turn it up for a hard auto-tune / talkbox effect.", "%");
    setupKnob(mKnobFmtShift, mLblFmtShift,
        "FMT SHIFT - moves the formants up or down in semitones without changing the pitch. "
        "Up = smaller head / chipmunk, down = larger head / monster.", " st");
    setupKnob(mKnobFmtStretch, mLblFmtStretch,
        "FMT STRETCH - spreads or compresses the spacing between formants. "
        "Above 1.0 widens the vowel character, below 1.0 makes it darker and narrower.", "x");
    setupKnob(mKnobLofi, mLblLofi,
        "LOFI - bit crush plus pitch-locked sample and hold on the carrier. "
        "Adds retro 8-bit grit while keeping the note in tune.");
    setupKnob(mKnobBasePitch, mLblBasePitch,
        "BASE PITCH - the fallback carrier pitch used in Auto mode. "
        "With TRACKING at 0% this is the exact pitch you hear.", " Hz");
    setupKnob(mKnobNoiseColor, mLblNoiseColor,
        "NOISE COLOR - centre frequency of the band-passed noise source. "
        "Low = breathy rumble, high = airy hiss and sibilance.", " Hz");
    // Hz系は小数を出すと "1000.00 Hz" が幅60pxに収まらず "1000..." と省略されるため整数表示
    mKnobBasePitch.setNumDecimalPlacesToDisplay(0);
    mKnobNoiseColor.setNumDecimalPlacesToDisplay(0);
    setupKnob(mKnobNoise, mLblNoise,
        "NOISE MIX - blends noise into the carrier for breath and consonants. "
        "In LPC mode it is bipolar: negative removes noise for a pure tone, "
        "0 follows the voice automatically, positive forces a whispered sound.", "%");
    setupKnob(mKnobBands, mLblBands,
        "BANDS - number of analysis/synthesis bands from 8 to 48, always spread over "
        "80 Hz to 7.5 kHz. Fewer bands = coarser, more vintage; more bands = more intelligible. "
        "Also sets the resolution of the BANDS EQ tab.");
    setupKnob(mKnobResonance, mLblResonance,
        "RESONANCE - width of each band filter. Low values are broad and warm, "
        "high values are narrow and whistly with a stronger vocoder character.", "x");
    setupKnob(mKnobWidth, mLblWidth,
        "WIDTH - how far alternating bands are panned left and right. "
        "0 keeps everything centred and fully mono-compatible, 1 gives the widest spread.");
    // 下段ノブ
    setupKnob(mKnobAttack, mLblAttack,
        "ATTACK - MIDI mode only. Time for the carrier to fade in after a note is pressed.", "s");
    setupKnob(mKnobDecay, mLblDecay,
        "DECAY - MIDI mode only. Time to fall from full level down to the SUSTAIN level.", "s");
    setupKnob(mKnobSustain, mLblSustain,
        "SUSTAIN - MIDI mode only. Level the carrier holds while a key stays pressed.");
    setupKnob(mKnobRelease, mLblRelease,
        "RELEASE - MIDI mode only. Time for the carrier to fade out after the key is let go.", "s");
    setupKnob(mKnobMix, mLblMix,
        "MIX - crossfade between the untouched input and the vocoded signal. "
        "The FX chain belongs to the wet side, so at 0% the input passes through completely clean.", "%");
    setupKnob(mKnobMasterPitch, mLblMasterPitch,
        "M.PITCH - transposes the carrier in semitones. If PITCH Q is engaged the result is "
        "snapped to the key and scale, so modulating this walks the sound around that scale.");
    setupKnob(mKnobOutLevel, mLblOutLevel,
        "OUT LEVEL - final output gain, applied after the mix and before the limiter.", " dB");

    // コンボボックス共通セットアップ
    auto setupCombo = [this](juce::ComboBox& c, const juce::StringArray& items,
                             const juce::String& tip)
    {
        c.setTooltip(tip);
        c.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        c.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        c.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        c.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
        c.setJustificationType(juce::Justification::centred);
        for (int i = 0; i < items.size(); ++i)
            c.addItem(items[i], i + 1);
        addAndMakeVisible(c);
    };

    setupCombo(mComboVocoderMode, { "Filterbank", "LPC Mode" },
        "ENGINE - Filterbank is the classic analogue-style bank of band filters: bright, wide "
        "and musical. LPC models the vocal tract itself: more speech-like and intelligible, "
        "and the source of the retro BitSpeek-style tones.");
    setupCombo(mComboVoicingMode, { "Auto Mode", "MIDI Mode" },
        "VOICING - Auto derives the carrier pitch from the incoming voice, so no keyboard is "
        "needed. MIDI plays the carrier from notes you send, letting you sing one line and "
        "play chords under it.");
    setupCombo(mComboTrackResponse, { "Track: Fast", "Track: Natural", "Track: Smooth" },
        "TRACK RESPONSE - how quickly detected pitch is allowed to move. Fast keeps every "
        "inflection, Smooth irons out wobble and glitches at the cost of some expression.");
    setupCombo(mComboPitchQKey, { "Key: C", "Key: C#", "Key: D", "Key: D#", "Key: E", "Key: F",
                                  "Key: F#", "Key: G", "Key: G#", "Key: A", "Key: A#", "Key: B" },
        "KEY - root note that PITCH Q and M.PITCH snap to.");
    setupCombo(mComboPitchQScale, ScaleSnap::getScaleNames(),
        "SCALE - scale that PITCH Q snaps to. Chromatic allows every semitone; "
        "narrower scales give a stronger, more obviously tuned effect.");
    setupCombo(mComboLpcOrder, { "Order 8", "Order 10", "Order 12", "Order 16" },
        "LPC ORDER - number of poles used to model the vocal tract. 8-10 is the classic "
        "speech-chip sound, 16 resolves more formants and is clearer but less retro.");
    setupCombo(mComboAnalysisWindow, { "Hann Window", "Hamming Window", "Blackman Window" },
        "ANALYSIS WINDOW - shape applied before LPC analysis. Hann is neutral, Hamming is "
        "slightly sharper, Blackman is the smoothest and most stable on noisy input.");
    setupCombo(mComboFrameRate, { "8 Hz", "15 Hz", "25 Hz", "50 Hz", "80 Hz" },
        "FRAME RATE - how often the vocal tract model is re-analysed. Low rates make the voice "
        "step and stutter like an old speech chip; 50-80 Hz sounds natural.");
    setupCombo(mComboQuantBits, { "K: Off", "K: 6bit", "K: 5bit", "K: 4bit", "K: 3bit" },
        "K QUANT - bit depth of the vocal tract coefficients. Fewer bits makes the throat "
        "shape coarse and grainy, the core of the vintage speech-synth character.");
    setupCombo(mComboLpcInterpolation, { "Step Interp", "LSP Interp", "LAR Interp" },
        "INTERPOLATION - how the model morphs between analysis frames. Step jumps quickly and "
        "keeps the toy-like stepping; LSP and LAR glide across the whole frame for smooth, "
        "natural formant movement.");

    // 点灯式トグルボタン
    mBtnLimiter.setTooltip("LIMIT - brickwall limiter on the output, ceiling -0.1 dBFS. "
                           "Leave it on to catch peaks; switching it off is crossfaded so "
                           "there is no click.");
    mBtnFormantFreeze.setTooltip("FREEZE - LPC only. Stops updating the vocal tract model, "
                                 "holding the current vowel. Anything you play through it keeps "
                                 "that frozen mouth shape.");
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
    mAttachmentWidth         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "stereoWidth", mKnobWidth);
    mAttachmentAttack        = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "attack", mKnobAttack);
    mAttachmentDecay         = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "decay", mKnobDecay);
    mAttachmentSustain       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sustain", mKnobSustain);
    mAttachmentRelease       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", mKnobRelease);
    mAttachmentMix           = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "mix", mKnobMix);
    mAttachmentMasterPitch   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "masterPitch", mKnobMasterPitch);
    mAttachmentOutLevel      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputLevel", mKnobOutLevel);

    mAttachmentVocoderMode      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "vocoderMode", mComboVocoderMode);
    mAttachmentVoicingMode      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", mComboVoicingMode);
    mAttachmentTrackResponse    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "trackResponse", mComboTrackResponse);
    mAttachmentPitchQKey        = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "pitchQKey", mComboPitchQKey);
    mAttachmentPitchQScale      = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "pitchQScale", mComboPitchQScale);
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

    startTimerHz(30);   // MODレンジ帯の更新
}

VocoderPanel::~VocoderPanel()
{
    stopTimer();
    for (ValueKnob* k : { &mKnobCharacter, &mKnobTracking, &mKnobPitchQuantize, &mKnobFmtShift,
                          &mKnobFmtStretch, &mKnobLofi, &mKnobBasePitch, &mKnobNoiseColor,
                          &mKnobNoise, &mKnobBands, &mKnobResonance, &mKnobWidth,
                          &mKnobAttack, &mKnobDecay,
                          &mKnobSustain, &mKnobRelease, &mKnobMix, &mKnobMasterPitch,
                          &mKnobOutLevel })
        k->setLookAndFeel(nullptr);
}

// 変調レンジ帯 / ライブ位置ドットの更新。
//  BANDS はModMatrixの宛先から意図的に外している (変調でフィルタバンク再構築が
//  走りクリック音が出るため) ので、ここにも登場しない。
void VocoderPanel::timerCallback()
{
    using M = ModMatrix;
    const auto& mm = processor.getModMatrix();

    const std::pair<ValueKnob*, int> map[] = {
        { &mKnobCharacter,     M::DstCharacter },
        { &mKnobTracking,      M::DstTracking },
        { &mKnobPitchQuantize, M::DstPitchQuantize },
        { &mKnobFmtShift,      M::DstFormantShift },
        { &mKnobFmtStretch,    M::DstFormantStretch },
        { &mKnobLofi,          M::DstLofi },
        { &mKnobBasePitch,     M::DstBasePitch },
        { &mKnobNoiseColor,    M::DstNoiseColor },
        { &mKnobNoise,         M::DstNoise },
        { &mKnobResonance,     M::DstResonance },
        { &mKnobWidth,         M::DstStereoWidth },
        { &mKnobAttack,        M::DstAttack },
        { &mKnobDecay,         M::DstDecay },
        { &mKnobSustain,       M::DstSustain },
        { &mKnobRelease,       M::DstRelease },
        { &mKnobMix,           M::DstMix },
        { &mKnobMasterPitch,   M::DstMasterPitch },
        { &mKnobOutLevel,      M::DstOutLevel },
    };

    for (const auto& e : map)
        ModRing::apply(*e.first, mm, e.second);
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

    // FilterBank専用の表示 (RESONANCE / WIDTH は帯域合成そのものの設定なのでLPCでは無効)
    mKnobResonance.setVisible(!lpc);
    mLblResonance.setVisible(!lpc);
    mKnobWidth.setVisible(!lpc);
    mLblWidth.setVisible(!lpc);

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
    placeCombo(mComboPitchQKey);
    placeCombo(mComboPitchQScale);
    if (lpc)
    {
        placeCombo(mComboLpcOrder);
        placeCombo(mComboAnalysisWindow);
        placeCombo(mComboFrameRate);
        placeCombo(mComboQuantBits);
        placeCombo(mComboLpcInterpolation);
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
    {
        upper.push_back({ &mKnobResonance, &mLblResonance });
        upper.push_back({ &mKnobWidth,     &mLblWidth });
    }

    // 12ノブ(FBモード)のときは6列に詰めて2行へ収める / LPCは10ノブ=5列2行
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

    // 下段（1行）: ADSR + MIX + M.PITCH + OUT + ボタン列。
    // 下段エリア = 区切り線(divY) 〜 パネル下端。その中でノブ/ボタンを縦中央に配置する。
    // 列数はノブ7 + ボタン1 = 8 (M.PITCH追加で7→8になった)
    const int lcols = 8;
    const int lcolW = knobAreaW / lcols;
    const int lowerTop = uy0 + 2 * urowH - 2;        // paint()の区切り線と一致
    const int lowerBottom = r.getBottom();
    const int lowerAreaH = lowerBottom - lowerTop;

    // ノブ本体+ラベルの合計高で縦センタリング
    const int knobBlockH = knobSize + labelH;
    const int ly = lowerTop + (lowerAreaH - knobBlockH) / 2;

    ValueKnob* lower[7]    = { &mKnobAttack, &mKnobDecay, &mKnobSustain, &mKnobRelease,
                               &mKnobMix, &mKnobMasterPitch, &mKnobOutLevel };
    juce::Label* lowerL[7] = { &mLblAttack, &mLblDecay, &mLblSustain, &mLblRelease,
                               &mLblMix, &mLblMasterPitch, &mLblOutLevel };
    for (int i = 0; i < 7; ++i)
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
    const int bx = knobAreaX + 7 * lcolW + 3;
    mBtnLimiter.setBounds(bx, by, btnW, btnH);
    mBtnFormantFreeze.setBounds(bx, by + btnH + btnGap, btnW, btnH);
}
