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
}

SPECTRA8AudioProcessor::~SPECTRA8AudioProcessor()
{
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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tracking", 1), "Tracking", 0.0f, 100.0f, 0.0f)); // レポート留意点5

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", 0.0f, 100.0f, 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputLevel", 1), "Output Level", -60.0f, 12.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray{ "Auto", "MIDI" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("formantFreeze", 1), "Formant Freeze", juce::StringArray{ "Off", "On" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("windowType", 1), "Window Type", juce::StringArray{ "Hann", "Hamming", "Blackman" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("interpolationMode", 1), "Interpolation Mode", juce::StringArray{ "LSP", "LAR" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("filterbankType", 1), "Filterbank Type", juce::StringArray{ "Bandpass", "Subtractive" }, 0));

    // --- キャリアパラメータ ---
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("waveform", 1), "Waveform", juce::StringArray{ "Saw", "Pulse", "Wavetable" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("wavetablePosition", 1), "Wavetable Position", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pulseWidth", 1), "Pulse Width", 5.0f, 95.0f, 50.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("detune", 1), "Detune", 0.0f, 1200.0f, 5.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise", 1), "Noise", 0.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lofi", 1), "LoFi", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("porta", 1), "Portamento", 0.0f, 2.0f, 0.1f)); // ポルタメント

    // MIDI用ADSR
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack", 0.001f, 5.0f, 0.01f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release", 0.001f, 5.0f, 0.2f));

    // --- モジュレーションマトリクスパラメータ (16スロット) ---
    for (int i = 0; i < 16; ++i)
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
    mExcitationEngine.prepare(sampleRate);
    mModMatrix.prepare(sampleRate);
    mPitchTracker.prepare(sampleRate);
    mLimiter.prepare(sampleRate);

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
        mPrevMode = mode;
    }

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
            for (int i = 0; i < 16; ++i)
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
            const float noise = juce::jlimit(0.0f, 100.0f, getModVal("noise", ModMatrix::DstNoise)) * 0.01f;
            const float lofi = juce::jlimit(0.0f, 1.0f, getModVal("lofi", ModMatrix::DstLofi));

            const float porta = apvts.getRawParameterValue("porta")->load();
            const int waveform = (int)(apvts.getRawParameterValue("waveform")->load());

            const float attack = apvts.getRawParameterValue("attack")->load();
            const float decay = apvts.getRawParameterValue("decay")->load();
            const float sustain = apvts.getRawParameterValue("sustain")->load();
            const float release = apvts.getRawParameterValue("release")->load();

            // モジュール側の同期
            mExcitationEngine.syncParameters(waveform, wtPos, pulseWidth, detune, noise, lofi, porta,
                                              attack, decay, sustain, release);
        }
        mControlRateCounter++;

        // 励起信号（キャリア）の生成
        float carrierL = 0.0f;
        float carrierR = 0.0f;
        
        // 有声音ピッチの取得 (トラッカーの値)
        float pitchHz = mPitchTracker.isVoiced() ? mPitchTracker.getPitchHz() : 130.0f;
        // Tracking パラメータの適用
        float tracking = apvts.getRawParameterValue("tracking")->load() * 0.01f;
        // MIDIモードでない場合は Tracking=100% の時に入力ピッチを完璧に追従
        float activePitch = isMidiMode ? (130.0f + (pitchHz - 130.0f) * tracking) : pitchHz;

        mExcitationEngine.processSample(carrierL, carrierR, activePitch, isMidiMode);

        // ボコーディング処理
        float wetL = 0.0f;
        float wetR = 0.0f;

        const int bandCount = (int)apvts.getRawParameterValue("bandCount")->load();
        const int filterbankType = (int)apvts.getRawParameterValue("filterbankType")->load();

        mFilterbankVocoder.processSample(inSample, carrierL, carrierR, wetL, wetR,
                                         bandCount, effectiveCharacter, effectiveFormantShift, effectiveFormantStretch,
                                         filterbankType, 1.0f, mBandGains, mBandLevelsForUi);

        m16kWetL[(size_t)s] = wetL;
        m16kWetR[(size_t)s] = wetR;
    }

    // 5. アップサンプリング & ドライ・ウェットブレンド
    float mix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    float outLevelDb = apvts.getRawParameterValue("outputLevel")->load();
    float outGain = std::pow(10.0f, outLevelDb / 20.0f);

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

            // ブレンド & ゲイン
            writeL[i] = (dryL * (1.0f - mix) + wetSampleL * mix) * outGain;
            writeR[i] = (dryR * (1.0f - mix) + wetSampleR * mix) * outGain;
        }
        mUpsampleTimeAccum -= (double)num16kSamples;
    }

    // 6. 最終段リミッター
    const bool limiter = (static_cast<int>(apvts.getRawParameterValue("limiterEnable")->load()) == 1);
    if (limiter)
    {
        mLimiter.process(writeL, writeR, numSamples, 0.985f);
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
    }
}

// JUCEにこのプラグインを生成するコールバックを教える
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SPECTRA8AudioProcessor();
}