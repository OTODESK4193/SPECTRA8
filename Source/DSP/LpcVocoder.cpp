// ==========================================
// File: LpcVocoder.cpp
// フェーズ2計画書v2 §4/§5 準拠
// ==========================================
#include "LpcVocoder.h"
#include <cmath>
#include <algorithm>

namespace
{
    // 反射係数k → 直接形係数a (step-up再帰)。order ≤ 16 なので十分軽い。
    inline void kToA(const float* k, int order, double* a) noexcept
    {
        a[0] = 1.0;
        for (int i = 1; i <= order; ++i) a[i] = 0.0;
        double tmp[LpcAnalyzer::kMaxOrder + 1];
        for (int m = 0; m < order; ++m)
        {
            for (int i = 0; i <= m + 1; ++i) tmp[i] = a[i];
            for (int i = 1; i <= m + 1; ++i)
                a[i] = tmp[i] + (double)k[m] * tmp[m + 1 - i];
        }
    }

    // 全極フィルタ 1/A(z) の「励起重み+デエンファシス込み」エネルギー。
    //   【修正B】評価グリッドを 64点 → 128点 に倍増し、Order 16 等の高次フォルマントが
    //   周波数グリッドの間隙に落ちてエネルギーを過小評価(ゲイン過大算出)するのを防止。
    //
    //   【修正E 2026-08-02】励起重みを 1/f² から「フラット(白色)」へ変更。
    //   LPCの 1/A(z) は白色残差を前提に同定された伝達関数なので、
    //   合成側の励起も白色でなければ出力スペクトルに余計な傾斜が乗る。
    //   processSample() 側でキャリアをプリエンファシスして白色化するようにしたため、
    //   ここの重みも白色前提に揃える。
    constexpr int kEnergyBins = 128;

    // 評価グリッド (60〜7600Hz 対数等間隔) の cos/sin を事前計算しておくテーブル。
    //  毎フレームだけでなく補間セグメント中の毎制御ブロックからも呼ばれるため、
    //  三角関数をテーブル化してリアルタイム負荷を抑える。
    struct EnergyGrid
    {
        double cosWi[kEnergyBins][LpcAnalyzer::kMaxOrder + 1];
        double sinWi[kEnergyBins][LpcAnalyzer::kMaxOrder + 1];
        double deemp2[kEnergyBins];   // 1/|1 - kPre·e^-jw|²

        EnergyGrid()
        {
            constexpr double kPre = 15.0 / 16.0;   // プリエンファシス係数と一致させること
            for (int m = 0; m < kEnergyBins; ++m)
            {
                const double f = 60.0 * std::pow(7600.0 / 60.0, (double)m / (double)(kEnergyBins - 1));
                const double w = 6.283185307179586 * f / LpcVocoder::kInternalSampleRate;
                for (int i = 0; i <= LpcAnalyzer::kMaxOrder; ++i)
                {
                    cosWi[m][i] = std::cos(w * (double)i);
                    sinWi[m][i] = std::sin(w * (double)i);
                }
                const double dr = 1.0 - kPre * std::cos(w);
                const double di = kPre * std::sin(w);
                deemp2[m] = 1.0 / std::max(1e-12, dr * dr + di * di);
            }
        }
    };
    const EnergyGrid gEnergyGrid;   // 静的初期化 (音声スレッド開始前に構築される)

    // stride: 1 = 全128点(フレーム毎の高精度)、4 = 32点(制御ブロック毎の軽量版)
    inline double weightedEnergy(const double* a, int order, int stride = 1) noexcept
    {
        double wE = 0.0;
        int cnt = 0;
        for (int m = 0; m < kEnergyBins; m += stride)
        {
            const double* cw = gEnergyGrid.cosWi[m];
            const double* sw = gEnergyGrid.sinWi[m];
            double re = 1.0, im = 0.0;
            for (int i = 1; i <= order; ++i)
            {
                re += a[i] * cw[i];
                im -= a[i] * sw[i];
            }
            wE += gEnergyGrid.deemp2[m] / std::max(1e-18, re * re + im * im);
            ++cnt;
        }
        // 点数に依存しない「平均」にしておく (stride を変えても値が揃う)
        return wE / (double)std::max(1, cnt);
    }

    inline double weightedEnergyFromK(const float* k, int order, int stride = 1) noexcept
    {
        double a[LpcAnalyzer::kMaxOrder + 1];
        kToA(k, order, a);
        return weightedEnergy(a, order, stride);
    }
}

void LpcVocoder::prepare(double /*hostSampleRate*/)
{
    mAnalyzer.prepare();

    // ゲイン平滑化係数（コントロールブロック = 32smp @16kHz = 2ms）
    const double blockDur = (double)kCtrlBlock / kInternalSampleRate;
    mGAttCoef = (float)std::exp(-blockDur / 0.005);   // att 5ms
    mGRelCoef = (float)std::exp(-blockDur / 0.030);   // rel 30ms

    // 修正F: サンプル単位の1極平滑 (1 - exp(-1/(τ·fs)) が到達係数)
    mVoicingCoef = 1.0f - (float)std::exp(-1.0 / (0.005 * kInternalSampleRate));  // 5ms
    mRmsCoef     = 1.0f - (float)std::exp(-1.0 / (0.020 * kInternalSampleRate));  // 20ms

    setWindowType(mWindowType);
    reset();
}

void LpcVocoder::reset()
{
    mLatticeL.reset();
    mLatticeR.reset();
    mRing.fill(0.0f);
    mKTarget.fill(0.0f);
    mKCur.fill(0.0f);
    mKInc.fill(0.0f);
    mGTarget = mGSmooth = mGCur = mGInc = 0.0f;
    mDeempL = mDeempR = 0.0f;
    mPreCarL = mPreCarR = 0.0f;
    mVoicingSm = 1.0f;
    mCarRms = 0.0f;
    mDcX1L = mDcY1L = mDcX1R = mDcY1R = 0.0f;
    mWritePos = 0;
    mFilled = 0;
    mHopCounter = 0;
    mCtrlCounter = 0;
    // M4: 補間セグメントも初期化(Stepに戻し、次フレームで再構築)
    mSegDomain = 0;
    mSegPos = 0;
    mSegLen = mHopSamples;
    mReprPrev.fill(0.0);
    mReprTarget.fill(0.0);
    mKSegPrev.fill(0.0f);
}

// ---- M4: フルホップ補間 ----------------------------------------
namespace
{
    // k <-> LAR(対数面積比)。|k|<=0.995 なので atanh は有限(|r|<=2.994)。
    inline double larFromK(float k) noexcept
    {
        const double kd = std::min(0.995, std::max(-0.995, (double)k));
        return std::atanh(kd);
    }
    inline float kFromLar(double r) noexcept
    {
        return (float)std::min(0.995, std::max(-0.995, std::tanh(r)));
    }
}

void LpcVocoder::setupSegment(int order) noexcept
{
    // 前端点 = ラティスが今まさに使っている平滑値 mKCur (不連続なし)。
    // ほぼ完了済み(残り1制御ブロック未満)のLSPセグメントから連続する場合は
    // 変換を省き repr を引き継ぐ(誤差はブロックランプ1回分以下=Step同等)。
    // ※ホップ長が32の倍数でない場合 mSegPos は mSegLen に届かないため余裕を持たせる。
    const bool prevLspDone = (mSegDomain == 1 && mSegPos + kCtrlBlock >= mSegLen);
    for (int p = 0; p < order; ++p)
        mKSegPrev[(size_t)p] = mKCur[(size_t)p];

    mSegLen = std::max(kCtrlBlock, mHopSamples);
    mSegPos = 0;
    mSegDomain = 0;
    if (mInterpMode == 0)
        return; // Step: 従来の高速ランプ動作(数値完全一致)

    if (mInterpMode == 1)
    {
        // LSP: 両端点を k→a→LSF 変換。どちらか失敗なら LAR へフォールバック。
        double aT[LpcAnalyzer::kMaxOrder + 1];
        double lt[LpcAnalyzer::kMaxOrder];
        LspConverter::reflToLpc(mKTarget.data(), order, aT);
        if (LspConverter::lpcToLsf(aT, order, lt))
        {
            if (prevLspDone)
            {
                // 直前セグメントの終端repr = 現在k とみなせる(ランプ完了済み)
                for (int p = 0; p < order; ++p)
                    mReprPrev[(size_t)p] = mReprTarget[(size_t)p];
            }
            else
            {
                double aP[LpcAnalyzer::kMaxOrder + 1];
                double lp[LpcAnalyzer::kMaxOrder];
                LspConverter::reflToLpc(mKSegPrev.data(), order, aP);
                if (!LspConverter::lpcToLsf(aP, order, lp))
                    goto fallbackLar; // 前端点の変換失敗
                for (int p = 0; p < order; ++p)
                    mReprPrev[(size_t)p] = lp[p];
            }
            for (int p = 0; p < order; ++p)
                mReprTarget[(size_t)p] = lt[p];
            mSegDomain = 1;
            return;
        }
    }

fallbackLar:
    // LAR (要求がLARの場合、およびLSP変換失敗時のフォールバック)
    for (int p = 0; p < order; ++p)
    {
        mReprPrev[(size_t)p]   = larFromK(mKSegPrev[(size_t)p]);
        mReprTarget[(size_t)p] = larFromK(mKTarget[(size_t)p]);
    }
    mSegDomain = 2;
}

void LpcVocoder::computeInterpK(int order, double alpha, float* kOut) const noexcept
{
    if (mSegDomain == 1)
    {
        // LSF領域の要素毎lerp(両端が昇順なら順序は保存される)。数値安全の最小間隔を強制。
        double lsf[LpcAnalyzer::kMaxOrder];
        for (int p = 0; p < order; ++p)
            lsf[p] = mReprPrev[(size_t)p] + (mReprTarget[(size_t)p] - mReprPrev[(size_t)p]) * alpha;
        lsf[0] = std::min(3.12, std::max(0.02, lsf[0]));
        for (int i = 1; i < order; ++i)
        {
            lsf[i] = std::min(3.13, std::max(0.02, lsf[i]));
            if (lsf[i] <= lsf[i - 1] + 1e-4)
                lsf[i] = lsf[i - 1] + 1e-4;
        }
        double a[LpcAnalyzer::kMaxOrder + 1];
        LspConverter::lsfToLpc(lsf, order, a);
        if (LspConverter::lpcToRefl(a, order, kOut))
        {
            for (int p = 0; p < order; ++p)
                kOut[p] = std::min(0.995f, std::max(-0.995f, kOut[p]));
            return;
        }
        // step-down失敗(まれ): kドメイン線形へフォールバック
        for (int p = 0; p < order; ++p)
            kOut[p] = (float)((double)mKSegPrev[(size_t)p]
                     + ((double)mKTarget[(size_t)p] - (double)mKSegPrev[(size_t)p]) * alpha);
        return;
    }

    // LAR
    for (int p = 0; p < order; ++p)
        kOut[p] = kFromLar(mReprPrev[(size_t)p]
                 + (mReprTarget[(size_t)p] - mReprPrev[(size_t)p]) * alpha);
}

void LpcVocoder::setWindowType(int type) noexcept
{
    mWindowType = std::min(2, std::max(0, type));
    mAnalyzer.setWindowType(mWindowType);
    const float e = mAnalyzer.getWindowEnergy(mWindowType);
    mExcNorm = (e > 1e-12f) ? 1.0f / std::sqrt(e) : 1.0f;
}

void LpcVocoder::processSample(float modulator, float carrierL, float carrierR,
                               float& outL, float& outR,
                               int order, bool freeze, float gamma,
                               float formantShiftSemitones, float formantStretch,
                               float voicing) noexcept
{
    order = std::min(LpcAnalyzer::kMaxOrder, std::max(1, order));

    // 次数変更時は状態をリセット（残留状態による不整合を防止）
    if (order != mCurOrder)
    {
        mCurOrder = order;
        mLatticeL.reset();
        mLatticeR.reset();
        mKTarget.fill(0.0f);
        mKCur.fill(0.0f);
        mKInc.fill(0.0f);
        mSegDomain = 0;   // M4: 次数変更時は補間セグメントも破棄(次フレームで再構築)
        mSegPos = 0;
    }

    // 1. モジュレーターをリングバッファへ
    mRing[(size_t)mWritePos] = modulator;
    mWritePos = (mWritePos + 1) & (kRingSize - 1);
    if (mFilled < kRingSize)
        ++mFilled;

    // 2. フレーム毎の分析（ターゲット更新）。ホップ長はフレームレート依存(M5)。
    if (++mHopCounter >= mHopSamples)
    {
        mHopCounter = 0;
        // freeze(明示) または frameRate=0 のときは分析更新を停止
        const bool doFreeze = freeze || mRateFreeze;

        // FMT SHIFT: 分析窓を factor=2^(st/12) 倍のステップでリサンプルして読み出す。
        //  factor>1 → 声道形状を時間圧縮して分析 → フォルマント上昇（テープ早回し相当）。
        //  合成は 16kHz でそのまま行うためピッチ(キャリア)は不変。
        const double factor = std::pow(2.0, (double)formantShiftSemitones / 12.0);
        const double span   = (double)(LpcAnalyzer::kWindowSize - 1) * factor; // 必要な過去履歴長
        const int    needed = std::min(kRingSize - 2, (int)std::ceil(span) + 2);

        if (!doFreeze && mFilled >= needed)
        {
            if (std::abs(factor - 1.0) < 1e-6)
            {
                // シフト無し: 従来通り等間隔読み出し（数値完全一致）
                const int start = (mWritePos - LpcAnalyzer::kWindowSize + kRingSize) & (kRingSize - 1);
                for (int n = 0; n < LpcAnalyzer::kWindowSize; ++n)
                    mFrame[(size_t)n] = mRing[(size_t)((start + n) & (kRingSize - 1))];
            }
            else
            {
                // 分数ステップ読み出し（線形補間）。窓の最後尾を最新サンプルに揃える。
                const int newest = (mWritePos - 1 + kRingSize) & (kRingSize - 1);
                for (int n = 0; n < LpcAnalyzer::kWindowSize; ++n)
                {
                    const double back = (double)(LpcAnalyzer::kWindowSize - 1 - n) * factor;
                    const double pos  = (double)newest - back;      // ring index（負もあり得る）
                    const double fp   = std::floor(pos);
                    const double fr   = pos - fp;
                    const int i0 = (((int)fp) % kRingSize + kRingSize) & (kRingSize - 1);
                    const int i1 = (i0 + 1) & (kRingSize - 1);
                    mFrame[(size_t)n] = (float)((double)mRing[(size_t)i0] * (1.0 - fr)
                                              + (double)mRing[(size_t)i1] * fr);
                }
            }

            // 【レベル基準】プリエンファシスを掛ける前に、原音フレームの窓掛けエネルギーを取る。
            //  これを励起ゲインの基準に使う。詳細は下の mGTarget 算出部のコメント参照。
            const double rawR0 = mAnalyzer.windowedEnergy(mFrame.data());

            // プリエンファシス (1 - kPreemph·z^-1) を窓に適用してから分析。
            // 高域を持ち上げてLPCの極を高次フォルマントにも配分させる。
            // 出力側のデエンファシスと対で周波数特性は復元される。
            // frame[0]は前サンプル不明のためそのまま(窓端はほぼ0のため影響なし)。
            for (int n = LpcAnalyzer::kWindowSize - 1; n >= 1; --n)
                mFrame[(size_t)n] -= kPreemph * mFrame[(size_t)(n - 1)];

            const float g = mAnalyzer.analyzeFrame(mFrame.data(), order, mKTarget.data(), (double)gamma);

            // ---- 励起ゲイン: 「合成出力レベル = 入力レベル」で決める ----
            //  旧実装は G = sqrt(E_P)（残差RMS）をそのまま使っていたが、
            //  次数を上げるほど E_P は小さくなる一方で 1/A(z) の利得は大きくなり、
            //  両者が相殺しないため LPC ORDER を変えるだけで音量が最大 8.6dB 動いていた
            //  (Docs/sim/lpc2.py で実測。8→16 で +6〜+8.6dB)。
            //
            //  合成出力のレベルは G²×wE (wE = 励起重み付きフィルタエネルギー) に比例する。
            //  そこで、そのフレームの平均二乗値 r0/Σw² に一致するよう
            //      G = sqrt( (r0/Σw²) / wE )
            //  と置く。定義から次数に依存せず、入力のダイナミクスもそのまま保たれる
            //  (シミュレーションで Order 間の差 0.00dB / 入力-20dB → 出力-20dB を確認)。
            //
            //  【修正D 2026-08-02】この r0 に mAnalyzer.getLastFrameR0() を使っていたが、
            //  それは「プリエンファシス後」の平均二乗値だった。
            //  プリエンファシス 1-0.9375z⁻¹ は 150Hz を -21dB / 6kHz を +5dB する強い傾斜を持つため、
            //  入力の明るさだけでゲイン基準が最大 26dB ずれ、息・子音・サ行の瞬間に
            //  WET が突発的に跳ね上がっていた。
            //  実測 (DRY/WET 相関 0.789・傾き 1.02dB/dB、息フレーム +16.9dB / 母音フレーム -7.7dB。
            //   同一RMSの帯域ノイズ掃引で 150Hz→6kHz の出力差 27.0dB)。
            //  基準を rawR0 (プリエンファシス前の窓掛けエネルギー) に変更し、帯域差 27.0dB→6.4dB、
            //  DRY基準のレベル誤差 p99 +22.0dB→+11.1dB、正規化ピーク 2.01→0.39。
            //  ※この変更で全体レベルが約 +12.7dB 上がるため kMakeupGain を 0.18→0.042 に再校正済み。
            if (g <= 0.0f)
            {
                mGTarget = 0.0f;   // 無音フレーム
            }
            else
            {
                const double winE = (double)mAnalyzer.getWindowEnergy(mWindowType);
                const double r0   = rawR0;   // 【修正D】原音(プリエンファシス前)レベル基準
                const double wE   = weightedEnergyFromK(&mKTarget[0], order);
                mGTarget = (wE > 1e-18 && winE > 1e-12 && r0 > 0.0)
                             ? (float)std::sqrt((r0 / winE) / wE)
                             : 0.0f;
            }

            // M4改2: FMT STRETCH（スペクトル包絡リサンプル方式）。
            //  現在のLPC包絡 P(w)=1/|A(w)|² を周波数グリッドで評価し、
            //  周波数軸を P'(w)=P(w/stretch) でワープ(FilterBankの「帯域中心 f×stretch」
            //  と同一の写像)、逆DFTで自己相関を再構成して Levinson で k を再フィットする。
            //  旧LSFワープ方式は変換失敗時のフォールバックや帯域幅拡張の段階切替が
            //  フレーム毎に発生して「ザラつき」ノイズの原因になっていた。
            //  本方式は根探索が無く常に成功し、stretch・入力に対して連続的に変化する。
            if (std::abs(formantStretch - 1.0f) > 0.01f)
            {
                double aS[LpcAnalyzer::kMaxOrder + 1];
                LspConverter::reflToLpc(mKTarget.data(), order, aS);

                // 1) 包絡評価 + ワープ: P'[m] = 1/|A(w_m/stretch)|² (π超は端値ホールド)
                constexpr int N = 128;
                constexpr double PI_ = 3.14159265358979;
                double Pw[N];
                for (int m = 0; m < N; ++m)
                {
                    const double w = PI_ * (double)m / (double)(N - 1);
                    const double ws = std::min(PI_, w / (double)formantStretch);
                    double re = 1.0, im = 0.0;
                    for (int i = 1; i <= order; ++i)
                    {
                        re += aS[i] * std::cos(ws * (double)i);
                        im -= aS[i] * std::sin(ws * (double)i);
                    }
                    Pw[m] = 1.0 / std::max(1e-12, re * re + im * im);
                }

                // 2) 逆DFTで自己相関を再構成 (台形則、端点半分重み)
                double r[LpcAnalyzer::kMaxOrder + 1];
                for (int j = 0; j <= order; ++j)
                {
                    double acc = 0.5 * Pw[0] + 0.5 * Pw[N - 1] * std::cos(PI_ * (double)j);
                    for (int m = 1; m < N - 1; ++m)
                        acc += Pw[m] * std::cos(PI_ * (double)m * (double)j / (double)(N - 1));
                    r[j] = acc;
                }
                // 数値衛生: 白色雑音補正 + ラグ窓 (σ=50Hz。分析側と同一の平滑度)
                r[0] = r[0] * 1.0001 + 1e-12;
                for (int j = 1; j <= order; ++j)
                {
                    const double arg = 6.283185307179586 * 50.0 * (double)j / kInternalSampleRate;
                    r[j] *= std::exp(-0.5 * arg * arg);
                }

                // 3) Levinson-Durbin で k を再フィット (|k|クランプで構造的に安定)
                double aN[LpcAnalyzer::kMaxOrder + 1] = {};
                double anew[LpcAnalyzer::kMaxOrder + 1] = {};
                float  kS[LpcAnalyzer::kMaxOrder] = {};
                double E = r[0];
                bool ok = (E > 1e-15);
                if (ok)
                {
                    for (int i = 1; i <= order; ++i)
                    {
                        double acc = r[i];
                        for (int j = 1; j < i; ++j)
                            acc += aN[j] * r[i - j];
                        double ki = -acc / E;
                        ki = std::min(0.995, std::max(-0.995, ki));
                        kS[i - 1] = (float)ki;
                        for (int j = 1; j < i; ++j)
                            anew[j] = aN[j] + ki * aN[i - j];
                        anew[i] = ki;
                        for (int j = 1; j <= i; ++j)
                            aN[j] = anew[j];
                        E *= (1.0 - ki * ki);
                        if (E < 1e-12) { E = std::max(E, 0.0); break; }
                    }
                    aN[0] = 1.0;
                }

                if (ok)
                {
                    // 4) ゲイン補償: 励起重み+デエンファシス込みのエネルギー比で
                    //    聴感レベルを維持し、max|H|比のピーク境界でクリップを防ぐ
                    //    【修正E】重みは weightedEnergy() と同じく白色前提(フラット)に統一
                    auto evalFilter = [order](const double* aa, double& maxH2, double& wE)
                    {
                        maxH2 = 0.0; wE = 0.0;
                        for (int m = 0; m < kEnergyBins; m += 2)
                        {
                            const double* cw = gEnergyGrid.cosWi[m];
                            const double* sw = gEnergyGrid.sinWi[m];
                            double re = 1.0, im = 0.0;
                            for (int i = 1; i <= order; ++i)
                            {
                                re += aa[i] * cw[i];
                                im -= aa[i] * sw[i];
                            }
                            const double h2 = gEnergyGrid.deemp2[m] / std::max(1e-18, re * re + im * im);
                            wE += h2;
                            if (h2 > maxH2) maxH2 = h2;
                        }
                    };
                    double mh0, e0, mh, e;
                    evalFilter(aS, mh0, e0);
                    evalFilter(aN, mh, e);
                    if (e > 1e-18 && e0 > 1e-18 && mh > 1e-18)
                    {
                        const double eComp   = std::sqrt(e0 / e);
                        const double pkBound = 1.5 * std::sqrt(mh0 / mh);
                        mGTarget *= (float)std::min(20.0, std::max(0.01, std::min(eComp, pkBound)));
                    }
                    for (int p = 0; p < order; ++p)
                        mKTarget[(size_t)p] = kS[p];
                }
            }

            // M5: 反射係数kのビット量子化（BitSpeek風レトロ）。
            //  LAR(対数面積比 atanh(k))領域の対称量子化 Q=2^(bits-1)-1。
            //  kドメイン一様量子化は |k|→1 付近で極半径への感度が極端に高く、丸め上げで
            //  共振が急増しクリップの原因になるため、感度が均等なLAR領域で丸める
            //  (TMS5220系実機の非一様量子化テーブル相当)。
            //  さらに量子化によるフィルタ利得変化をゲイン補償して低ビット時の音量暴れを防ぐ
            //  (レトロな粗さは維持される)。
            //
            //  【補償方式の変更】以前は白色雑音基準の G *= sqrt(Π(1-kq²)/Π(1-k²)) を
            //  使っていたが、本機のキャリアはノコギリ/コム(1/f²スペクトル)であり
            //  この式では全く足りなかった。実測で 3bit 時に +15.8dB の音量跳ね上がりが
            //  残っていた(量子化でkが 0.963/0.995 へ寄り超高Q共振が立つため)。
            //  FMT STRETCH と同じ「励起重み付きエネルギー比」に統一する。
            if (mQuantBits >= 2)
            {
                const double eBefore = weightedEnergyFromK(&mKTarget[0], order);

                const double Q = (double)((1 << (mQuantBits - 1)) - 1);
                constexpr double kLarMax = 2.994;   // atanh(0.995)
                for (int p = 0; p < order; ++p)
                {
                    const double k0   = std::min(0.995, std::max(-0.995, (double)mKTarget[(size_t)p]));
                    const double lar  = std::atanh(k0);
                    const double larQ = std::round(lar * (Q / kLarMax)) * (kLarMax / Q);
                    const double kq   = std::min(0.995, std::max(-0.995, std::tanh(larQ)));
                    mKTarget[(size_t)p] = (float)kq;
                }

                const double eAfter = weightedEnergyFromK(&mKTarget[0], order);
                if (eBefore > 1e-18 && eAfter > 1e-18)
                {
                    // 0.05〜2.0 に制限 (極端な補正でかえって不自然にならないように)
                    const double comp = std::sqrt(eBefore / eAfter);
                    mGTarget *= (float)std::min(2.0, std::max(0.05, comp));
                }
            }

            // M4: フルホップ補間セグメントを構築(現在値→新ターゲットをホップ全長でモーフ)
            setupSegment(order);
        }
    }

    // 3. コントロールブロック毎に補間ランプを再計算
    if (mCtrlCounter == 0)
    {
        // ゲインの非対称平滑化（att 5ms / rel 30ms）
        // 【修正A】目標ゲインが減少する場合(mGTarget < mGSmooth)は、
        // 30msのリリース時間を待たずに即座にmGTargetへ引き下げる。
        // これにより、フィルタが2msで鋭いフォルマントへ変化した際の過渡的な音量膨脹(+28dB)を防止。
        //
        // 【修正G 2026-08-02】この即時リリースを Step (mSegDomain == 0) 限定にしていたのが、
        //  LSP/LAR 補間モードだけ全体レベルが +5〜6.7dB 高くリミッターに常時当たっていた真因だった。
        //  LSP/LAR は 30ms リリースのままだったため、減衰区間でゲインが残り続けていた。
        //  全モードで即時リリースに統一すると三者の音量が揃う
        //  (実測 全体RMS: Step -27.4 / LSP -27.5 / LAR -27.4 dB、ピーク 0.51/0.50/0.50。
        //   補正前は LSP -22.2 / LAR -22.1 dB でピーク 1.0000)。
        if (mGTarget < mGSmooth)
        {
            mGSmooth = mGTarget;
        }
        else
        {
            const float coef = (mGTarget > mGSmooth) ? mGAttCoef : mGRelCoef;
            mGSmooth = mGTarget + (mGSmooth - mGTarget) * coef;
        }

        constexpr float inv = 1.0f / (float)kCtrlBlock;
        if (mSegDomain == 0)
        {
            // Step(従来): ブロック内ランプでターゲットへ即到達(旧動作と数値完全一致)
            for (int p = 0; p < order; ++p)
                mKInc[(size_t)p] = (mKTarget[(size_t)p] - mKCur[(size_t)p]) * inv;
        }

        if (mSegDomain == 0)
        {
            // Step(従来): ブロック内ランプでターゲットへ即到達(旧動作と数値完全一致)
            for (int p = 0; p < order; ++p)
                mKInc[(size_t)p] = (mKTarget[(size_t)p] - mKCur[(size_t)p]) * inv;
        }
        else
        {
            // M4 フルホップ補間: このブロック終端時点の補間値をブロックターゲットにする
            mSegPos = std::min(mSegLen, mSegPos + kCtrlBlock);
            const double alpha = (double)mSegPos / (double)mSegLen;
            float kBlk[LpcAnalyzer::kMaxOrder];
            computeInterpK(order, alpha, kBlk);
            for (int p = 0; p < order; ++p)
                mKInc[(size_t)p] = (kBlk[p] - mKCur[(size_t)p]) * inv;

            // ※補間途中の k から wE を測り直して G を補正する案も実装・計測したが、
            //   上の即時リリース統一を入れると効果は 0.2dB 未満で CPU だけ増えたため採用しない。
        }
        mGInc = (mGSmooth - mGCur) * inv;
    }
    if (++mCtrlCounter >= kCtrlBlock)
        mCtrlCounter = 0;

    // 4. サンプル毎の線形ランプ（|k|<1 は補間途中でも保存 → 常時安定）
    for (int p = 0; p < order; ++p)
        mKCur[(size_t)p] += mKInc[(size_t)p];
    mGCur += mGInc;

    // 5-a.【修正E】キャリア白色化。
    //  LPCの 1/A(z) は「白色残差 → プリエンファシス済み音声」の伝達関数として同定される。
    //  よって合成側の励起も白色でなければならない。鋸波キャリアは元々 -6dB/oct の傾斜を持ち、
    //  出力のデエンファシスでさらに -6dB/oct が乗るため、旧実装は合計 -12dB/oct 余計に暗く、
    //  母音の 5-8kHz が -27.9dB、無声音では低域 +10.2dB / 5-8kHz -13.0dB とバランスが崩れていた
    //  (実測。修正後は母音 -4.3dB / 無声音 ±2dB 以内)。
    //  ここで分析と同じ (1 - kPreemph·z⁻¹) を掛けて白色化する。
    const float wcL = carrierL - kPreemph * mPreCarL; mPreCarL = carrierL;
    const float wcR = carrierR - kPreemph * mPreCarR; mPreCarR = carrierR;

    // 5-b.【修正F】無声音の雑音励起。
    //  歯擦音・息を周期波で鳴らすとピッチのついたブザーになる
    //  (周期性 実測 0.21(原音) → 0.35(旧実装))。voicing に応じて白色雑音へクロスフェードする。
    //  クロスフェードはパワー保存 (√v / √(1-v))。voicing は 5ms で平滑化しパチつきを防ぐ。
    float excL = wcL, excR = wcR;
    if (mUnvoicedAuto > 0.0001f)
    {
        const float vTarget = 1.0f - mUnvoicedAuto * (1.0f - std::min(1.0f, std::max(0.0f, voicing)));
        mVoicingSm += (vTarget - mVoicingSm) * mVoicingCoef;

        // 雑音レベルは白色化後キャリアの追従RMSに合わせる (切替時の音量段差を防ぐ)
        const float mag = 0.5f * (std::abs(wcL) + std::abs(wcR));
        mCarRms += (mag * 1.4142136f - mCarRms) * mRmsCoef;

        const float gv = std::sqrt(mVoicingSm);
        const float gu = std::sqrt(1.0f - mVoicingSm) * mCarRms;
        excL = gv * wcL + gu * nextNoise();
        excR = gv * wcR + gu * nextNoise();
    }

    // 5-c. ラティス合成 + デエンファシス 1/(1 - kPreemph·z^-1)
    const float g = mGCur * kMakeupGain;
    const float yL = mLatticeL.processSample(g * excL, mKCur.data(), order);
    const float yR = mLatticeR.processSample(g * excR, mKCur.data(), order);
    mDeempL = yL + kPreemph * mDeempL;
    mDeempR = yR + kPreemph * mDeempR;

    // 6. DCブロッカー (fc=20Hz)。
    //    デエンファシスはDCで16倍(+24dB)効くため、キャリア側の僅かなDCオフセットが
    //    そのまま増幅されて低域が膨らみ、ラティスを飽和させることがある。
    //    可聴域外の20Hz以下だけを落とすので音色への影響は無い。
    const float dcOutL = mDeempL - mDcX1L + kDcR * mDcY1L;
    mDcX1L = mDeempL; mDcY1L = dcOutL;
    const float dcOutR = mDeempR - mDcX1R + kDcR * mDcY1R;
    mDcX1R = mDeempR; mDcY1R = dcOutR;

    // 【修正C】ソフトセーフティ制限 (Soft Safety Limiter)
    //  ±0.85f (-1.4dBFS) 以下の通常振幅に対しては完全リニア(歪みゼロ)。
    //  デエンファシス過渡応答等の突発大振幅(0.85f超)のみを tanh で滑らかに収束させ、
    //  出力上限を絶対に ±1.0f (0.0 dBFS) 以下に抑え込む。
    auto softLimit = [](float x) noexcept -> float
    {
        constexpr float thresh = 0.85f;
        constexpr float headroom = 0.15f;
        if (x > thresh)       return thresh + headroom * std::tanh((x - thresh) / headroom);
        else if (x < -thresh) return -thresh + headroom * std::tanh((x + thresh) / headroom);
        return x;
    };

    outL = softLimit(dcOutL);
    outR = softLimit(dcOutR);
}
