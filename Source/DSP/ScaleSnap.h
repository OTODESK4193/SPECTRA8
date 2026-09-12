// ==========================================
// File: ScaleSnap.h
// Key/Scale へのピッチ吸着（PITCH Q / M.PITCH 共用）
//
//  Autoモード(PluginProcessor)とMIDIモード(ExcitationEngine)の両方から使う。
//  以前はスケールマスクの表がPluginProcessor.cpp内に直書きされており、
//  MIDIモードからは参照できずM.PITCH/PITCH Qが一切効かなかった。
//  ここに集約することで両モードの挙動が定義上一致する。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <cstdint>

namespace ScaleSnap
{
    // pitchQScale パラメータの並びと一致させること。
    //  ビット i = ルートから i 半音上の音を許可 (LSB = ルート)。
    //  【重要】先頭5つ(Chromatic/Major/Minor/MajPenta/MinPenta)の並びは変更しないこと。
    //  AudioParameterChoice はインデックス保存なので、既存セッションの設定がズレる。
    //  追加は必ず末尾へ。
    static constexpr int kNumScales = 20;

    inline uint16_t maskFor(int scale) noexcept
    {
        static constexpr uint16_t kMasks[kNumScales] = {
            0b111111111111,  //  0 Chromatic   全音
            0b101010110101,  //  1 Major       {0,2,4,5,7,9,11}
            0b010110101101,  //  2 Minor (nat) {0,2,3,5,7,8,10}
            0b001010010101,  //  3 Maj Penta   {0,2,4,7,9}
            0b010010101001,  //  4 Min Penta   {0,3,5,7,10}
            // --- 以下は追加分 (末尾に追記すること) ---
            0b100110101101,  //  5 Harm Minor  {0,2,3,5,7,8,11}
            0b101010101101,  //  6 Mel Minor   {0,2,3,5,7,9,11}
            0b011010101101,  //  7 Dorian      {0,2,3,5,7,9,10}
            0b010110101011,  //  8 Phrygian    {0,1,3,5,7,8,10}
            0b101011010101,  //  9 Lydian      {0,2,4,6,7,9,11}
            0b011010110101,  // 10 Mixolydian  {0,2,4,5,7,9,10}
            0b010101101011,  // 11 Locrian     {0,1,3,5,6,8,10}
            0b010011101001,  // 12 Blues       {0,3,5,6,7,10}
            0b010101010101,  // 13 Whole Tone  {0,2,4,6,8,10}
            0b011011011011,  // 14 Dim (H-W)   {0,1,3,4,6,7,9,10}
            0b100111001101,  // 15 Hungarian   {0,2,3,6,7,8,11}
            0b000110001101,  // 16 Hirajoshi   {0,2,3,7,8}
            0b010010100011,  // 17 Insen       {0,1,5,7,10}
            0b010001100011,  // 18 Iwato       {0,1,5,6,10}
            0b010110110011,  // 19 Hijaz       {0,1,4,5,7,8,10}  (=1459)
        };
        return kMasks[juce::jlimit(0, kNumScales - 1, scale)];
    }

    // pitchQScale コンボの表示名 (maskFor と同じ並び)
    inline juce::StringArray getScaleNames()
    {
        return { "Chromatic", "Major", "Minor", "Maj Penta", "Min Penta",
                 "Harm Minor", "Mel Minor", "Dorian", "Phrygian", "Lydian",
                 "Mixolydian", "Locrian", "Blues", "Whole Tone", "Dim (H-W)",
                 "Hungarian", "Hirajoshi", "Insen", "Iwato", "Hijaz" };
    }

    inline bool isAllowed(int note, int key, uint16_t mask) noexcept
    {
        const int deg = ((note - key) % 12 + 12) % 12;
        return ((mask >> deg) & 1) != 0;
    }

    // 整数MIDIノート番号を直接最近傍のスケール音へ吸着する (ヒステリシスなし、Resonator用)
    inline int snapMidiNote(int note, int key, int scale) noexcept
    {
        const uint16_t mask = maskFor(scale);
        const int k = juce::jlimit(0, 11, key);
        if (isAllowed(note, k, mask))
            return note;

        int nearest = note;
        int bestDist = 999;
        for (int d = -12; d <= 12; ++d)
        {
            const int cand = note + d;
            if (!isAllowed(cand, k, mask)) continue;
            const int dist = std::abs(d);
            if (dist < bestDist)
            {
                bestDist = dist;
                nearest = cand;
            }
        }
        return nearest;
    }

    // 周波数 → 連続MIDIノート値
    inline float hzToNote(float hz) noexcept
    {
        return 12.0f * std::log2(juce::jmax(1.0e-6f, hz) / 440.0f) + 69.0f;
    }
    inline float noteToHz(float note) noexcept
    {
        return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
    }

    // 連続ノート値 cont を許可音へ吸着する。
    //  heldNote : ヒステリシス用の状態 (呼び出し側が保持。初期値 -1)
    //  hyst     : 「現在保持中の音より hyst 半音以上近い音」が現れた時だけ切替える。
    //             LFOでM.PITCHを振った時などの境界チャタリング(ワブル)を防ぐ。
    // 戻り値: 吸着後のノート番号
    inline int snapNote(float cont, int key, int scale, int& heldNote, float hyst = 0.3f) noexcept
    {
        const uint16_t mask = maskFor(scale);
        const int k = juce::jlimit(0, 11, key);

        const int centre = (int)std::lround(cont);
        int nearest = centre;
        float bestDist = 1.0e9f;
        // ±1オクターブ探せば許可音は必ず見つかる
        for (int d = -12; d <= 12; ++d)
        {
            const int n = centre + d;
            if (!isAllowed(n, k, mask)) continue;
            const float dist = std::abs(cont - (float)n);
            if (dist < bestDist) { bestDist = dist; nearest = n; }
        }

        if (heldNote < 0 || !isAllowed(heldNote, k, mask)
            || bestDist + hyst < std::abs(cont - (float)heldNote))
            heldNote = nearest;

        return heldNote;
    }

    // 移調 + 吸着をまとめて適用する。
    //  hz            : 元のピッチ
    //  masterPitchSt : M.PITCH (半音)
    //  qAmt          : PITCH Q の効き 0..1 (0で吸着なし)
    //  戻り値: 処理後の周波数
    //
    //  ※移調を「吸着より前」に行うのが要点。こうすることで PITCH Q=100% のとき
    //    移調後の音が必ずスケール構成音へ落ち、M.PITCHをLFOで振ると
    //    そのKey/Scale上を音が渡り歩く。
    inline float transposeAndSnap(float hz, float masterPitchSt, float qAmt,
                                  int key, int scale, int& heldNote) noexcept
    {
        float out = hz;
        if (std::abs(masterPitchSt) > 0.0001f)
            out = juce::jlimit(20.0f, 8000.0f, out * std::pow(2.0f, masterPitchSt / 12.0f));

        if (qAmt > 0.001f && out > 20.0f && !std::isnan(out))
        {
            const int n = snapNote(hzToNote(out), key, scale, heldNote);
            const float qHz = noteToHz((float)n);
            if (!std::isnan(qHz) && qHz > 20.0f)
                out = out + (qHz - out) * juce::jlimit(0.0f, 1.0f, qAmt);
        }
        return out;
    }
}
