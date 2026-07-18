// ==========================================
// File: PluginProcessor.cpp
// SPECTRA8 プロセッサー層 (フェーズ1軽量化設計)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

SPECTRA8AudioProcessor::SPECTRA8AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // EQゲインの初期値 1.0 (0dB) に設定
    for (int i = 0; i < 48; ++i)
    {
        mBandGains[(size_t)i].store(1.0f);
        mBandLevelsForUi[(size_t)i].store(0.0f);
    }
    mFormatManager.registerBasicFormats();   // wav/aiff リーダー
    mIsInitialized = true;
}

// ---- カスタムWavetable ----
bool SPECTRA8AudioProcessor::loadCustomWavetable(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(mFormatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    // 最大 64フレーム(2048smp/フレーム) まで読み込み・モノラルミックス
    const int maxLen = MorphWavetable::kMaxCustomFrames * MorphWavetable::kTableSize;
    const int len = (int)juce::jmin<juce::int64>(reader->lengthInSamples, (juce::int64)maxLen);
    if (len < 16)
        return false;

    juce::AudioBuffer<float> buf((int)reader->numChannels, len);
    if (!reader->read(&buf, 0, len, 0, true, true))
        return false;

    std::vector<float> mono((size_t)len, 0.0f);
    const float chScale = 1.0f / (float)juce::jmax(1u, (unsigned)reader->numChannels);
    for (int ch = 0; ch < (int)reader->numChannels; ++ch)
    {
        const float* src = buf.getReadPointer(ch);
        for (int n = 0; n < len; ++n)
            mono[(size_t)n] += src[n] * chScale;
    }

    if (!mExcitationEngine.getWavetable().loadCustomFromBuffer(mono.data(), len))
        return false;

    apvts.state.setProperty("customWavetablePath", file.getFullPathName(), nullptr);
    return true;
}

void SPECTRA8AudioProcessor::clearCustomWavetable()
{
    mExcitationEngine.getWavetable().clearCustom();
    apvts.state.removeProperty("customWavetablePath", nullptr);
}

SPECTRA8AudioProcessor::~SPECTRA8AudioProcessor()
{
    mIsInitialized = false;
}

juce::AudioProcessorValueTreeState::ParameterLayout SPECTRA8AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- ボコーダー基本パラメータ ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("character", 1), "Character", 0.0f, 1.0f, 1.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("vocoderMode", 1), "Vocoder Mode", juce::StringArray{ "Filterbank", "LPC Mode" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("limiterEnable", 1), "Limiter", juce::StringArray{ "Off", "On" }, 1));

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("bandCount", 1), "Band Count", 8, 48, 48));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantShift", 1), "Formant Shift", -24.0f, 24.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantStretch", 1), "Formant Stretch", 0.5f, 2.0f, 1.0f));

    // BPF Bank のバンド幅スケール (バンド間隔連動Qに乗算)。1.0=標準(中域で旧Q=10相当)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resonance", 1), "Resonance",
        juce::NormalisableRange<float>(0.3f, 3.0f, 0.0f, 0.5f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tracking", 1), "Tracking", 0.0f, 100.0f, 0.0f)); // レポート留意点5

    // Tracking応答速度: ピッチ追従のlog域平滑時定数 (Fast=2ms/Natural=6ms/Smooth=20ms)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("trackResponse", 1), "Track Response",
        juce::StringArray{ "Fast", "Natural", "Smooth" }, 1));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", 0.0f, 100.0f, 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputLevel", 1), "Output Level", -60.0f, 12.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pitchQuantize", 1), "Pitch Quantize", 0.0f, 100.0f, 0.0f));

    // PITCH Q のスナップ先: キー(ルート音)とスケール
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("pitchQKey", 1), "PitchQ Key",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("pitchQScale", 1), "PitchQ Scale",
        juce::StringArray{ "Chromatic", "Major", "Minor", "Maj Penta", "Min Penta" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray{ "Auto", "MIDI" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("formantFreeze", 1), "Formant Freeze", juce::StringArray{ "Off", "On" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("windowType", 1), "Window Type", juce::StringArray{ "Hann", "Hamming", "Blackman" }, 0));

    // M4: フレーム間フルホップ補間ドメイン。
    //  Step=従来のブロックランプ(低フレームレートでカクつくトイ感を保持)
    //  LSP/LAR=ホップ全長を掛けて滑らかにモーフ(Hi-Fi向き)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("interpolationMode", 1), "Interpolation Mode", juce::StringArray{ "Step", "LSP", "LAR" }, 0));

    // ※ filterbankType (Bandpass/Subtractive) は Subtractive LR4 廃止に伴い削除 (2026-07-18)

    // フェーズ2: LPC次数 (BitSpeekレトロ=10 / 高品位=16)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lpcOrder", 1), "LPC Order", juce::StringArray{ "8", "10", "12", "16" }, 3));

    // フェーズ2 M5: BitSpeekレトロ層
    //  FRAME RATE: 分析フレーム更新レート。低いほど声が「カクつく」トイ感。
    //  ※フリーズ(更新停止)は FREEZE ボタン(formantFreeze)に一本化した。
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("frameRate", 1), "Frame Rate",
        juce::StringArray{ "8 Hz", "15 Hz", "25 Hz", "50 Hz", "80 Hz" }, 3)); // 既定50Hz
    //  K QUANT: 反射係数のビット量子化。少ないほど声道が粗くBitSpeek的レトロ音に。
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lpcQuantBits", 1), "K Quant",
        juce::StringArray{ "Off", "6 bit", "5 bit", "4 bit", "3 bit" }, 0));

    // --- キャリアパラメータ ---
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("waveform", 1), "Waveform", juce::StringArray{ "Saw", "Pulse", "Wavetable" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("wavetablePosition", 1), "Wavetable Position", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pulseWidth", 1), "Pulse Width", 5.0f, 95.0f, 50.0f));

    // Detune: cent値と度数(音程名)の併記表示
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("detune", 1), "Detune",
        juce::NormalisableRange<float>(0.0f, 1200.0f), 5.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int)
            {
                static const char* names[13] = {
                    "1度", "短2度", "長2度", "短3度", "長3度",
                    "完全4度", "増4度", "完全5度", "短6度",
                    "長6度", "短7度", "長7度", "8度" };
                const int st = juce::jlimit(0, 12, (int)std::lround(v / 100.0f));
                return juce::String((int)std::lround(v)) + " ct (" + juce::String(juce::CharPointer_UTF8(names[st])) + ")";
            })));

    // Detune SNAP: ON時はDetuneノブが100セント(半音=度数)単位でスナップ (GUI挙動用)
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("detuneSnap", 1), "Detune Snap", false));

    // Detune Mode: ユニゾンデチューンの分散アルゴリズム
    //  Classic=従来 / Linear=均等拡散 / Exp=中心密・外側疎(スーパーソウ的) /
    //  Drift=ボイス毎ランダムウォーク(アナログ揺らぎ) / Chorus=低速LFO変調
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("detuneMode", 1), "Detune Mode",
        juce::StringArray{ "Classic", "Linear", "Exp", "Drift", "Chorus" }, 0));

    // Noise: BitSpeek式の双方向コントロール。
    //  Filterbank : 0〜100% がキャリアへのノイズ混入（負値は0扱い＝従来と同一）
    //  LPC        : -100%=ノイズ除去(純トーン) / 0%=自動V/UV追従 / +100%=全ノイズ(ウィスパー)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise", 1), "Noise", -100.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lofi", 1), "LoFi", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("porta", 1), "Portamento", 0.0f, 2.0f, 0.1f)); // ポルタメント

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("basePitch", 1), "Base Pitch", 50.0f, 500.0f, 130.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noiseColor", 1), "Noise Color", 
        juce::NormalisableRange<float>(100.0f, 10000.0f, 0.0f, 0.25f), 1000.0f));

    // MIDI用ADSR
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack", 0.001f, 5.0f, 0.01f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release", 0.001f, 5.0f, 0.2f));

    // --- モジュレーションマトリクスパラメータ (12スロット = ModMatrix::kNumSlots) ---
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const juce::String prefix = "slot" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "src", 1), prefix + " Source", ModMatrix::getSourceNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "dst", 1), prefix + " Dest", ModMatrix::getDestNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "amt", 1), prefix + " Amount", -1.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "uni", 1), prefix + " Uni", false));
    }

    // LFO (4基)
    for (int i = 0; i < 4; ++i)
    {
        const juce::String prefix = "lfo" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "rate", 1), prefix + " Rate", 0.01f, 50.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "sync", 1), prefix + " Sync", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "rateSync", 1), prefix + " Rate Sync", ModMatrix::getSyncRateNames(), 6));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "wave", 1), prefix + " Wave", ModMatrix::getWaveNames(), 0));
    }

    // ENV (3基)
    for (int i = 0; i < 3; ++i)
    {
        const juce::String prefix = "env" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "attack", 1), prefix + " Attack", 0.001f, 5.0f, 0.1f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "decay", 1), prefix + " Decay", 0.001f, 5.0f, 0.3f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "sustain", 1), prefix + " Sustain", 0.0f, 1.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "release", 1), prefix + " Release", 0.001f, 5.0f, 0.5f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "loop", 1), prefix + " Loop", false));
    }

    return layout;
}

void SPECTRA8AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mStoredSampleRate = sampleRate;

    mFilterbankVocoder.prepare(sampleRate);
    mLpcVocoder.prepare(sampleRate);
    mPostEq.prepare(LpcVocoder::kInternalSampleRate);   // ポストEQは16kHz内部レートで動作
    mExcitationEngine.prepare(sampleRate);
    mModMatrix.prepare(sampleRate);
    // 重要: PitchTracker へは 16kHz ダウンサンプル後のサンプル (processBlock の
    // 16kループ内 inSample) を供給しているため、prepare も 16kHz を渡す。
    // ホストレートを渡すと内部でさらに 1/3 デシメーションされ、検出ピッチが
    // ホスト48kHz時に常に3倍になる (Tracking使用時に音程が3倍になるバグの原因)。
    mPitchTracker.prepare(LpcVocoder::kInternalSampleRate);
    mLimiter.prepare(sampleRate);

    // ボコーダーモード切替状態の初期化 + PDC報告
    // (LPCモードは分析窓の群遅延 kLatency16k = 窓長/2 @16kHz)
    mCurVocoderMode = -1;
    mVocXfadeRemaining = 0;
    mVoicedSmooth = 0.0f;
    mPitchLogSmooth = -1.0f;   // Trackingピッチ平滑の再初期化
    mQuantNoteHeld = -1;       // PITCH Q保持音の再初期化

    // パラメータ・スムージング初期化 (20msランプ)
    mMixSm.reset(sampleRate, 0.02);
    mOutGainSm.reset(sampleRate, 0.02);
    mMixSm.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load() * 0.01f);
    mOutGainSm.setCurrentAndTargetValue(
        std::pow(10.0f, apvts.getRawParameterValue("outputLevel")->load() / 20.0f));
    mFmtShiftSm = apvts.getRawParameterValue("formantShift")->load();
    mFmtStretchSm = apvts.getRawParameterValue("formantStretch")->load();
    {
        const double lpcLatencySec = (double)LpcVocoder::kLatency16k / LpcVocoder::kInternalSampleRate;
        const int vm = (int)apvts.getRawParameterValue("vocoderMode")->load();
        setLatencySamples(vm == 1 ? (int)std::round(lpcLatencySec * sampleRate) : 0);
    }

    mDownsampleTimeAccum = 0.0;
    mUpsampleTimeAccum = 0.0;
    mControlRateCounter = 0;
    mInputEnvelope.store(0.0f);

    // バッファ確保
    int maxSafeSize = std::max(samplesPerBlock * 3, 4096);
    mDownsampledBuffer.assign((size_t)maxSafeSize, 0.0f);
    m16kWetL.assign((size_t)maxSafeSize, 0.0f);
    m16kWetR.assign((size_t)maxSafeSize, 0.0f);
}

void SPECTRA8AudioProcessor::releaseResources()
{
}

bool SPECTRA8AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SPECTRA8AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    if (!mIsInitialized)
    {
        buffer.clear();
        return;
    }

    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numInputs = getTotalNumInputChannels();
    
    if (numSamples <= 0) return;

    // 1. MIDIイベントのExcitationEngineへの供給
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            mExcitationEngine.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            mExcitationEngine.noteOff(msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            mExcitationEngine.allNotesOff();
    }
    mModMatrix.handleMidi(midiMessages); // モジュレーションマトリクス用
    midiMessages.clear();

    // 2. 入力音声の包絡線（エンベロープ）算出
    float inputAvgAbs = 0.0f;
    if (numInputs > 0)
    {
        const float* readPtr = buffer.getReadPointer(0);
        float sumAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sumAbs += std::abs(readPtr[i]);
        inputAvgAbs = sumAbs / (float)numSamples;
    }
    // 指数平滑化
    float coeff = (inputAvgAbs > mInputEnvelope.load()) ? 0.05f : 0.005f;
    mInputEnvelope.store(mInputEnvelope.load() * (1.0f - coeff) + inputAvgAbs * coeff);

    float effectiveCharacter = apvts.getRawParameterValue("character")->load();
    float effectiveFormantShift = apvts.getRawParameterValue("formantShift")->load();
    float effectiveFormantStretch = apvts.getRawParameterValue("formantStretch")->load();

    // 3. ダウンサンプリング処理 (16kHzへ)
    int num16kSamples = 0;
    if (numInputs > 0 && mStoredSampleRate > 0.0)
    {
        const float* inputL = buffer.getReadPointer(0);
        double step = mStoredSampleRate / 16000.0;
        int maxSafeSize = (int)mDownsampledBuffer.size();

        while (mDownsampleTimeAccum < (double)numSamples)
        {
            int idx0 = (int)mDownsampleTimeAccum;
            int idx1 = std::min(numSamples - 1, idx0 + 1);
            float frac = (float)(mDownsampleTimeAccum - idx0);

            float val = inputL[idx0] * (1.0f - frac) + inputL[idx1] * frac;

            if (num16kSamples < maxSafeSize)
            {
                mDownsampledBuffer[(size_t)num16kSamples] = val;
                num16kSamples++;
            }
            mDownsampleTimeAccum += step;
        }
        mDownsampleTimeAccum -= (double)numSamples;
    }

    // 4. 16kHz領域でのDSP処理ループ
    const int mode = static_cast<int>(apvts.getRawParameterValue("mode")->load());
    const bool isMidiMode = (mode == 1);

    if (mPrevMode != mode)
    {
        mExcitationEngine.reset();
        mFilterbankVocoder.reset();
        mLpcVocoder.reset();
        mPrevMode = mode;
    }

    // ボコーダーモード切替の検出 (30ms等パワークロスフェード + PDC更新)
    const int vocoderMode = (int)apvts.getRawParameterValue("vocoderMode")->load();
    if (mCurVocoderMode < 0)
    {
        mCurVocoderMode = vocoderMode;   // 初回は即時適用
    }
    else if (vocoderMode != mCurVocoderMode)
    {
        mCurVocoderMode = vocoderMode;
        mVocXfadeRemaining = kVocXfadeLen;
        // 切り替わり先モジュールの状態をクリアしてから立ち上げる
        if (vocoderMode == 1) mLpcVocoder.reset(); else mFilterbankVocoder.reset();
        const double lpcLatencySec = (double)LpcVocoder::kLatency16k / LpcVocoder::kInternalSampleRate;
        setLatencySamples(vocoderMode == 1 ? (int)std::round(lpcLatencySec * mStoredSampleRate) : 0);
    }

    // LPCモード用パラメータ (ブロック毎に1回読む)
    static constexpr int kLpcOrderMap[4] = { 8, 10, 12, 16 };
    const int lpcOrderIdx = juce::jlimit(0, 3, (int)apvts.getRawParameterValue("lpcOrder")->load());
    const int lpcOrder = kLpcOrderMap[lpcOrderIdx];
    const bool lpcFreeze = (apvts.getRawParameterValue("formantFreeze")->load() >= 0.5f);
    mLpcVocoder.setWindowType((int)apvts.getRawParameterValue("windowType")->load());

    // M5: FRAME RATE / k量子化（ブロック毎に設定）
    static constexpr float kFrameRateMap[5] = { 8.0f, 15.0f, 25.0f, 50.0f, 80.0f };
    const int frIdx = juce::jlimit(0, 4, (int)apvts.getRawParameterValue("frameRate")->load());
    mLpcVocoder.setFrameRate(kFrameRateMap[frIdx]);
    static constexpr int kQuantBitsMap[5] = { 0, 6, 5, 4, 3 };
    const int qbIdx = juce::jlimit(0, 4, (int)apvts.getRawParameterValue("lpcQuantBits")->load());
    mLpcVocoder.setQuantBits(kQuantBitsMap[qbIdx]);

    // M4: フルホップ補間ドメイン (0=Step/1=LSP/2=LAR)。次の分析フレームから適用。
    mLpcVocoder.setInterpolationMode((int)apvts.getRawParameterValue("interpolationMode")->load());

    // Tracking応答: log2領域1次平滑の係数をブロック毎に算出 (Fast=2ms/Natural=6ms/Smooth=20ms)
    {
        static constexpr float kRespTau[3] = { 0.002f, 0.006f, 0.020f };
        const int respIdx = juce::jlimit(0, 2, (int)apvts.getRawParameterValue("trackResponse")->load());
        mPitchSmoothCoef = 1.0f - std::exp(-1.0f / (kRespTau[respIdx] * 16000.0f));
    }

    // ポストEQ(LPC出力用)の係数をブロック毎に更新。BANDS EQの帯域ゲインを反映する。
    mPostEq.updateCoeffs((int)apvts.getRawParameterValue("bandCount")->load(), mBandGains);

    for (int s = 0; s < num16kSamples; ++s)
    {
        const float inSample = mDownsampledBuffer[(size_t)s];

        // ピッチ検出器にサンプル供給
        mPitchTracker.pushSample(inSample);

        // コントロールレート同期 (32サンプルごと)
        if (mControlRateCounter >= 32 || mControlRateCounter == 0)
        {
            mControlRateCounter = 0;

            // DAWのテンポ情報を取得
            double bpm = 120.0;
            if (auto* pH = getPlayHead())
            {
                if (auto info = pH->getPosition())
                {
                    if (info->getBpm().hasValue())
                        bpm = *(info->getBpm());
                }
            }

            // APVTSからパラメータ値をモジュレーションマトリクス用構造体にロード
            mModParams.bpm = bpm;
            for (int i = 0; i < ModMatrix::kNumSlots; ++i)
            {
                const juce::String prefix = "slot" + juce::String(i);
                mModParams.slot[(size_t)i].src = (int)(apvts.getRawParameterValue(prefix + "src")->load());
                mModParams.slot[(size_t)i].dst = (int)(apvts.getRawParameterValue(prefix + "dst")->load());
                mModParams.slot[(size_t)i].amt = apvts.getRawParameterValue(prefix + "amt")->load();
                mModParams.slot[(size_t)i].uni = (apvts.getRawParameterValue(prefix + "uni")->load() >= 0.5f);
            }
            for (int i = 0; i < 4; ++i)
            {
                const juce::String prefix = "lfo" + juce::String(i);
                mModParams.lfo[(size_t)i].rateHz = apvts.getRawParameterValue(prefix + "rate")->load();
                mModParams.lfo[(size_t)i].sync = (apvts.getRawParameterValue(prefix + "sync")->load() >= 0.5f);
                mModParams.lfo[(size_t)i].rateSync = (int)(apvts.getRawParameterValue(prefix + "rateSync")->load());
                mModParams.lfo[(size_t)i].wave = (int)(apvts.getRawParameterValue(prefix + "wave")->load());
            }
            for (int i = 0; i < 3; ++i)
            {
                const juce::String prefix = "env" + juce::String(i);
                mModParams.env[(size_t)i].attack = apvts.getRawParameterValue(prefix + "attack")->load();
                mModParams.env[(size_t)i].decay = apvts.getRawParameterValue(prefix + "decay")->load();
                mModParams.env[(size_t)i].sustain = apvts.getRawParameterValue(prefix + "sustain")->load();
                mModParams.env[(size_t)i].release = apvts.getRawParameterValue(prefix + "release")->load();
                mModParams.env[(size_t)i].loop = (apvts.getRawParameterValue(prefix + "loop")->load() >= 0.5f);
            }

            mModMatrix.processBlock(32, mModParams);

            // モジュレーションが適用された実効値の算出
            auto getModVal = [this](const juce::String& paramId, int dstType) -> float
            {
                float base = apvts.getRawParameterValue(paramId)->load();
                float mod = mModMatrix.get(dstType) * ModMatrix::destScale(dstType);
                return base + mod;
            };

            effectiveCharacter = juce::jlimit(0.0f, 1.0f, getModVal("character", ModMatrix::DstCharacter));
            effectiveFormantShift = juce::jlimit(-24.0f, 24.0f, getModVal("formantShift", ModMatrix::DstFormantShift));
            effectiveFormantStretch = juce::jlimit(0.5f, 2.0f, getModVal("formantStretch", ModMatrix::DstFormantStretch));
            
            const float wtPos = juce::jlimit(0.0f, 1.0f, getModVal("wavetablePosition", ModMatrix::DstWtPos));
            const float pulseWidth = juce::jlimit(5.0f, 95.0f, getModVal("pulseWidth", ModMatrix::DstPulseWidth)) * 0.01f;
            const float detune = juce::jlimit(0.0f, 1200.0f, getModVal("detune", ModMatrix::DstDetune));
            const float lofi = juce::jlimit(0.0f, 1.0f, getModVal("lofi", ModMatrix::DstLofi));

            // --- NOISE± (BitSpeek式) → ExcitationEngineへ渡す実効ノイズmix(0..1)を算出 ---
            const float noiseSigned = juce::jlimit(-100.0f, 100.0f, getModVal("noise", ModMatrix::DstNoise)) * 0.01f; // -1..+1
            // 有声度の平滑化(入力音声のV/UV)。無声ほど自動でノイズ励起へ。
            const float voicedNow = mPitchTracker.isVoiced() ? 1.0f : 0.0f;
            mVoicedSmooth += 0.25f * (voicedNow - mVoicedSmooth);
            float noise; // ExcitationEngine::syncParameters が受け取る 0..1 のmix
            if (mCurVocoderMode == 1) // LPCモード: -1=除去 / 0=自動V/UV / +1=全ノイズ
            {
                const float autoMix = 1.0f - mVoicedSmooth;            // 無声=1.0, 有声=0.0
                noise = (noiseSigned >= 0.0f)
                          ? autoMix + noiseSigned * (1.0f - autoMix)   // 0→auto, +1→1.0
                          : autoMix * (1.0f + noiseSigned);            // 0→auto, -1→0.0
            }
            else // Filterbankモード: 従来通り(負値は0)
            {
                noise = juce::jmax(0.0f, noiseSigned);
            }

            const float porta = apvts.getRawParameterValue("porta")->load();
            const int waveform = (int)(apvts.getRawParameterValue("waveform")->load());

            const float attack = apvts.getRawParameterValue("attack")->load();
            const float decay = apvts.getRawParameterValue("decay")->load();
            const float sustain = apvts.getRawParameterValue("sustain")->load();
            const float release = apvts.getRawParameterValue("release")->load();
            
            float noiseColor = 1000.0f;
            if (auto* p = apvts.getRawParameterValue("noiseColor"))
                noiseColor = p->load();

            const int detuneMode = (int)(apvts.getRawParameterValue("detuneMode")->load());

            // モジュール側の同期
            mExcitationEngine.syncParameters(waveform, wtPos, pulseWidth, detune, noise, lofi, porta,
                                              attack, decay, sustain, release, noiseColor, detuneMode);
        }
        mControlRateCounter++;

        // 励起信号（キャリア）の生成
        float carrierL = 0.0f;
        float carrierR = 0.0f;
        
        // 有声音ピッチの取得。
        //  旧実装の「±20セントデッドバンド保持→超えたら一括ジャンプ」は、自然な
        //  イントネーションを階段状にしてうねり/ポルタメント風アーティファクトの
        //  原因になっていたため廃止。代わりに生ピッチを log2 領域の1次平滑
        //  (Responseパラメータ: Fast/Natural/Smooth) に通す。
        //  微小ジッタは平滑が吸収し、原音の抑揚(ピッチ感)はそのまま保たれる。
        float targetHz = mLastVoicedPitch;
        if (mPitchTracker.isVoiced())
        {
            targetHz = mPitchTracker.getRawPitchHz();
            mLastVoicedPitch = targetHz;
        }
        targetHz = juce::jlimit(30.0f, 4000.0f, targetHz);

        if (mPitchLogSmooth < 0.0f)
            mPitchLogSmooth = std::log2(targetHz);   // 初回は即値
        mPitchLogSmooth += mPitchSmoothCoef * (std::log2(targetHz) - mPitchLogSmooth);
        const float pitchHz = std::exp2(mPitchLogSmooth);

        // Tracking パラメータの適用 (Autoモードでも0%のときは基準ピッチに固定しうねりを防止)
        float basePitch = apvts.getRawParameterValue("basePitch")->load();
        float tracking = apvts.getRawParameterValue("tracking")->load() * 0.01f;
        float activePitch = basePitch + (pitchHz - basePitch) * tracking;

        // 安全対策: activePitch が異常値のときは basePitch に戻す
        if (std::isnan(activePitch) || activePitch <= 20.0f || activePitch > 8000.0f)
            activePitch = basePitch;

        // ケロケロ（ピッチ量子化）の適用: Key/Scaleスナップ + ヒステリシス
        //  - スケールマスクで許可音のみにスナップ(Chromatic=全音)
        //  - ヒステリシス: 現在保持中の音より「0.3半音以上近い」別の許可音が
        //    現れたときだけ切替える。境界付近での音程チャタリング(ワブル)を防止。
        float qAmt = apvts.getRawParameterValue("pitchQuantize")->load() * 0.01f;
        if (qAmt > 0.001f && activePitch > 20.0f && !std::isnan(activePitch))
        {
            static constexpr uint16_t kScaleMasks[5] = {
                0b111111111111,  // Chromatic
                0b101010110101,  // Major     {0,2,4,5,7,9,11}
                0b010110101101,  // Minor(nat){0,2,3,5,7,8,10}
                0b001010010101,  // MajPenta  {0,2,4,7,9}
                0b010010101001,  // MinPenta  {0,3,5,7,10}
            };
            const int key   = juce::jlimit(0, 11, (int)apvts.getRawParameterValue("pitchQKey")->load());
            const int scale = juce::jlimit(0, 4,  (int)apvts.getRawParameterValue("pitchQScale")->load());
            const uint16_t mask = kScaleMasks[scale];

            const float cont = 12.0f * std::log2(activePitch / 440.0f) + 69.0f; // 連続MIDIノート値
            auto isAllowed = [&](int n) {
                const int deg = ((n - key) % 12 + 12) % 12;
                return (mask >> deg) & 1;
            };
            // 最近傍の許可音を探索 (±1オクターブで必ず見つかる)
            int nearest = (int)std::lround(cont);
            float bestDist = 1e9f;
            for (int d = -12; d <= 12; ++d)
            {
                const int n = (int)std::lround(cont) + d;
                if (!isAllowed(n)) continue;
                const float dist = std::abs(cont - (float)n);
                if (dist < bestDist) { bestDist = dist; nearest = n; }
            }
            // ヒステリシス判定
            if (mQuantNoteHeld < 0 || !isAllowed(mQuantNoteHeld)
                || bestDist + 0.3f < std::abs(cont - (float)mQuantNoteHeld))
                mQuantNoteHeld = nearest;

            const float qPitch = 440.0f * std::pow(2.0f, ((float)mQuantNoteHeld - 69.0f) / 12.0f);
            if (!std::isnan(qPitch) && qPitch > 20.0f)
                activePitch = activePitch + (qPitch - activePitch) * qAmt;
        }
        else
        {
            mQuantNoteHeld = -1; // PITCH Q無効時は保持解除
        }

        // FMT SHIFT/STRETCH の一次平滑 (τ≈5ms@16k)。制御ブロック毎(2ms)の階段状変化に
        // よるフィルタ係数ジャンプ/ジッパーノイズを防ぐ。
        constexpr float kFmtSmCoef = 0.0124f;   // 1-exp(-1/(0.005*16000))
        mFmtShiftSm   += kFmtSmCoef * (effectiveFormantShift   - mFmtShiftSm);
        mFmtStretchSm += kFmtSmCoef * (effectiveFormantStretch - mFmtStretchSm);

        mExcitationEngine.processSample(carrierL, carrierR, activePitch, isMidiMode);

        // ボコーディング処理 (vocoderMode: 0=Filterbank / 1=LPC)
        float wetL = 0.0f;
        float wetR = 0.0f;

        const int bandCount = (int)apvts.getRawParameterValue("bandCount")->load();
        const float resonance = apvts.getRawParameterValue("resonance")->load();

        auto renderFilterbank = [&](float& l, float& r)
        {
            mFilterbankVocoder.processSample(inSample, carrierL, carrierR, l, r,
                                             bandCount, effectiveCharacter, resonance,
                                             mFmtShiftSm, mFmtStretchSm,
                                             1.0f, mBandGains, mBandLevelsForUi);
        };
        // character(0..1) → 帯域拡張γ(0.97=ぼやけ 〜 0.998=シャープ) へマッピング (M3)
        const float lpcGamma = 0.970f + 0.028f * juce::jlimit(0.0f, 1.0f, effectiveCharacter);
        auto renderLpc = [&](float& l, float& r)
        {
            // FMT SHIFT(リサンプル比) / FMT STRETCH(LSP領域の間隔伸縮)をLPCへ渡す
            mLpcVocoder.processSample(inSample, carrierL, carrierR, l, r, lpcOrder, lpcFreeze,
                                      lpcGamma, mFmtShiftSm, mFmtStretchSm);
            // BANDS EQ をポストEQとしてLPC出力へ適用 (クロスフェード時もLPC側のみに掛かる)
            mPostEq.process(l, r);
        };

        if (mVocXfadeRemaining > 0)
        {
            // 30ms等パワークロスフェード (旧→新)
            float oldL = 0.0f, oldR = 0.0f, newL = 0.0f, newR = 0.0f;
            if (mCurVocoderMode == 1) { renderFilterbank(oldL, oldR); renderLpc(newL, newR); }
            else                      { renderLpc(oldL, oldR); renderFilterbank(newL, newR); }

            const float t = 1.0f - (float)mVocXfadeRemaining / (float)kVocXfadeLen;
            const float gOld = std::cos(t * juce::MathConstants<float>::halfPi);
            const float gNew = std::sin(t * juce::MathConstants<float>::halfPi);
            wetL = oldL * gOld + newL * gNew;
            wetR = oldR * gOld + newR * gNew;
            --mVocXfadeRemaining;
        }
        else if (mCurVocoderMode == 1)
        {
            renderLpc(wetL, wetR);
            // LPCモードはフィルターバンク合成を行わないため、そのままだと
            // BANDS EQ のアナライザー(帯域レベルメーター)が更新されず止まってしまう。
            // 分析専用パスを呼び、入力音声の帯域レベルだけをメーターへ反映する。
            // (クロスフェード中は renderFilterbank 側で分析が走るので、ここでは呼ばない=二重処理を防止)
            mFilterbankVocoder.analyzeForMeter(inSample, bandCount, effectiveCharacter, resonance,
                                               mBandLevelsForUi);
        }
        else
        {
            renderFilterbank(wetL, wetR);
        }

        m16kWetL[(size_t)s] = wetL;
        m16kWetR[(size_t)s] = wetR;
    }

    // 5. アップサンプリング & ドライ・ウェットブレンド
    //    MIX/OUT LEVEL は20msランプでサンプル毎に平滑 (ジッパーノイズ対策)
    mMixSm.setTargetValue(apvts.getRawParameterValue("mix")->load() * 0.01f);
    mOutGainSm.setTargetValue(
        std::pow(10.0f, apvts.getRawParameterValue("outputLevel")->load() / 20.0f));

    // ゲートゲインの算出 (リニア入力エンベロープに基づくソフトゲート)
    float envVal = mInputEnvelope.load();
    float threshold = 0.005f; // ナレーション等の合間の極小ノイズを遮断するしきい値
    float gateGain = 1.0f;
    if (!isMidiMode)
    {
        if (envVal < threshold)
            gateGain = 0.0f;
        else if (envVal < threshold + 0.005f)
            gateGain = (envVal - threshold) / 0.005f; // 0.005〜0.01の間で滑らかにフェード
    }

    float* writeL = buffer.getWritePointer(0);
    float* writeR = (numInputs > 1) ? buffer.getWritePointer(1) : writeL;

    if (numInputs > 0 && mStoredSampleRate > 0.0)
    {
        double step = 16000.0 / mStoredSampleRate;
        for (int i = 0; i < numSamples; ++i)
        {
            // 線形補間アップサンプリング
            int idx0 = (int)mUpsampleTimeAccum;
            int idx1 = std::min(num16kSamples - 1, idx0 + 1);
            float frac = (float)(mUpsampleTimeAccum - idx0);

            float wetSampleL = (m16kWetL[(size_t)idx0] * (1.0f - frac) + m16kWetL[(size_t)idx1] * frac) * gateGain;
            float wetSampleR = (m16kWetR[(size_t)idx0] * (1.0f - frac) + m16kWetR[(size_t)idx1] * frac) * gateGain;

            mUpsampleTimeAccum += step;

            float dryL = writeL[i];
            float dryR = (numInputs > 1) ? writeR[i] : dryL;

            // ブレンド & ゲイン (サンプル毎スムージング)
            const float mix = mMixSm.getNextValue();
            const float outGain = mOutGainSm.getNextValue();
            writeL[i] = (dryL * (1.0f - mix) + wetSampleL * mix) * outGain;
            writeR[i] = (dryR * (1.0f - mix) + wetSampleR * mix) * outGain;
        }
        mUpsampleTimeAccum -= (double)num16kSamples;
    }

    // 6. 最終段リミッター
    const bool limiter = (static_cast<int>(apvts.getRawParameterValue("limiterEnable")->load()) == 1);
    if (limiter)
    {
        // 天井は内部固定 -0.1 dBFS (BrickLimiter::kCeiling)。突発ピークも天井へ抑える。
        mLimiter.process(writeL, writeR, numSamples);
    }
}

juce::AudioProcessorEditor* SPECTRA8AudioProcessor::createEditor()
{
    return new SPECTRA8AudioProcessorEditor(*this);
}

void SPECTRA8AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    
    // Band EQ ゲインを追加でXMLにシリアライズ
    auto* eqNode = xml->createNewChildElement("BAND_EQ_GAINS");
    for (int i = 0; i < 48; ++i)
    {
        eqNode->setAttribute("gain" + juce::String(i), (double)mBandGains[(size_t)i].load());
    }

    copyXmlToBinary(*xml, destData);
}

void SPECTRA8AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }

        // Band EQ ゲインの復元
        if (auto* eqNode = xmlState->getChildByName("BAND_EQ_GAINS"))
        {
            for (int i = 0; i < 48; ++i)
            {
                float g = (float)eqNode->getDoubleAttribute("gain" + juce::String(i), 1.0);
                mBandGains[(size_t)i].store(g);
            }
        }

        // カスタムWavetableの復元 (パスが保存されていればロード)。
        // ファイルIO/FFTを伴うためメッセージスレッドで実行する。
        // 非同期実行時はWeakReferenceで生存確認 (プロセッサ破棄後のダングリング防止)。
        const juce::String wtPath = getCustomWavetablePath();
        if (wtPath.isNotEmpty())
        {
            juce::WeakReference<SPECTRA8AudioProcessor> wp(this);
            auto doLoad = [wp, wtPath]
            {
                if (auto* p = wp.get())
                {
                    const juce::File f(wtPath);
                    if (f.existsAsFile())
                        p->loadCustomWavetable(f);
                }
            };
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                doLoad();
            else
                juce::MessageManager::callAsync(doLoad);
        }
    }
}

// JUCEにこのプラグインを生成するコールバックを教える
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SPECTRA8AudioProcessor();
}