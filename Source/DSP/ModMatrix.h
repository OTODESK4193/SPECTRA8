// ==========================================
// File: ModMatrix.h
// モジュレーションマトリクス（ブロックレート処理 / Granular 準拠）
//
//  Sources : LFO×4 (テンポ同期/フリー, Sine/Tri/Saw/Sqr/S&H/Chaos)
//            ENV×3 (ループ可能ADSR), Velocity, Note, ModWheel,
//            Random(ノート毎S&H), Macro×4
//  Slots   : 16 ( Source × Amount(-1..+1) × Uni/Bipolar → Destination )
//  Dests   : SPECTRA8 固有 (開発計画書 第2版 ③MOD MATRIXタブ準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class ModMatrix
{
public:
    static constexpr int kNumLfos = 4;
    static constexpr int kNumEnvs = 3;
    static constexpr int kNumMacros = 4;
    static constexpr int kNumSlots = 16;

    enum Src
    {
        SrcNone = 0,
        SrcLfo1, SrcLfo2, SrcLfo3, SrcLfo4,
        SrcEnv1, SrcEnv2, SrcEnv3,
        SrcVelocity, SrcNote, SrcModWheel, SrcRandom,
        SrcMacro1, SrcMacro2, SrcMacro3, SrcMacro4,
        NumSrcs
    };

    enum Dst
    {
        DstNone = 0,
        DstCharacter,
        DstFormantShift, DstFormantStretch,
        DstWtPos, DstPulseWidth, DstDetune, DstNoise, DstLofi,
        DstMix, DstOutLevel,
        NumDsts
    };

    static juce::StringArray getSourceNames()
    {
        return { "None", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "ENV 1", "ENV 2", "ENV 3",
                 "Velocity", "Note", "Mod Wheel", "Random",
                 "Macro 1", "Macro 2", "Macro 3", "Macro 4" };
    }
    static juce::StringArray getDestNames()
    {
        return { "None", "Character",
                 "Formant Shift", "Formant Stretch",
                 "WT Position", "Pulse Width", "Detune", "Noise", "LoFi",
                 "Mix", "Out Level" };
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

    // 変調1.0あたりの実パラメータ単位スケール（DSPとGUIアーク表示で共有）
    static float destScale(int dst) noexcept
    {
        switch (dst)
        {
        case DstCharacter:      return 1.0f;    // 0..1
        case DstFormantShift:   return 24.0f;   // ±24 semitones
        case DstFormantStretch: return 0.75f;   // 0.5..2.0 の半レンジ
        case DstWtPos:          return 1.0f;    // 0..1
        case DstPulseWidth:     return 45.0f;   // ±45 %
        case DstDetune:         return 600.0f;  // ±600 cents (UI単位)
        case DstNoise:          return 100.0f;  // ±100 %
        case DstLofi:           return 1.0f;    // 0..1
        case DstMix:            return 100.0f;  // ±100 %
        case DstOutLevel:       return 24.0f;   // ±24 dB
        default:                return 0.0f;
        }
    }

    struct Params
    {
        struct Lfo { float rateHz = 1.0f; bool sync = false; int rateSync = 6; int wave = 0; };
        struct Env { float attack = 0.05f; float decay = 0.5f; float sustain = 1.0f; float release = 0.3f; bool loop = false; };
        struct Slot { int src = 0; int dst = 0; float amt = 0.0f; bool uni = false; };

        std::array<Lfo, kNumLfos> lfo;
        std::array<Env, kNumEnvs> env;
        std::array<float, kNumMacros> macro { 0.5f, 0.5f, 0.5f, 0.5f };
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
        destAccum.fill(0.0f);
        rangeMin.fill(0.0f);
        rangeMax.fill(0.0f);
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
            while (st.phase >= 1.0)
            {
                st.phase -= 1.0;
                st.shValue = rng.nextFloat() * 2.0f - 1.0f; // S&H更新
            }
            while (st.phase2 >= 1.0) st.phase2 -= 1.0;
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

        // --- MIDI / Macro ---
        src[SrcVelocity] = velocity;
        src[SrcNote] = noteNorm;
        src[SrcModWheel] = modWheel;
        src[SrcRandom] = randomSH;
        for (int i = 0; i < kNumMacros; ++i)
            src[SrcMacro1 + i] = p.macro[(size_t)i];

        // --- スロット合成 (Uni/Bipolar極性変換 + レンジ算出) ---
        destAccum.fill(0.0f);
        rangeMin.fill(0.0f);
        rangeMax.fill(0.0f);
        for (const auto& s : p.slot)
        {
            if (s.src <= 0 || s.src >= NumSrcs || s.dst <= 0 || s.dst >= NumDsts) continue;
            if (std::abs(s.amt) < 0.0001f) continue;

            const bool srcBip = isBipolarSource(s.src);

            float v = src[s.src];
            if (s.uni) { if (srcBip) v = (v + 1.0f) * 0.5f; }        // 出力 0..1
            else       { if (!srcBip) v = v * 2.0f - 1.0f; }         // 出力 -1..+1
            destAccum[(size_t)s.dst] += v * s.amt;

            // GUIアーク用: この行き先が取りうるオフセット範囲を集計
            const float lo = s.uni ? 0.0f : -1.0f;
            const float hi = 1.0f;
            const float c1 = lo * s.amt, c2 = hi * s.amt;
            rangeMin[(size_t)s.dst] += juce::jmin(c1, c2);
            rangeMax[(size_t)s.dst] += juce::jmax(c1, c2);
        }
    }

    // 合成済み変調値 (概ね -1..+1)
    float get(int dst) const noexcept
    {
        return destAccum[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)];
    }

    // GUIアーク用: 行き先が取りうる最小/最大オフセット (mod単位)
    float getRangeMin(int dst) const noexcept { return rangeMin[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)]; }
    float getRangeMax(int dst) const noexcept { return rangeMax[(size_t)juce::jlimit(0, (int)NumDsts - 1, dst)]; }

    // ソースが本来バイポーラ(±1)か: LFOのみ
    static bool isBipolarSource(int s) noexcept { return s >= SrcLfo1 && s <= SrcLfo4; }

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

    std::array<float, NumDsts> destAccum {};
    std::array<float, NumDsts> rangeMin {};
    std::array<float, NumDsts> rangeMax {};
    juce::Random rng;
};
