// ==========================================
// File: FilterbankVocoder.h
// アナログ風フィルターバンク・ボコーダー (スカラー設計)
//
//  - BPF Bank (2次SVFカスケード) / Subtractive LR4 (4次Linkwitz-Riley)
//  - 8〜48バンドの動的追従
//  - フォルマント・シフト/ストレッチによる合成側フィルタ周波数の制御
//  - 帯域交互ステレオ・パンニング
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

class FilterbankVocoder
{
public:
    static constexpr int kMaxBands = 48;
    static constexpr double kInternalSampleRate = 16000.0;

    FilterbankVocoder();
    ~FilterbankVocoder() = default;

    void prepare(double sampleRate);
    void reset();

    // 1サンプル処理
    // modulator: 分析側に入力する音声サンプル
    // carrierL/R: 合成側のステレオキャリア入力サンプル
    // outL/R: ボコーディング後のステレオ出力サンプル (書き戻し)
    void processSample(float modulator, float carrierL, float carrierR,
                       float& outL, float& outR,
                       int bandCount, float character,
                       float formantShift, float formantStretch,
                       int filterbankType, float stereoWidth,
                       const std::array<std::atomic<float>, kMaxBands>& bandGains,
                       std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept;

private:
    struct SVFState
    {
        float s1 = 0.0f;
        float s2 = 0.0f;
        void reset() { s1 = 0.0f; s2 = 0.0f; }
    };

    // Subtractive モードのメイクアップゲイン (分析側エンベロープに適用)
    // BPF Bank は SVF-BP 2段カスケード (利得 Q^2=100/側) の高利得構造のため、
    // 正規化済み(ピーク0dB)のLR4バンクとの聴感レベル整合に +36dB を与える (数値実測による較正)
    static constexpr float kSubMakeup = 64.0f;

    struct LR4State
    {
        // 減算型バンド = LR4-HPF(下端エッジ) → LR4-LPF(上端エッジ) の直列 + ピーク正規化
        SVFState hiLp1, hiLp2; // 上端エッジ用 LR4-LPF (2次Butterworth×2段)
        SVFState loHp1, loHp2; // 下端エッジ用 LR4-HPF (2次Butterworth×2段)
        void reset() { hiLp1.reset(); hiLp2.reset(); loHp1.reset(); loHp2.reset(); }
    };

    void computeFilterCoeffs(float fc, float Q, float& g, float& k, float& a1) noexcept
    {
        g = std::tan(3.14159265f * fc / (float)kInternalSampleRate);
        k = 1.0f / Q;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    // 2次Butterworth (Q=0.707) LPFセクション (ZDF SVF)。2段カスケードでLR4-LPFを構成する。
    static float processLpfSection(SVFState& s, float x, float g, float a1) noexcept
    {
        const float v1 = a1 * (s.s1 + g * (x - s.s2));
        const float lp = s.s2 + g * v1;
        s.s1 = 2.0f * v1 - s.s1;
        s.s2 = 2.0f * lp - s.s2;
        return lp;
    }

    // 2次Butterworth (Q=0.707) HPFセクション (ZDF SVF)。2段カスケードでLR4-HPFを構成する。
    static float processHpfSection(SVFState& s, float x, float g, float a1) noexcept
    {
        const float v1 = a1 * (s.s1 + g * (x - s.s2));
        const float lp = s.s2 + g * v1;
        const float hp = x - 1.41421356f * v1 - lp;
        s.s1 = 2.0f * v1 - s.s1;
        s.s2 = 2.0f * lp - s.s2;
        return hp;
    }

    // バンド中心 (エッジの幾何平均) での利得を 0dB に揃える正規化ゲインの解析計算
    // |LR4-LPF(f)| = 1/(1+(f/fc)^4), |LR4-HPF(f)| = (f/fc)^4/(1+(f/fc)^4) を利用
    static float computeBandNorm(float eLo, float eHi) noexcept
    {
        const float fc = std::sqrt(eLo * eHi);
        const float tl = fc / eLo, th = fc / eHi;
        const float tl2 = tl * tl, th2 = th * th;
        const float xl4 = tl2 * tl2, xh4 = th2 * th2;
        const float magSq = (xl4 / (1.0f + xl4)) * (1.0f / (1.0f + xh4));
        return (magSq > 0.01f) ? (1.0f / magSq) : 100.0f; // 安全上限 +40dB
    }

    double mSampleRate = 44100.0;

    // 分析側バンド周波数
    std::array<float, kMaxBands> mBandF0 {};

    // Subtractive(減算型)用のバンド端周波数 (kMaxBands+1 エッジ) と分析側固定係数
    std::array<float, kMaxBands + 1> mBandEdges {};
    std::array<float, kMaxBands + 1> mEdgeG {};
    std::array<float, kMaxBands + 1> mEdgeA1 {};
    std::array<float, kMaxBands> mBandNorm {}; // 分析側ピーク正規化ゲイン

    // フィルタ状態変数
    std::array<std::array<SVFState, 2>, kMaxBands> mAnalSvf {}; // 2段カスケード用
    std::array<LR4State, kMaxBands> mAnalLr4 {};

    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfL {};
    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfR {};
    std::array<LR4State, kMaxBands> mSynthLr4L {};
    std::array<LR4State, kMaxBands> mSynthLr4R {};

    // 包絡線追従
    std::array<float, kMaxBands> mEnvValues {};
};
