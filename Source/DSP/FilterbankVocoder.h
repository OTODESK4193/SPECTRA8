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

    // 分析専用パス (合成は行わない)。
    // LPCモードのようにフィルターバンク合成を実行しないときでも、
    // BANDS EQ のアナライザー(入力音声の帯域レベルメーター)を動かし続けるために使う。
    // 内部の分析フィルタ状態と mEnvValues、そして bandLevelsForUi のみを更新する。
    void analyzeForMeter(float modulator, int bandCount, float character, int filterbankType,
                         std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept
    {
        const int activeBands = juce::jlimit(8, kMaxBands, bandCount);
        for (int i = 0; i < activeBands; ++i)
        {
            updateAnalysisBand(i, modulator, filterbankType, character);
            bandLevelsForUi[(size_t)i].store(mEnvValues[(size_t)i]);
        }
    }

private:
    // 分析側(モジュレーター)の1バンド分の処理。mEnvValues[i] を更新する。
    // processSample と analyzeForMeter の両方から呼ばれる共通ロジック。
    inline void updateAnalysisBand(int i, float modulator, int filterbankType, float character) noexcept
    {
        const float f0 = mBandF0[(size_t)i];
        const float Q = 10.0f; // BPF用のクオリティファクタ

        float analOut = 0.0f;
        float g_anal, k_anal, a1_anal;
        computeFilterCoeffs(f0, Q, g_anal, k_anal, a1_anal);

        if (filterbankType == 0) // Bandpass Bank
        {
            // ZDF SVF 1段目 (BPF)
            auto& s1 = mAnalSvf[(size_t)i][0];
            float v1 = a1_anal * (s1.s1 + g_anal * (modulator - s1.s2));
            float y_bp = v1;
            float y_lp = s1.s2 + g_anal * v1;
            s1.s1 = 2.0f * y_bp - s1.s1;
            s1.s2 = 2.0f * y_lp - s1.s2;

            // ZDF SVF 2段目 (BPF)
            auto& s2 = mAnalSvf[(size_t)i][1];
            float v1_2 = a1_anal * (s2.s1 + g_anal * (y_bp - s2.s2));
            float y_bp_2 = v1_2;
            float y_lp_2 = s2.s2 + g_anal * v1_2;
            s2.s1 = 2.0f * y_bp_2 - s2.s1;
            s2.s2 = 2.0f * y_lp_2 - s2.s2;

            analOut = y_bp_2;
        }
        else // Subtractive (バンド端エッジの8次HPF→8次LPF直列 + ピーク正規化)
        {
            auto& sub = mAnalSub[(size_t)i];
            const float gLo  = mEdgeG[(size_t)i];      // 下端エッジ (HPF)
            const float a1Lo = mEdgeA1[(size_t)i];
            const float gHi  = mEdgeG[(size_t)i + 1];  // 上端エッジ (LPF)
            const float a1Hi = mEdgeA1[(size_t)i + 1];

            float y = modulator;
            for (int sec = 0; sec < 4; ++sec) y = processLpfSection(sub.hiLp[(size_t)sec], y, gHi, a1Hi);
            for (int sec = 0; sec < 4; ++sec) y = processHpfSection(sub.loHp[(size_t)sec], y, gLo, a1Lo);

            // 正規化(ピーク0dB) + BPF Bankとの聴感レベル整合メイクアップ
            analOut = y * mBandNorm[(size_t)i] * kSubMakeup;
        }

        // エンベロープ追従（キャラクター値でアタック/リリースタイムを調整）
        float env = std::abs(analOut);
        float att = 0.005f + character * 0.045f;
        float rel = 0.001f + character * 0.009f;

        float coeff = (env > mEnvValues[(size_t)i]) ? att : rel;
        mEnvValues[(size_t)i] += coeff * (env - mEnvValues[(size_t)i]);
    }

    struct SVFState
    {
        float s1 = 0.0f;
        float s2 = 0.0f;
        void reset() { s1 = 0.0f; s2 = 0.0f; }
    };

    // Subtractive モードのメイクアップゲイン (分析側エンベロープに適用)
    // BPF Bank は SVF-BP 2段カスケード (利得 Q^2=100/側) の高利得構造で常時リミッター駆動のため、
    // 正規化済み(ピーク0dB)の減算型バンクとの聴感レベル整合に +48dB を与える
    // (ユーザー実測: BPF +1.58dB(クリップ레일) vs 旧Sub -18.5dB → 差約20dB を補正した較正値)
    static constexpr float kSubMakeup = 256.0f;

    // 出力ユニティ・トリム: BPF Bank は Q²=100 の高利得構造で既定設定の出力が
    // 約 +49.8dB(実測, Ableton)まで持ち上がる。LPCモード(≈0dB)と揃えるため、
    // フィルターバンク合成の総和に -49.8dB のトリムを掛けて出力を約0dBへ落とす。
    // (Subtractive は kSubMakeup で BPF に整合済みのため同じトリムで両タイプが揃う)
    static constexpr float kOutputTrim = 0.003236f; // 10^(-49.8/20)

    struct SubBandState
    {
        // 減算型バンド = 8次HPF(下端エッジ) → 8次LPF(上端エッジ) の直列 + ピーク正規化
        // (2次Butterworth Q=0.707 を4段カスケード、スロープ48dB/oct)
        std::array<SVFState, 4> hiLp; // 上端エッジ用 LPF セクション
        std::array<SVFState, 4> loHp; // 下端エッジ用 HPF セクション
        void reset() { for (auto& s : hiLp) s.reset(); for (auto& s : loHp) s.reset(); }
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
    // Butterworth2次×2段: |LPF| = 1/(1+(f/fc)^4), |HPF| = (f/fc)^4/(1+(f/fc)^4)
    // 4段カスケード(8次)はその2乗
    static float computeBandNorm(float eLo, float eHi) noexcept
    {
        const float fc = std::sqrt(eLo * eHi);
        const float tl = fc / eLo, th = fc / eHi;
        const float tl2 = tl * tl, th2 = th * th;
        const float xl4 = tl2 * tl2, xh4 = th2 * th2;
        const float m1 = (xl4 / (1.0f + xl4)) * (1.0f / (1.0f + xh4));
        const float mag = m1 * m1; // 8次 = 4次特性の2乗
        return (mag > 0.005f) ? (1.0f / mag) : 200.0f; // 安全上限 +46dB
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
    std::array<SubBandState, kMaxBands> mAnalSub {};

    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfL {};
    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfR {};
    std::array<SubBandState, kMaxBands> mSynthSubL {};
    std::array<SubBandState, kMaxBands> mSynthSubR {};

    // 包絡線追従
    std::array<float, kMaxBands> mEnvValues {};
};
