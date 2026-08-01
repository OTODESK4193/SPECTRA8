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
#include "LspConverter.h"
#include <array>

class LpcVocoder
{
public:
    static constexpr double kInternalSampleRate = 16000.0;
    static constexpr int kHopSamples = 320;   // 既定 FRAME RATE 50Hz（M5でパラメータ化）
    static constexpr int kCtrlBlock = 32;     // 補間ブロック（PluginProcessorの制御レートと同一）
    // 2の冪、> kWindowSize。FMT SHIFT上げ(最大+24st=×4)時の分析窓スパン
    // (kWindowSize-1)*4≒1020サンプルの履歴を賄うため 2048 とする。
    static constexpr int kRingSize = 2048;
    static constexpr int kLatency16k = LpcAnalyzer::kWindowSize / 2; // PDC報告用 (160smp = 10ms)

    // プリエンファシス係数 (TMS5220系の古典値 15/16)。
    // 分析前に 1-αz^-1 で高域を持ち上げ高次フォルマントの推定精度を上げ、
    // 合成後に 1/(1-αz^-1) で戻す。こもりが減り子音の明瞭度が向上する。
    static constexpr float kPreemph = 0.9375f;

    // 励起メイクアップ。
    //  和音キャリアや Formant モーフィング時のパワー過剰による 0dBFS 超えを防ぐため、
    //  0.259f → 0.18f (約 -3.1dB) にキャリブレーション調整。
    //  【修正D 2026-08-02】ゲイン基準をプリエンファシス後→原音レベルへ変更したことで
    //  全体レベルが約 +12.7dB 上がるため、0.18f → 0.042f (-12.6dB) へ再校正。
    //  (音声素材の明るさで数dBは前後する。耳で最終確認すること)
    static constexpr float kMakeupGain = 0.042f;

    // 出力DCブロッカーのカットオフ。
    //  デエンファシス 1/(1-0.9375z⁻¹) は DC 利得が 16倍(+24dB)あるため、
    //  キャリア(特にカスタムWavetable)に僅かなDCオフセットがあると
    //  それが16倍されて低域が膨らみ、ラティスを飽和させることがある。
    static constexpr float kDcBlockHz = 20.0f;

    void prepare(double hostSampleRate);
    void reset();

    void setWindowType(int type) noexcept;

    // M5: フレームレート(Hz)。0以下でフリーズ(分析更新停止)。既定50Hz。
    void setFrameRate(float hz) noexcept
    {
        if (hz < 0.5f) { mRateFreeze = true;  mHopSamples = kHopSamples; }
        else           { mRateFreeze = false;
                         mHopSamples = std::max(1, (int)std::lround(kInternalSampleRate / (double)hz)); }
    }

    // M5: 反射係数kのビット量子化。0=無効。少ないほどレトロ(粗い声道)。
    void setQuantBits(int bits) noexcept { mQuantBits = (bits < 2) ? 0 : std::min(16, bits); }

    // M4: フレーム間フルホップ補間のドメイン。
    //  0=Step: 従来動作(制御ブロック32smpの高速ランプでターゲットへ即到達→低レートでカクつくトイ感)
    //  1=LSP : 線スペクトル周波数領域でホップ全長を掛けて滑らかにモーフ(フォルマント軌跡が自然)
    //  2=LAR : 対数面積比領域で同上(軽量。声道断面積の対数補間)
    //  変更は次の分析フレームから適用される(最大1ホップ=125ms@8Hzの遅延)。
    void setInterpolationMode(int mode) noexcept { mInterpMode = std::min(2, std::max(0, mode)); }

    // 1サンプル処理（16kHz領域）
    //  modulator : 分析側入力
    //  carrierL/R: 合成側キャリア
    //  order     : LPC次数 1..16
    //  freeze    : k/G フレーム更新停止
    //  gamma     : 帯域拡張係数（1.0=無効。character→γ で 0.97〜0.998）
    //  formantShiftSemitones : FMT SHIFT。分析窓を 2^(st/12) 倍のステップで
    //            リサンプルして読み出す(テープ変速式)。+でフォルマント上昇。ピッチは不変。
    //  formantStretch : FMT STRETCH。LSP(線スペクトル対)領域でフォルマント間隔を伸縮
    //                   (1.0=無効, >1=間隔拡大, <1=圧縮)。M4。
    void processSample(float modulator, float carrierL, float carrierR,
                       float& outL, float& outR,
                       int order, bool freeze, float gamma = 1.0f,
                       float formantShiftSemitones = 0.0f,
                       float formantStretch = 1.0f) noexcept;

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

    // M5: レトロ層
    int  mHopSamples = kHopSamples;  // 可変ホップ(フレームレート)
    bool mRateFreeze = false;        // frameRate=0 相当のフリーズ
    int  mQuantBits  = 0;            // k量子化ビット数(0=無効)

    // M4: フルホップ補間セグメント(前フレーム→新フレームをホップ全長で補間)
    //  mSegDomain は実際に使用中のドメイン(LSP変換失敗時は要求LSPでもLARへフォールバック)
    int mInterpMode = 0;             // 要求ドメイン(0=Step/1=LSP/2=LAR)
    int mSegDomain  = 0;             // 現セグメントのドメイン
    int mSegPos = 0;                 // セグメント内位置(サンプル)
    int mSegLen = kHopSamples;       // セグメント長(=構築時のホップ長)
    std::array<double, LpcAnalyzer::kMaxOrder> mReprPrev {};   // ドメイン表現の端点(前)
    std::array<double, LpcAnalyzer::kMaxOrder> mReprTarget {}; // ドメイン表現の端点(新)
    std::array<float,  LpcAnalyzer::kMaxOrder> mKSegPrev {};   // kドメイン端点(フォールバック用)

    // 新フレームのkターゲット確定後に補間セグメントを構築
    void setupSegment(int order) noexcept;
    // セグメント位置alpha(0..1)における補間kを算出
    void computeInterpK(int order, double alpha, float* kOut) const noexcept;

    float mDeempL = 0.0f;    // デエンファシス状態 (L)
    float mDeempR = 0.0f;    // デエンファシス状態 (R)

    // DCブロッカー (1次ハイパス) の状態。y[n] = x[n] - x[n-1] + R·y[n-1]
    float mDcX1L = 0.0f, mDcY1L = 0.0f;
    float mDcX1R = 0.0f, mDcY1R = 0.0f;
    // R = exp(-2π·fc/fs)。16kHz・20Hz なので約 0.99215
    static constexpr float kDcR = 0.992156f;

    float mExcNorm = 1.0f;   // 1/sqrt(Σw²)（窓タイプ依存）
    float mGAttCoef = 0.0f;  // att 5ms @ コントロールレート
    float mGRelCoef = 0.0f;  // rel 30ms @ コントロールレート
};
