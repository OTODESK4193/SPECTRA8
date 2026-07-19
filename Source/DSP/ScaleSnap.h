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
    // pitchQScale パラメータの並びと一致させること
    //  0:Chromatic 1:Major 2:Minor(nat) 3:MajPenta 4:MinPenta
    inline uint16_t maskFor(int scale) noexcept
    {
        static constexpr uint16_t kMasks[5] = {
            0b111111111111,  // Chromatic
            0b101010110101,  // Major     {0,2,4,5,7,9,11}
            0b010110101101,  // Minor(nat){0,2,3,5,7,8,10}
            0b001010010101,  // MajPenta  {0,2,4,7,9}
            0b010010101001,  // MinPenta  {0,3,5,7,10}
        };
        return kMasks[juce::jlimit(0, 4, scale)];
    }

    inline bool isAllowed(int note, int key, uint16_t mask) noexcept
    {
        const int deg = ((note - key) % 12 + 12) % 12;
        return ((mask >> deg) & 1) != 0;
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
