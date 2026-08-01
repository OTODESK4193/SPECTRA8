// ==========================================
// File: LpcAnalyzer.h
// LPC分析器: 窓掛け → 自己相関(+白色雑音補正+ラグ窓) → Levinson-Durbin
// 出力は反射係数 k[1..P] とゲイン G のみ（フェーズ2計画書v2 §5.2）
//
// 符号規約（唯一の基準）: A(z) = 1 + Σ a_i z^-i
// 合成にLPC係数 a は使わない。|k| <= 0.995 クランプで構造的安定。
//
// JUCE非依存（スタブ環境で単体テスト可能）。内部演算は double。
// リアルタイム安全: analyzeFrame() はアロケーション・例外なし。
// ==========================================
#pragma once

#include <array>
#include <cmath>
#include <algorithm>

class LpcAnalyzer
{
public:
    static constexpr int    kMaxOrder = 16;
    // 20ms @16kHz。旧256(16ms)は既定ホップ320(50Hz)より短く、フレーム間に
    // 4msの未分析区間が生じて子音の取りこぼしの原因だった。窓長≥ホップ長に。
    static constexpr int    kWindowSize = 320;
    static constexpr double kInternalSampleRate = 16000.0;
    static constexpr double kReflClamp = 0.995;           // |k|クランプ
    static constexpr double kSigmaLagHz = 50.0;           // ラグ窓 σ
    static constexpr double kSilenceThresh = 1e-7;        // 無音判定 (窓掛け後 r0)

    enum WindowType { Hann = 0, Hamming = 1, Blackman = 2 };

    // 窓テーブル・ラグ窓テーブルの事前計算（非RT文脈で呼ぶ）
    void prepare();

    void setWindowType(int type) noexcept
    {
        mWindowType = std::min(2, std::max(0, type));
    }

    // 1フレーム分析。
    //  x     : kWindowSize サンプル（16kHz）
    //  order : 1..kMaxOrder
    //  kOut  : order 個の反射係数を書き込む（無音時は全0）
    //  gamma : 帯域拡張係数（1.0=無効。<1.0で極半径を γ 倍に縮小しフォルマントを平滑化）
    //          a_k ← a_k·γ^k と等価。character ノブ(M3)から 0.97〜0.998 が渡る。
    //  戻り値: ゲイン G = sqrt(E_P)（無音時は 0）
    float analyzeFrame(const float* x, int order, float* kOut, double gamma = 1.0) noexcept;

    // 窓エネルギー Σw² （励起レベル正規化 G/sqrt(Σw²) 用）
    float getWindowEnergy(int type) const noexcept
    {
        return mWinEnergy[(size_t)std::min(2, std::max(0, type))];
    }

    // 直前フレームの自己相関 r[0]（窓掛け後・白色雑音補正およびラグ窓の適用前）。
    //  ※【重要】LpcVocoder は分析前にプリエンファシスを掛けた波形を渡すため、
    //    これは「プリエンファシス後」の平均二乗値であり、原音のレベルではない。
    //    レベル整合ゲインには使わないこと (windowedEnergy() を使う)。
    double getLastFrameR0() const noexcept { return mLastR0; }

    // 【レベル基準】任意フレーム x の窓掛けエネルギー Σ(w·x)²。
    //  分析器と同一の窓を使うので、原音(プリエンファシス前)フレームを渡せば
    //  Σ(w·x)² / Σw² がそのフレームの平均二乗値になる。
    //  ゲイン算出でプリエンファシスの傾斜(150Hz -21dB 〜 6kHz +5dB)を持ち込まないための入口。
    double windowedEnergy(const float* x) const noexcept
    {
        const auto& w = mWindows[(size_t)mWindowType];
        double acc = 0.0;
        for (int n = 0; n < kWindowSize; ++n)
        {
            const double v = (double)x[n] * (double)w[(size_t)n];
            acc += v * v;
        }
        return acc;
    }

private:
    std::array<std::array<float, kWindowSize>, 3> mWindows {};
    std::array<float, 3> mWinEnergy {};
    std::array<double, kMaxOrder + 1> mLagWindow {};
    std::array<double, kWindowSize> mScratch {};
    int mWindowType = 0;
    double mLastR0 = 0.0;   // 直前フレームの r[0] (補正前)
};
