// ==========================================
// File: ExcitationEngine.cpp
// キャリア励起信号生成エンジン (8ボイス・スカラー設計)
// ==========================================
#include "ExcitationEngine.h"

ExcitationEngine::ExcitationEngine()
{
    reset();
}

void ExcitationEngine::prepare(double sampleRate)
{
    // 内部は常に 16kHz で動作するため、ホストサンプルレートは未使用
    juce::ignoreUnused(sampleRate);
    reset();
}

void ExcitationEngine::reset()
{
    mTriggerCounter = 0;
    mLastTriggeredFreq = 130.0f;
    mDetuneSm = mDetuneCents;
    mNoiseSm = mNoiseMix;
    mLofiRateCounter = 0.0f;
    mLofiLastValL = 0.0f;
    mLofiLastValR = 0.0f;

    for (auto& v : mVoices)
    {
        v.active = false;
        v.noteNumber = -1;
        v.velocity = 0.0f;
        v.triggerTime = 0;
        v.targetFreq = 130.0f;
        v.currentFreq = 130.0f;
        v.phaseL = 0.0f;
        v.phaseR = 0.0f;
        v.stage = Voice::Idle;
        v.envValue = 0.0f;
        v.releaseStartVal = 0.0f;
    }

    mNoiseFilterL.reset();
    mNoiseFilterR.reset();
    for (int j = 0; j < 3; ++j)
    {
        mVocFilterL[j].reset();
        mVocFilterR[j].reset();
    }

    mDriftVal.fill(0.0f);
    for (int i = 0; i < kMaxVoices; ++i)
        mChorusPhase[(size_t)i] = 0.785f * (float)i;   // ボイス毎に位相をずらす
}

void ExcitationEngine::noteOn(int noteNumber, float velocity) noexcept
{
    if (velocity <= 0.0f)
    {
        noteOff(noteNumber);
        return;
    }

    triggerVoice(noteNumber, velocity);
}

void ExcitationEngine::noteOff(int noteNumber) noexcept
{
    for (auto& v : mVoices)
    {
        if (v.active && v.noteNumber == noteNumber)
        {
            v.stage = Voice::Release;
            v.releaseStartVal = v.envValue;
        }
    }
}

void ExcitationEngine::allNotesOff() noexcept
{
    for (auto& v : mVoices)
    {
        if (v.active)
        {
            v.stage = Voice::Release;
            v.releaseStartVal = v.envValue;
        }
    }
}

void ExcitationEngine::syncParameters(int waveform, float wtPos, float pulseWidth, float detuneCents,
                                    float noiseMix, float lofi, float portaTimeSec,
                                    float attackSec, float decaySec, float sustainVal, float releaseSec,
                                    float noiseColorHz, int detuneMode,
                                    float bendAmt, float bendShift,
                                    float syncAmt, float syncShift,
                                    float vocAmt, float vocShift) noexcept
{
    mWaveform = juce::jlimit(0, 2, waveform);
    mDetuneMode = juce::jlimit(0, 4, detuneMode);

    // ---- Morph 事前計算 (BassSynth precomputeWarp 準拠) ----
    //  Bend / Sync / Vocode は独立ノブで、それぞれ Amt=0 のとき自動バイパス。
    //  同時に掛けた場合は Bend → Sync → Vocode の順に適用される。
    {
        const float bA = juce::jlimit(-1.0f, 1.0f, bendAmt);
        mBendOn = std::abs(bA) > 0.001f;
        if (mBendOn)
        {
            mBendSym = juce::jlimit(0.01f, 0.99f, 0.5f + juce::jlimit(-1.0f, 1.0f, bendShift) * 0.49f);
            mBendB = std::exp(-juce::jlimit(-0.99f, 0.99f, bA) * 2.0f);
        }
    }
    {
        const float sA = juce::jlimit(0.0f, 1.0f, syncAmt);
        mSyncOn = sA > 0.001f;
        if (mSyncOn)
        {
            mSyncSt = 1.0f + sA * 7.0f;
            // ※BassSynthではShift未接続だったのを有効化
            mSyncShiftHalf = juce::jlimit(-1.0f, 1.0f, syncShift) * 0.5f;
        }
    }
    {
        const bool wasOn = (mVocAmt > 0.001f);
        mVocAmt = juce::jlimit(0.0f, 1.0f, vocAmt);
        if (wasOn && mVocAmt <= 0.001f)
        {
            // OFFへ落ちた瞬間に共振状態を捨てる (再ONでの残響ポップ防止)
            for (int j = 0; j < 3; ++j) { mVocFilterL[j].reset(); mVocFilterR[j].reset(); }
        }
        if (mVocAmt > 0.001f)
        {
            // BassSynth SpectralMorphProcessor mode9 のテーブル (値=倍音番号)
            static const float kFmts[5][3] = {
                { 32.0f, 55.0f, 120.0f },   // A
                { 14.0f, 102.0f, 139.0f },  // I
                { 14.0f, 41.0f, 116.0f },   // U
                { 18.0f, 74.0f, 111.0f },   // E
                { 18.0f, 37.0f, 120.0f },   // O
            };
            float s = juce::jlimit(-1.0f, 1.0f, vocShift);
            int seq[5];
            if (s >= 0.0f) { seq[0]=0; seq[1]=1; seq[2]=2; seq[3]=3; seq[4]=4; }        // A,I,U,E,O
            else           { seq[0]=0; seq[1]=3; seq[2]=1; seq[3]=4; seq[4]=2; s = -s; } // A,E,I,O,U
            const float pos = s * 4.0f;
            const int i0 = juce::jlimit(0, 3, (int)pos);
            const int i1 = i0 + 1;
            const float frac = pos - (float)i0;
            for (int j = 0; j < 3; ++j)
                mVocHarm[j] = kFmts[seq[i0]][j] * (1.0f - frac) + kFmts[seq[i1]][j] * frac;
        }
    }
    mWtPos = juce::jlimit(0.0f, 1.0f, wtPos);
    mPulseWidth = juce::jlimit(0.05f, 0.95f, pulseWidth);
    mDetuneCents = juce::jlimit(0.0f, 1200.0f, detuneCents);
    mNoiseMix = juce::jlimit(0.0f, 1.0f, noiseMix);
    mNoiseColor = juce::jlimit(100.0f, 10000.0f, noiseColorHz);
    mLofi = juce::jlimit(0.0f, 1.0f, lofi);
    mPortaTime = juce::jlimit(0.0f, 2.0f, portaTimeSec);

    mAttack = juce::jmax(0.001f, attackSec);
    mDecay = juce::jmax(0.001f, decaySec);
    mSustain = juce::jlimit(0.0f, 1.0f, sustainVal);
    mRelease = juce::jmax(0.001f, releaseSec);
}

void ExcitationEngine::triggerVoice(int noteNumber, float velocity) noexcept
{
    const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(noteNumber) - 69.0f) / 12.0f);
    mTriggerCounter++;

    // 1. 同一ノート番号が既に発音中ならリトリガー
    for (auto& v : mVoices)
    {
        if (v.active && v.noteNumber == noteNumber)
        {
            v.velocity = velocity;
            v.targetFreq = freq;
            v.triggerTime = mTriggerCounter;
            v.stage = Voice::Attack;
            v.quantNoteHeld = -1;   // 新しい音は前の音の吸着状態を引き継がない
            mLastTriggeredFreq = freq;
            return;
        }
    }

    // 2. 空きボイスを探す
    for (auto& v : mVoices)
    {
        if (!v.active || v.stage == Voice::Idle)
        {
            v.active = true;
            v.noteNumber = noteNumber;
            v.velocity = velocity;
            v.targetFreq = freq;
            v.triggerTime = mTriggerCounter;
            v.stage = Voice::Attack;
            v.quantNoteHeld = -1;   // 新しい音は前の音の吸着状態を引き継がない

            // ポルタメント処理
            if (mPortaTime > 0.001f)
                v.currentFreq = mLastTriggeredFreq;
            else
                v.currentFreq = freq;

            mLastTriggeredFreq = freq;
            return;
        }
    }

    // 3. 空きがなければ最も古いボイスをスチール (古い順スチール)
    int oldestIdx = 0;
    uint32_t oldestTime = 0xFFFFFFFF;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (mVoices[(size_t)i].triggerTime < oldestTime)
        {
            oldestTime = mVoices[(size_t)i].triggerTime;
            oldestIdx = i;
        }
    }

    auto& v = mVoices[(size_t)oldestIdx];
    v.active = true;
    v.noteNumber = noteNumber;
    v.velocity = velocity;
    v.targetFreq = freq;
    v.triggerTime = mTriggerCounter;
    v.stage = Voice::Attack;
    v.envValue = 0.0f; // スチールなのでリセット
    v.quantNoteHeld = -1;

    if (mPortaTime > 0.001f)
        v.currentFreq = mLastTriggeredFreq;
    else
        v.currentFreq = freq;

    mLastTriggeredFreq = freq;
}

float ExcitationEngine::applyPolyBlep(float phase, float phaseInc) const noexcept
{
    // 不連続点の前後 dt サンプル範囲での補正
    if (phase < phaseInc)
    {
        float t = phase / phaseInc;
        return t + t - t * t - 1.0f;
    }
    else if (phase > 1.0f - phaseInc)
    {
        float t = (phase - 1.0f) / phaseInc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// Morph位相ワープ (BassSynth applyPhaseWarp 準拠、境界フェード付き)。
//  Bend → Sync の順に直列適用する (両方ONなら曲げた位相をさらにシンクで繰り返す)。
float ExcitationEngine::applyMorphPhase(float phase, float phaseInc, float& sMul) const noexcept
{
    if (!mBendOn && !mSyncOn)
        return phase;

    const float orig = phase;
    float warped = phase;

    if (mBendOn) // Bend +/-
    {
        if (warped < mBendSym)
            warped = mBendSym * std::pow(warped / mBendSym, mBendB);
        else
            warped = mBendSym + (1.0f - mBendSym)
                   * (1.0f - std::pow((1.0f - warped) / (1.0f - mBendSym), mBendB));
    }

    if (mSyncOn) // Sync: 位相を1〜8倍で繰り返し、折返し境界で振幅フェード (クリック防止)
    {
        float res = (warped + mSyncShiftHalf) * mSyncSt;
        res -= std::floor(res);
        if (res > 0.985f)      sMul *= (1.0f - res) / 0.015f;
        else if (res < 0.015f) sMul *= res / 0.015f;
        warped = res - mSyncShiftHalf;
        if (warped >= 1.0f) warped -= 1.0f;
        else if (warped < 0.0f) warped += 1.0f;
    }

    // 周期端の連続性フェード (BassSynth fadeWidth = freq*1e-5 相当。freq=phaseInc*16000)
    const float fadeWidth = juce::jlimit(0.001f, 0.03f, phaseInc * 0.16f);
    if (orig < fadeWidth)
    {
        const float mix = orig / fadeWidth;
        return warped * mix + orig * (1.0f - mix);
    }
    if (orig > 1.0f - fadeWidth)
    {
        const float mix = (1.0f - orig) / fadeWidth;
        return warped * mix + orig * (1.0f - mix);
    }
    return warped;
}

float ExcitationEngine::processVoiceSample(Voice& v, int channel, float phaseInc) noexcept
{
    float phase = (channel == 0) ? v.phaseL : v.phaseR;

    // Morph (Bend/Sync) の位相ワープ
    float sMul = 1.0f;
    phase = applyMorphPhase(phase, phaseInc, sMul);

    float out = 0.0f;
    if (mWaveform == 0) // Saw
    {
        out = 2.0f * phase - 1.0f;
        out -= applyPolyBlep(phase, phaseInc);
    }
    else if (mWaveform == 1) // Pulse
    {
        out = (phase < mPulseWidth) ? 1.0f : -1.0f;
        out += applyPolyBlep(phase, phaseInc);
        out -= applyPolyBlep(std::fmod(phase + (1.0f - mPulseWidth), 1.0f), phaseInc);
    }
    else // Wavetable
    {
        out = mWavetable.sample(phase, mWtPos, phaseInc);
    }

    return out * sMul;
}

void ExcitationEngine::applyLoFi(float& l, float& r, float pitchHz) noexcept
{
    if (mLofi <= 0.0f) return;

    // 1. ビット数量子化
    // lofi=0: 24bit (実質変化なし), lofi=1: 1bit
    const float bits = 24.0f - 23.0f * mLofi;
    const float levels = std::pow(2.0f, bits);
    l = std::round(l * levels) / levels;
    r = std::round(r * levels) / levels;

    // 2. ピッチ同期サンプル&ホールド
    //  旧実装(固定レート、最低100Hz)はホールドレートが基音を下回ると
    //  S&H周期そのものが低い音程として聞こえ「低音強調」になっていた。
    //  ホールドレートを常に基音の整数倍 N×f0 に同期させることで、
    //  基音(=元音の高さ)は保たれたまま倍音だけが粗くなるLofi味付けにする。
    //  lofi=0: N=64 (ほぼ変化なし) → lofi=1: N=4 (1周期4ステップの荒い波形)
    const float f0 = juce::jlimit(30.0f, 2000.0f, pitchHz);
    const float stepsPerCycle = 64.0f * std::pow(4.0f / 64.0f, mLofi); // 64 → 4
    const float holdRate = juce::jmin((float)kInternalSampleRate, stepsPerCycle * f0);
    const float sampleHoldPeriod = (float)kInternalSampleRate / holdRate;

    if (sampleHoldPeriod <= 1.001f)
        return; // ホールド実質なし

    mLofiRateCounter += 1.0f;
    if (mLofiRateCounter >= sampleHoldPeriod)
    {
        mLofiRateCounter = std::fmod(mLofiRateCounter, sampleHoldPeriod);
        mLofiLastValL = l;
        mLofiLastValR = r;
    }
    else
    {
        l = mLofiLastValL;
        r = mLofiLastValR;
    }
}

void ExcitationEngine::processSample(float& outL, float& outR, float externalPitchHz, bool isMidiMode) noexcept
{
    float voiceSumL = 0.0f;
    float voiceSumR = 0.0f;
    int activeVoiceCount = 0;

    // Detune/NoiseMix の一次平滑 (τ≈5ms@16k)。制御ブロック毎(2ms)の
    // 階段状変化によるクリック/ジッパーノイズを防ぐ。
    constexpr float kSmCoef = 0.0124f;   // 1-exp(-1/(0.005*16000))
    mDetuneSm += kSmCoef * (mDetuneCents - mDetuneSm);
    mNoiseSm  += kSmCoef * (mNoiseMix  - mNoiseSm);

    // ポルタメント係数
    const float portaCoeff = (mPortaTime > 0.001f)
        ? static_cast<float>(1.0 - std::exp(-1.0 / (mPortaTime * kInternalSampleRate)))
        : 1.0f;

    // ADSR用 1サンプルあたりのインクリメント量
    const float attStep = 1.0f / (mAttack * (float)kInternalSampleRate);
    const float decStep = 1.0f / (mDecay * (float)kInternalSampleRate);
    const float relStep = 1.0f / (mRelease * (float)kInternalSampleRate);

    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& v = mVoices[(size_t)i];

        // 非MIDIモード（Autoモード）の場合、ボイス0〜2をアクティブにしてユニゾンデチューン効果を得る
        if (!isMidiMode)
        {
            if (i > 2)
            {
                v.active = false;
                v.stage = Voice::Idle;
                v.envValue = 0.0f;
                v.velocity = 0.0f;
                continue;
            }
            v.active = true;
            v.stage = Voice::Sustain;
            v.envValue = (i == 0) ? 1.0f : 0.8f;
            v.velocity = 1.0f;
            v.targetFreq = externalPitchHz;
        }

        if (!v.active || v.stage == Voice::Idle) continue;

        activeVoiceCount++;

        // 1. ピッチポルタメント適用
        v.currentFreq += portaCoeff * (v.targetFreq - v.currentFreq);

        // ※旧実装の「lofi>0.5で半音ピッチ量子化」は削除した。
        //   ケロケロ効果は pitchQuantize ノブに一本化(挙動を予測可能に)。
        float pitch = v.currentFreq;

        // M.PITCH / PITCH Q の適用 (MIDIモードのみ)。
        //  Autoモードでは PluginProcessor 側の activePitch に適用済みなので、
        //  ここで再度掛けると二重適用になる。
        //  ボイス毎にヒステリシス状態を持たせ、和音でも各声部が独立に吸着する。
        if (isMidiMode)
        {
            pitch = ScaleSnap::transposeAndSnap(pitch, mMasterPitchSt, mQuantAmt,
                                                mQuantKey, mQuantScale, v.quantNoteHeld);
            if (mQuantAmt <= 0.001f)
                v.quantNoteHeld = -1;
        }

        // 2. デチューン計算 (Detune Mode別の分散アルゴリズム)
        const float detuneCents = mDetuneSm;
        const float sign = (i % 2 == 0) ? -1.0f : 1.0f;
        const float classicScale = 0.15f + 0.1f * static_cast<float>(i / 2);
        float p; // 正規化デチューン・ポジション (符号込み)
        switch (mDetuneMode)
        {
        case 1: // Linear: ボイスを均等間隔で拡散
            p = sign * (0.25f + 0.75f * (float)i / (float)(kMaxVoices - 1));
            break;
        case 2: // Exp: 中心密・外側疎 (スーパーソウ的。ボイス0はセンター純音)
        {
            const float t = (float)i / (float)(kMaxVoices - 1);
            p = sign * t * t * 1.2f;
            break;
        }
        case 3: // Drift: ランダムウォークで比率が揺らぐ (アナログVCO風)
            mDriftVal[(size_t)i] = mDriftVal[(size_t)i] * 0.9999f
                                 + (mRng.nextFloat() - 0.5f) * 0.002f;
            p = sign * classicScale * (1.0f + 2.5f * mDriftVal[(size_t)i]);
            break;
        case 4: // Chorus: ボイス毎の低速LFOでポジションがうねる
            mChorusPhase[(size_t)i] += 6.2831853f * (0.13f * (1.0f + 0.37f * (float)i)) / (float)kInternalSampleRate;
            if (mChorusPhase[(size_t)i] > 6.2831853f) mChorusPhase[(size_t)i] -= 6.2831853f;
            p = sign * classicScale + 0.35f * std::sin(mChorusPhase[(size_t)i]);
            break;
        default: // Classic: 従来 (交互符号 × 固定比率)
            p = sign * classicScale;
            break;
        }
        const float detuneOffset = detuneCents * p;

        float freqL = pitch * std::pow(2.0f, detuneOffset / 1200.0f);
        float freqR = pitch * std::pow(2.0f, -detuneOffset / 1200.0f);

        // 範囲制限
        freqL = juce::jlimit(20.0f, 7800.0f, freqL);
        freqR = juce::jlimit(20.0f, 7800.0f, freqR);

        float phaseIncL = freqL / (float)kInternalSampleRate;
        float phaseIncR = freqR / (float)kInternalSampleRate;

        // 3. ボイス別ADSRエンベロープ更新 (MIDIモード時のみ)
        if (isMidiMode)
        {
            switch (v.stage)
            {
            case Voice::Attack:
                v.envValue += attStep;
                if (v.envValue >= 1.0f)
                {
                    v.envValue = 1.0f;
                    v.stage = Voice::Decay;
                }
                break;

            case Voice::Decay:
                v.envValue -= decStep * (1.0f - mSustain);
                if (v.envValue <= mSustain)
                {
                    v.envValue = mSustain;
                    v.stage = Voice::Sustain;
                }
                break;

            case Voice::Sustain:
                v.envValue = mSustain;
                break;

            case Voice::Release:
                v.envValue -= relStep * v.releaseStartVal;
                if (v.envValue <= 0.0f)
                {
                    v.envValue = 0.0f;
                    v.stage = Voice::Idle;
                    v.active = false;
                }
                break;

            default:
                v.envValue = 0.0f;
                break;
            }
        }
        else
        {
            // Autoモード時はADSR設定を無視してキャリア音量を最大固定
            v.envValue = (i == 0) ? 1.0f : 0.8f;
        }

        // 4. 波形生成
        float voiceL = processVoiceSample(v, 0, phaseIncL);
        float voiceR = processVoiceSample(v, 1, phaseIncR);

        float gain = v.envValue * v.velocity;
        voiceSumL += voiceL * gain;
        voiceSumR += voiceR * gain;

        // 5. 位相インクリメント
        v.phaseL = std::fmod(v.phaseL + phaseIncL, 1.0f);
        v.phaseR = std::fmod(v.phaseR + phaseIncR, 1.0f);
    }

    // 5b. Morph Vocode: A-I-U-E-O フォルマントフィルタ (BassSynth mode9 の時間領域等価)。
    //  BassSynthはウェーブテーブル倍音(bin=倍音番号)への振幅EQだったため、
    //  中心周波数 = 倍音番号 × 基音 で追従する3基の共振BPFとして実装する。
    //  mag' = mag*(1-|amt|) + mag*env*4*|amt| に対応する dry/wet 構成。
    if (mVocAmt > 0.001f && activeVoiceCount > 0)
    {
        const float f0 = juce::jlimit(30.0f, 2000.0f,
                                      isMidiMode ? mLastTriggeredFreq : externalPitchHz);
        float wetL = 0.0f, wetR = 0.0f;
        for (int j = 0; j < 3; ++j)
        {
            const float fc = juce::jlimit(60.0f, 7200.0f, mVocHarm[j] * f0);
            // BassSynth: width_bins = 0.15*target + 4*scale → Hz換算 0.15*fc + 4*f0
            const float bwHz = 0.15f * fc + 4.0f * f0;
            const float Q = juce::jlimit(1.0f, 30.0f, fc / juce::jmax(1.0f, bwHz));
            wetL += mVocFilterL[j].processBPF_Q(voiceSumL, fc, (float)kInternalSampleRate, Q);
            wetR += mVocFilterR[j].processBPF_Q(voiceSumR, fc, (float)kInternalSampleRate, Q);
        }
        voiceSumL = voiceSumL * (1.0f - mVocAmt) + wetL * 4.0f * mVocAmt;
        voiceSumR = voiceSumR * (1.0f - mVocAmt) + wetR * 4.0f * mVocAmt;
    }

    // 6. ノイズブレンド (有声/無声比率、有声音へのホワイトノイズ混入)
    // -1.0 〜 +1.0 のホワイトノイズ
    float noiseSampleL = mRng.nextFloat() * 2.0f - 1.0f;
    float noiseSampleR = mRng.nextFloat() * 2.0f - 1.0f;

    // バンドパスフィルタでノイズの音程（色）を変更
    noiseSampleL = mNoiseFilterL.processBPF(noiseSampleL, mNoiseColor, (float)kInternalSampleRate);
    noiseSampleR = mNoiseFilterR.processBPF(noiseSampleR, mNoiseColor, (float)kInternalSampleRate);

    // ポリフォニック合算音がクリップしないようにスケーリング
    float voiceScale = (activeVoiceCount > 1) ? (1.0f / std::sqrt((float)activeVoiceCount)) : 1.0f;
    voiceSumL *= voiceScale;
    voiceSumR *= voiceScale;

    outL = (1.0f - mNoiseSm) * voiceSumL + mNoiseSm * noiseSampleL;
    outR = (1.0f - mNoiseSm) * voiceSumR + mNoiseSm * noiseSampleR;

    // MIDIモード時、鍵盤を弾いていない（アクティブなボイスが0）ときはキャリアを完全ミュート（常時ノイズ出力を防止）
    if (isMidiMode && activeVoiceCount == 0)
    {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    // 7. LoFiポストエフェクト適用 (ピッチ同期S&H用に現在の基音を渡す)
    //    Autoモード: トラッキング済みピッチ / MIDIモード: 最後に発音したノート周波数
    const float lofiRefPitch = isMidiMode ? mLastTriggeredFreq : externalPitchHz;
    applyLoFi(outL, outR, lofiRefPitch);
}
