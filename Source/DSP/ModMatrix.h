// ==========================================
// File: ModMatrix.h
// モジュレーションマトリクス（ブロックレート処理 / Granular 準拠）
//
//  Sources : LFO×3 (テンポ同期/フリー, Sine/Tri/Saw/Sqr/S&H/Chaos)
//            ENV×2 (ループ可能ADSR), Velocity, Note, ModWheel, Random(ノート毎S&H)
//  Slots   : 6 ( Source × Amount(-1..+1) × Uni/Bipolar → Destination )
//  Dests   : VOCODER / EXCITATION タブの全ノブ (26個)
//            ※BANDS(bandCount)のみ除外。変調するとフィルタバンク再構築が走り
//              切替の度にクリック音が出るため。
//
//  【重要】宛先の定義は destParamId / destScale / destKind の3点セットで完結させ、
//  適用は applyMod() に一本化している。DSP側とGUIのアーク表示が同じ関数を通るので、
//  スケールの食い違い(表示と実際の効きがズレる)が構造的に起きない。
//  宛先を追加するときは Dst / getDestNames / destParamId / destScale / destKind の
//  5箇所を必ず揃えること (順序も一致させる)。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <cmath>

class ModMatrix
{
public:
    static constexpr int kNumLfos = 3;
    static constexpr int kNumEnvs = 2;
    static constexpr int kNumSlots = 6;

    enum Src
    {
        SrcNone = 0,
        SrcLfo1, SrcLfo2, SrcLfo3,
        SrcEnv1, SrcEnv2,
        SrcVelocity, SrcNote, SrcModWheel, SrcRandom,
        NumSrcs
    };

    enum Dst
    {
        DstNone = 0,
        // ---- VOCODER タブ ----
        DstCharacter, DstTracking, DstPitchQuantize,
        DstFormantShift, DstFormantStretch,
        DstLofi, DstBasePitch, DstNoiseColor, DstNoise, DstResonance,
        DstAttack, DstDecay, DstSustain, DstRelease,
        DstMix, DstOutLevel,
        // ---- EXCITATION タブ ----
        DstWtPos, DstPulseWidth, DstPorta, DstDetune,
        DstBendAmt, DstBendShift, DstSyncAmt, DstSyncShift,
        DstVocAmt, DstVocShift,
        // ※新しい宛先は必ず末尾に足すこと。途中に挿入すると
        //   AudioParameterChoice がインデックス保存のため既存セッションの
        //   スロット設定が別の宛先にズレる。
        DstMasterPitch,
        DstStereoWidth,   // Filterbank の帯域交互パンニング幅 (0=モノ 〜 1=最大)
        // ---- FX タブ ----
        DstFxDrive,
        DstGateRate, DstGateDepth, DstGateVowel, DstGateSmooth, DstGateShape,
        DstResDecay, DstResShimmer, DstResDamp, DstResInharm,
        DstChorusRate, DstChorusDepth, DstChorusWidth, DstChorusMix,
        DstDelayTime, DstDelayFb, DstDelayTone, DstDelayMix,
        DstReverbSize, DstReverbDecay, DstReverbPre, DstReverbDamp, DstReverbMix,
        DstAir,           // 高域エアバンド量 (2026-08-02 追加。必ず末尾)
        DstGateDecay,     // Gate Decay (Colors移植)
        DstResShift,      // Resonator Shift (Colors移植)
        DstResSpread,     // Resonator Spread (Colors移植)
        DstResOutGain,    // Resonator Out Gain (Colors移植)
        NumDsts
    };

    // 変調の掛かり方。対数的なパラメータ(周波数/時間)は加算だと低域側で使い物に
    // ならないので、オクターブ倍率で掛ける。
    enum Kind { KindLinear = 0, KindExpOct };

    static juce::StringArray getSourceNames()
    {
        return { "None", "LFO 1", "LFO 2", "LFO 3", "ENV 1", "ENV 2",
                 "Velocity", "Note", "Mod Wheel", "Random" };
    }
    static juce::StringArray getDestNames()
    {
        return { "None",
                 // VOCODER
                 "Character", "Tracking", "Pitch Quantize",
                 "Formant Shift", "Formant Stretch",
                 "LoFi", "Base Pitch", "Noise Color", "Noise", "Resonance",
                 "Attack", "Decay", "Sustain", "Release",
                 "Mix", "Out Level",
                 // EXCITATION
                 "WT Position", "Pulse Width", "Porta", "Detune",
                 "Bend", "Bend Sym", "Sync", "Sync Ph",
                 "Vocode", "Vowel",
                 "Master Pitch", "Width",
                 // FX
                 "Drive",
                 "Gate Rate", "Gate Depth", "Gate Vowel", "Gate Smooth", "Gate Shape",
                 "Resonator Decay", "Resonator Shimmer", "Resonator Damp", "Resonator Inharm",
                 "Chorus Rate", "Chorus Depth", "Chorus Width", "Chorus Mix",
                 "Delay Time", "Delay Feedback", "Delay Tone", "Delay Mix",
                 "Reverb Size", "Reverb Decay", "Reverb PreDelay", "Reverb Damp", "Reverb Mix",
                 "Air",
                 "Gate Decay", "Resonator Shift", "Resonator Spread", "Resonator Out Gain" };
    }
    static juce::StringArray getWaveNames()
    {
        return { "Sine", "Triangle", "Saw", "Square", "S&H", "Chaos" };
    }
    static juce::StringArray getSyncRateNames()
    {
        return { "1/1", "1/2", "1/2T", "1/4", "1/4.", "1/4T",
                 "1/8", "1/8.", "1/8T", "1/16", "1/16.", "1/16T", "1/32" };
    }

    // 宛先に対応するAPVTSのパラメータID。GUIのノブ紐付けにも使う。
    static const char* destParamId(int dst) noexcept
    {
        switch (dst)
        {
        case DstCharacter:      return "character";
        case DstTracking:       return "tracking";
        case DstPitchQuantize:  return "pitchQuantize";
        case DstFormantShift:   return "formantShift";
        case DstFormantStretch: return "formantStretch";
        case DstLofi:           return "lofi";
        case DstBasePitch:      return "basePitch";
        case DstNoiseColor:     return "noiseColor";
        case DstNoise:          return "noise";
        case DstResonance:      return "resonance";
        case DstAttack:         return "attack";
        case DstDecay:          return "decay";
        case DstSustain:        return "sustain";
        case DstRelease:        return "release";
        case DstAir:            return "air";
        case DstMix:            return "mix";
        case DstOutLevel:       return "outputLevel";
        case DstWtPos:          return "wavetablePosition";
        case DstPulseWidth:     return "pulseWidth";
        case DstPorta:          return "porta";
        case DstDetune:         return "detune";
        case DstBendAmt:        return "bendAmt";
        case DstBendShift:      return "bendShift";
        case DstSyncAmt:        return "syncAmt";
        case DstSyncShift:      return "syncShift";
        case DstVocAmt:         return "vocAmt";
        case DstVocShift:       return "vocShift";
        case DstMasterPitch:    return "masterPitch";
        case DstStereoWidth:    return "stereoWidth";
        case DstFxDrive:        return "drvDrive";
        case DstGateRate:       return "gateRate";
        case DstGateDepth:      return "gateDepth";
        case DstGateVowel:      return "gateVowel";
        case DstGateSmooth:     return "gateSmooth";
        case DstGateShape:      return "gateShape";
        case DstResDecay:       return "resDecay";
        case DstResShimmer:     return "resShimmer";
        case DstResDamp:        return "resDamp";
        case DstResInharm:      return "resInharm";
        case DstChorusRate:     return "choRate";
        case DstChorusDepth:    return "choDepth";
        case DstChorusWidth:    return "choWidth";
        case DstChorusMix:      return "choMix";
        case DstDelayTime:      return "dlyTime";
        case DstDelayFb:        return "dlyFeedback";
        case DstDelayTone:      return "dlyTone";
        case DstDelayMix:       return "dlyMix";
        case DstReverbSize:     return "revSize";
        case DstReverbDecay:    return "revDecay";
        case DstReverbPre:      return "revPredelay";
        case DstReverbDamp:     return "revDamp";
        case DstReverbMix:      return "revMix";
        case DstGateDecay:      return "gateDecay";
        case DstResShift:       return "resShift";
        case DstResSpread:      return "resSpread";
        case DstResOutGain:     return "resOutGain";
        default:                return "";
        }
    }

    // 変調1.0あたりの実パラメータ単位スケール。
    //  KindLinear : 実単位での加算量  KindExpOct : 倍率のオクターブ数
    static float destScale(int dst) noexcept
    {
        switch (dst)
        {
        // --- VOCODER ---
        case DstCharacter:      return 1.0f;    // 0..1
        case DstTracking:       return 100.0f;  // 0..100 %
        case DstPitchQuantize:  return 100.0f;  // 0..100 %
        case DstFormantShift:   return 24.0f;   // ±24 semitones
        case DstFormantStretch: return 0.75f;   // 0.5..2.0 の半レンジ
        case DstLofi:           return 1.0f;    // 0..1
        case DstBasePitch:      return 2.0f;    // ±2 oct  (50..500Hz)
        case DstNoiseColor:     return 3.0f;    // ±3 oct  (100..10kHz)
        case DstNoise:          return 100.0f;  // ±100 %
        case DstResonance:      return 1.5f;    // ±1.5 oct (0.3..3.0)
        case DstAttack:         return 3.0f;    // ±3 oct  (0.001..5s)
        case DstDecay:          return 3.0f;
        case DstSustain:        return 1.0f;    // 0..1
        case DstRelease:        return 3.0f;
        case DstAir:            return 100.0f;  // ±100 %
        case DstMix:            return 100.0f;  // ±100 %
        case DstOutLevel:       return 24.0f;   // ±24 dB (dBは既に対数なので線形加算)
        // --- EXCITATION ---
        case DstWtPos:          return 1.0f;    // 0..1
        case DstPulseWidth:     return 45.0f;   // ±45 %
        case DstPorta:          return 1.0f;    // ±1 s (0..2)
        case DstDetune:         return 600.0f;  // ±600 cents
        case DstBendAmt:        return 1.0f;    // -1..+1
        case DstBendShift:      return 1.0f;
        case DstSyncAmt:        return 1.0f;    // 0..1
        case DstSyncShift:      return 1.0f;    // -1..+1
        case DstVocAmt:         return 1.0f;    // 0..1
        case DstVocShift:       return 1.0f;    // -1..+1
        // Master Pitch: ±24半音。PitchQ=100%ならスケールにスナップされるので、
        // LFOを当てると該当Key/Scale上を音が移動する。
        case DstMasterPitch:    return 24.0f;
        case DstStereoWidth:    return 1.0f;    // 0..1
        // --- FX ---
        case DstFxDrive:        return 1.0f;
        case DstGateRate:       return 3.0f;
        case DstGateDepth:      return 100.0f;
        case DstGateVowel:      return 100.0f;
        case DstGateSmooth:     return 1.0f;
        case DstGateShape:      return 1.0f;
        case DstGateDecay:      return 100.0f;
        case DstResShift:       return 24.0f;
        case DstResDecay:       return 3.0f;
        case DstResShimmer:     return 100.0f;
        case DstResDamp:        return 100.0f;
        case DstResInharm:      return 100.0f;
        case DstResSpread:      return 100.0f;
        case DstResOutGain:     return 12.0f;
        case DstChorusRate:     return 3.0f;
        case DstChorusDepth:    return 12.0f;
        case DstChorusWidth:    return 1.0f;
        case DstChorusMix:      return 1.0f;
        case DstDelayTime:      return 3.0f;
        case DstDelayFb:        return 1.0f;
        case DstDelayTone:      return 1.0f;
        case DstDelayMix:       return 1.0f;
        case DstReverbSize:     return 1.0f;
        case DstReverbDecay:    return 3.0f;
        case DstReverbPre:      return 3.0f;
        case DstReverbDamp:     return 1.0f;
        case DstReverbMix:      return 1.0f;
        default:                return 0.0f;
        }
    }

    static int destKind(int dst) noexcept
    {
        switch (dst)
        {
        case DstBasePitch:
        case DstNoiseColor:
        case DstResonance:
        case DstAttack:
        case DstDecay:
        case DstRelease:
        case DstResDecay:
        case DstChorusRate:
        case DstDelayTime:
        case DstReverbDecay:
        case DstReverbPre:
            return KindExpOct;
        default:
            return KindLinear;
        }
    }

    // 変調適用の単一の真実源。DSPもGUIのアーク表示もこれを通す。
    //  base    : ノブの生値 (実パラメータ単位)
    //  modVal  : 合成済み変調量 (概ね -1..+1)
    static float applyMod(int dst, float base, float modVal) noexcept
    {
        const float s = destScale(dst);
        if (destKind(dst) == KindExpOct)
            return base * std::pow(2.0f, modVal * s);
        return base + modVal * s;
    }

    struct Params
    {
        struct Lfo { float rateHz = 1.0f; bool sync = false; int rateSync = 6; int wave = 0; };
        struct Env { float attack = 0.05f; float decay = 0.5f; float sustain = 1.0f; float release = 0.3f; bool loop = false; };
        struct Slot { int src = 0; int dst = 0; float amt = 0.0f; bool uni = false; };

        std::array<Lfo, kNumLfos> lfo;
        std::array<Env, kNumEnvs> env;
        std::array<Slot, kNumSlots> slot;
        double bpm = 120.0;
    };

    void prepare(double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        for (auto& l : lfoState) l = {};
        for (auto& e : envState) e = {};
        velocity = 0.8f;
        noteNorm = 0.5f;
        modWheel = 0.0f;
        randomSH = 0.5f;
        heldNotes = 0;
        gate = false;
        clearArr(destAccum);
        clearArr(rangeMin);
        clearArr(rangeMax);
    }

    // 入力MIDIからソース値を更新
    void handleMidi(const juce::MidiBuffer& midi)
    {
        for (const auto& m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn())
            {
                velocity = msg.getFloatVelocity();
                noteNorm = (float)msg.getNoteNumber() / 127.0f;
                randomSH = rng.nextFloat();
                ++heldNotes;
                gate = true;
                for (auto& e : envState) e.stage = EnvState::Attack; // リトリガー(現在値から)
            }
            else if (msg.isNoteOff())
            {
                if (--heldNotes <= 0) { heldNotes = 0; gate = false; } // 全ノート離鍵→ゲートOFF
            }
            else if (msg.isControllerOfType(1))
            {
                modWheel = (float)msg.getControllerValue() / 127.0f;
            }
        }
    }

    // ブロック毎: ソース値を確定し、スロットを合成して destAccum に集計
    void processBlock(int numSamples, const Params& p)
    {
        static const double beatsTable[13] = {
            4.0, 2.0, 4.0 / 3.0, 1.0, 1.5, 2.0 / 3.0,
            0.5, 0.75, 1.0 / 3.0, 0.25, 0.375, 1.0 / 6.0, 0.125 };

        float src[NumSrcs] = {};

        // --- LFO ---
        for (int i = 0; i < kNumLfos; ++i)
        {
            const auto& lp = p.lfo[(size_t)i];
            auto& st = lfoState[(size_t)i];

            double freq = (double)lp.rateHz;
            if (lp.sync)
            {
                const double bpm = p.bpm > 1.0 ? p.bpm : 120.0;
                freq = bpm / (60.0 * beatsTable[juce::jlimit(0, 12, lp.rateSync)]);
            }

            src[SrcLfo1 + i] = lfoValue(st, lp.wave);

            const double inc = freq * (double)numSamples / sampleRate;
            st.phase += inc;
            st.phase2 += inc * 1.41421356; // Chaos用

            // 【重要】ラップ回数に上限を設ける。
            //  freq や inc が NaN/Inf になると `while (phase >= 1.0)` が永久に回り、
            //  オーディオスレッドが固まってDAWごとフリーズする。
            //  非有限になったら位相を捨てて再スタートする (Wavetable版 Lfo.h と同じ考え方)。
            if (!std::isfinite(st.phase) || !std::isfinite(st.phase2))
            {
                st.phase = 0.0;
                st.phase2 = 0.0;
            }
            else
            {
                int wraps = 0;
                while (st.phase >= 1.0 && wraps < 8)
                {
                    st.phase -= 1.0;
                    st.shValue = rng.nextFloat() * 2.0f - 1.0f; // S&H更新
                    ++wraps;
                }
                if (st.phase >= 1.0)                 // 1ブロックで8周以上 = 異常な高レート
                    st.phase -= std::floor(st.phase);
                if (st.phase2 >= 1.0)
                    st.phase2 -= std::floor(st.phase2);
            }
        }

        // --- ENV (ADSR / ブロックレート、Loop時はA-D循環) ---
        for (int i = 0; i < kNumEnvs; ++i)
        {
            const auto& ep = p.env[(size_t)i];
            auto& st = envState[(size_t)i];
            const float sus = juce::jlimit(0.0f, 1.0f, ep.sustain);

            src[SrcEnv1 + i] = st.value;

            const float blockSec = (float)((double)numSamples / sampleRate);

            if (!ep.loop && !gate
                && st.stage != EnvState::Idle && st.stage != EnvState::Release)
            {
                st.stage = EnvState::Release;
                st.releaseStart = juce::jmax(0.0001f, st.value);
            }

            // LOOP がONなら、ノートを弾いていなくても自走させる。
            //  旧実装は Idle のまま止まっていたため、Autoモード(MIDIを送らない使い方)では
            //  LOOP を点けても ENV がまったく動かなかった。
            //  LOOP は「4つ目のLFO」として使えるべきなので、Idle から自動で立ち上げる。
            if (ep.loop && st.stage == EnvState::Idle)
                st.stage = EnvState::Attack;

            switch (st.stage)
            {
            case EnvState::Attack:
                st.value += blockSec / juce::jmax(0.001f, ep.attack);
                if (st.value >= 1.0f) { st.value = 1.0f; st.stage = EnvState::Decay; }
                break;

            case EnvState::Decay:
                if (ep.loop)
                {
                    st.value -= blockSec / juce::jmax(0.001f, ep.decay);
                    if (st.value <= 0.0f) { st.value = 0.0f; st.stage = EnvState::Attack; }
                }
                else
                {
                    st.value -= (1.0f - sus) * blockSec / juce::jmax(0.001f, ep.decay);
                    if (st.value <= sus) { st.value = sus; st.stage = EnvState::Sustain; }
                }
                break;

            case EnvState::Sustain:
                st.value = sus;
                break;

            case EnvState::Release:
                st.value -= st.releaseStart * blockSec / juce::jmax(0.001f, ep.release);
                if (st.value <= 0.0f) { st.value = 0.0f; st.stage = EnvState::Idle; }
                break;

            default: // Idle
                st.value = 0.0f;
                break;
            }
        }

        // --- MIDI ---
        src[SrcVelocity] = velocity;
        src[SrcNote] = noteNorm;
        src[SrcModWheel] = modWheel;
        src[SrcRandom] = randomSH;

        // --- スロット合成 (Uni/Bipolar極性変換) ---
        clearArr(destAccum);
        for (const auto& s : p.slot)
        {
            if (s.src <= 0 || s.src >= NumSrcs || s.dst <= 0 || s.dst >= NumDsts) continue;
            if (std::abs(s.amt) < 0.0001f) continue;

            const bool srcBip = isBipolarSource(s.src);

            float v = src[s.src];
            if (s.uni) { if (srcBip) v = (v + 1.0f) * 0.5f; }        // 出力 0..1
            else       { if (!srcBip) v = v * 2.0f - 1.0f; }         // 出力 -1..+1
            addTo(destAccum, (size_t)s.dst, v * s.amt);
        }
        updateStaticRanges(p);
    }

    // GUIアーク用: 現在のスロット設定から各行き先の静的オフセット範囲を集計
    // DAW停止中であっても、パラメータ変更時に即座に反映できる。
    void updateStaticRanges(const Params& p) noexcept
    {
        clearArr(rangeMin);
        clearArr(rangeMax);
        for (const auto& s : p.slot)
        {
            if (s.src <= 0 || s.src >= NumSrcs || s.dst <= 0 || s.dst >= NumDsts) continue;
            if (std::abs(s.amt) < 0.0001f) continue;

            const float lo = s.uni ? 0.0f : -1.0f;
            const float hi = 1.0f;
            const float c1 = lo * s.amt, c2 = hi * s.amt;
            addTo(rangeMin, (size_t)s.dst, juce::jmin(c1, c2));
            addTo(rangeMax, (size_t)s.dst, juce::jmax(c1, c2));
        }
    }

    // DAW停止中用: GUIタイマー間隔(秒)で自走LFOを進め、ノブ上のライブ点(MOD値)とレンジをアニメーションさせる
    void processPreview(double deltaSec, const Params& p) noexcept
    {
        const int samples = juce::jmax(1, (int)(deltaSec * sampleRate));
        processBlock(samples, p);
    }

    // 合成済み変調値 (概ね -1..+1)。
    //  複数スロットが同じ宛先を指すと単純加算されるため、上限を設けておく。
    //  特に KindExpOct の宛先は pow(2, modVal*scale) なので、非有限値が1つ紛れ込むと
    //  そのまま NaN が全DSPへ伝播する。ここで水際を作る。
    float get(int dst) const noexcept
    {
        const float v = destAccum[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)].load(std::memory_order_relaxed);
        return std::isfinite(v) ? juce::jlimit(-4.0f, 4.0f, v) : 0.0f;
    }

    // GUIアーク用: 行き先が取りうる最小/最大オフセット (mod単位)
    float getRangeMin(int dst) const noexcept { return rangeMin[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)].load(std::memory_order_relaxed); }
    float getRangeMax(int dst) const noexcept { return rangeMax[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)].load(std::memory_order_relaxed); }

    // ソースが本来バイポーラ(±1)か: LFOのみ
    static bool isBipolarSource(int s) noexcept { return s >= SrcLfo1 && s <= SrcLfo3; }

private:
    struct LfoState
    {
        double phase = 0.0;
        double phase2 = 0.0;   // Chaos用の非整数比セカンド位相
        float shValue = 0.0f;
    };
    struct EnvState
    {
        enum Stage { Idle, Attack, Decay, Sustain, Release };
        int stage = Idle;
        float value = 0.0f;
        float releaseStart = 1.0f;
    };

    float lfoValue(const LfoState& st, int wave) const noexcept
    {
        const float ph = (float)st.phase;
        switch (wave)
        {
        case 0: return std::sin(ph * juce::MathConstants<float>::twoPi);              // Sine
        case 1: return 1.0f - 4.0f * std::abs(ph - 0.5f);                             // Triangle
        case 2: return 2.0f * ph - 1.0f;                                              // Saw
        case 3: return ph < 0.5f ? 1.0f : -1.0f;                                      // Square
        case 4: return st.shValue;                                                    // S&H
        case 5: return (std::sin(ph * juce::MathConstants<float>::twoPi)              // Chaos
                      + std::sin((float)st.phase2 * juce::MathConstants<float>::twoPi)) * 0.5f;
        default: return 0.0f;
        }
    }

    double sampleRate = 44100.0;
    std::array<LfoState, kNumLfos> lfoState;
    std::array<EnvState, kNumEnvs> envState;

    float velocity = 0.8f;
    float noteNorm = 0.5f;
    float modWheel = 0.0f;
    float randomSH = 0.5f;
    int   heldNotes = 0;
    bool  gate = false;

    // 【2026-08-02 修正】これらはオーディオスレッドが書き、GUIスレッドが
    //  get() / getRangeMin() / getRangeMax() で読む。素の float だと形式上は
    //  データ競合(未定義動作)になるため atomic + relaxed にする。
    //  x86では生成コードは実質同じでコストは増えない。
    std::array<std::atomic<float>, NumDsts> destAccum {};
    std::array<std::atomic<float>, NumDsts> rangeMin {};
    std::array<std::atomic<float>, NumDsts> rangeMax {};

    // std::atomic は代入可能でないため fill/+= を明示ヘルパで置き換える
    static void clearArr(std::array<std::atomic<float>, NumDsts>& a) noexcept
    {
        for (auto& x : a) x.store(0.0f, std::memory_order_relaxed);
    }
    static void addTo(std::array<std::atomic<float>, NumDsts>& a, size_t i, float v) noexcept
    {
        a[i].store(a[i].load(std::memory_order_relaxed) + v, std::memory_order_relaxed);
    }
    juce::Random rng;
};
