#pragma once
#include <cstdint>
#include <immintrin.h>

namespace DSP {

// DSPレンダリング用状態（アライメント32バイト保証）
struct alignas(32) PolyphonicVoiceSoA {
    static constexpr int kWaveTableSize = 2048;
    static constexpr int kWaveTableMask = 2047;
    static constexpr int kNumBands = 48;

    alignas(32) float phaseIncrL[8] = { 0.0f };
    alignas(32) float phaseIncrR[8] = { 0.0f };
    alignas(32) float phaseL[8] = { 0.0f };
    alignas(32) float phaseR[8] = { 0.0f };

    // 48バンド 4次ZDF SVF直列フィルタの各ボイスごとの状態変数 (s1, s2)
    // セクション 1 (S1)
    alignas(32) float filterS1_S1_L[kNumBands][8] = { { 0.0f } };
    alignas(32) float filterS1_S2_L[kNumBands][8] = { { 0.0f } };
    alignas(32) float filterS1_S1_R[kNumBands][8] = { { 0.0f } };
    alignas(32) float filterS1_S2_R[kNumBands][8] = { { 0.0f } };

    // セクション 2 (S2)
    alignas(32) float filterS2_S1_L[kNumBands][8] = { { 0.0f } };
    alignas(32) float filterS2_S2_L[kNumBands][8] = { { 0.0f } };
    alignas(32) float filterS2_S1_R[kNumBands][8] = { { 0.0f } };
    alignas(32) float filterS2_S2_R[kNumBands][8] = { { 0.0f } };

    // LPC合成用状態変数 (16次)
    static constexpr int kLpcOrder = 16;
    alignas(32) float lpcCoeffs[kLpcOrder][8] = { { 0.0f } };
    alignas(32) float lpcHistoryL[kLpcOrder][8] = { { 0.0f } };
    alignas(32) float lpcHistoryR[kLpcOrder][8] = { { 0.0f } };
    alignas(32) float lpcResidual[8] = { 0.0f };
};

// ボイス管理・変調用状態（アライメント32バイト保証）
struct alignas(32) VoiceSoABlock {
    alignas(32) float phaseL[8] = { 0.0f };
    alignas(32) float phaseR[8] = { 0.0f };
    alignas(32) float phaseIncL[8] = { 0.0f };
    alignas(32) float phaseIncR[8] = { 0.0f };
    alignas(32) float detuneCoeffL[8] = { 0.0f };
    alignas(32) float detuneCoeffR[8] = { 0.0f };
    alignas(32) float noteNo[8] = { 0.0f };
    alignas(32) float gate[8] = { 0.0f };         // 1.0f = ON, 0.0f = OFF
    alignas(32) float age[8] = { 0.0f };          // サンプル/ブロック単位の経過時間
    alignas(32) float triggerRand[8] = { 0.0f };  // NoteOn時に生成される固定乱数 [0, 1)

    // 変調ターゲット（モジュレーションマトリクスによって書き込まれる）
    alignas(32) float wavetablePosition[8] = { 0.0f };
    alignas(32) float formantShift[8] = { 0.0f };
    alignas(32) float detuneWidth[8] = { 0.0f };
    
    // Xorshift32 のシード状態（8ボイス独立）
    alignas(32) uint32_t xorState[8] = { 0 };
};

} // namespace DSP
