// ==========================================
// File: LpcVocoder.h
// LPCボコーダー統括モジュール（フェーズ2計画書v2 §4）
//
//   モジュレーター → リングバッファ → [フレーム毎] LpcAnalyzer → k/G ターゲット
//   キャリア(L/R) → [コントロールブロック毎に k/G を線形補間] → LpcLattice → 出力
//
//  - FilterbankVocoder と同じ「毎サンプル呼び出し」I/F
//  - formantFreeze: k/G のフレーム更新を停止（BitSpeek FrameRate=0 相当）
//  - ゲインGは att 5ms / rel 30ms の非対称平滑化（§7 クリック対策）
//  - JUCE非依存（スタブ環境で単体テスト可能）、RTセーフ（アロケーションなし）
// ==========================================
#pragma once

#include "LpcAnalyzer.h"
#include "LpcLattice.h"
#include <array>

class LpcVocoder
{
public:
    static constexpr double kInternalSampleRate = 16000.0;
    static constexpr int kHopSamples = 320;   // 既定 FRAME RATE 50Hz（M5でパラメータ化）
    static constexpr int kCtrlBlock = 32;     // 補間ブロック（PluginProcessorの制御レートと同一）
    static constexpr int kRingSize = 512;     // 2の冪、> kWindowSize
    static constexpr int kLatency16k = LpcAnalyzer::kWindowSize / 2; // PDC報告用 (128smp = 8ms)

    // 励起メイクアップ（スタブ実測較正: 母音入力+ノコギリキャリアで
    // 出力RMS ≒ 入力RMS となる値。最終段は既存BrickLimiterが保護）
    static constexpr float kMakeupGain = 2.0f;

    void prepare(double hostSampleRate);
    void reset();

    void setWindowType(int type) noexcept;

    // 1サンプル処理（16kHz領域）
    //  modulator : 分析側入力
    //  carrierL/R: 合成側キャリア
    //  order     : LPC次数 1..16
    //  freeze    : k/G フレーム更新停止
    //  gamma     : 帯域拡張係数（1.0=無効。character→γ で 0.97〜0.998）
    void processSample(float modulator, float carrierL, float carrierR,
                       float& outL, float& outR,
                       int order, bool freeze, float gamma = 1.0f) noexcept;

private:
    LpcAnalyzer mAnalyzer;
    LpcLattice mLatticeL, mLatticeR;

    std::array<float, kRingSize> mRing {};
    std::array<float, LpcAnalyzer::kWindowSize> mFrame {};

    std::array<float, LpcAnalyzer::kMaxOrder> mKTarget {};
    std::array<float, LpcAnalyzer::kMaxOrder> mKCur {};
    std::array<float, LpcAnalyzer::kMaxOrder> mKInc {};

    float mGTarget = 0.0f;   // 分析出力（励起正規化済み）
    float mGSmooth = 0.0f;   // att/rel 非対称平滑化後
    float mGCur = 0.0f;      // サンプル毎ランプ値
    float mGInc = 0.0f;

    int mWritePos = 0;
    int mFilled = 0;
    int mHopCounter = 0;
    int mCtrlCounter = 0;
    int mCurOrder = 16;
    int mWindowType = 0;

    float mExcNorm = 1.0f;   // 1/sqrt(Σw²)（窓タイプ依存）
    float mGAttCoef = 0.0f;  // att 5ms @ コントロールレート
    float mGRelCoef = 0.0f;  // rel 30ms @ コントロールレート
};
