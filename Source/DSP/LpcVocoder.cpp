// ==========================================
// File: LpcVocoder.cpp
// フェーズ2計画書v2 §4/§5 準拠
// ==========================================
#include "LpcVocoder.h"
#include <cmath>
#include <algorithm>

void LpcVocoder::prepare(double /*hostSampleRate*/)
{
    mAnalyzer.prepare();

    // ゲイン平滑化係数（コントロールブロック = 32smp @16kHz = 2ms）
    const double blockDur = (double)kCtrlBlock / kInternalSampleRate;
    mGAttCoef = (float)std::exp(-blockDur / 0.005);   // att 5ms
    mGRelCoef = (float)std::exp(-blockDur / 0.030);   // rel 30ms

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
                               float formantShiftSemitones, float formantStretch) noexcept
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

            // プリエンファシス (1 - kPreemph·z^-1) を窓に適用してから分析。
            // 高域を持ち上げてLPCの極を高次フォルマントにも配分させる。
            // 出力側のデエンファシスと対で周波数特性は復元される。
            // frame[0]は前サンプル不明のためそのまま(窓端はほぼ0のため影響なし)。
            for (int n = LpcAnalyzer::kWindowSize - 1; n >= 1; --n)
                mFrame[(size_t)n] -= kPreemph * mFrame[(size_t)(n - 1)];

            const float g = mAnalyzer.analyzeFrame(mFrame.data(), order, mKTarget.data(), (double)gamma);
            mGTarget = g * mExcNorm;   // 励起レベル正規化（per-sample残差RMS相当へ）

            // M4改: FMT STRETCH（LSF領域のVTLN周波数ワーピング）。
            //  FilterBankモード(合成帯域 f×stretch)と同じ方向感(上げ=明るく)に統一。
            //  旧実装(π/2中心の間隔伸縮)は (a) 方向感がFBと逆、(b) クランプ端でLSFが
            //  最小間隔1e-3radまで密集して超高Q共振が発生、(c) ゲイン補償が無く
            //  +30dB超の爆音→ラティス飽和で「ブツブツ」ノイズになっていた。
            //  VTLNワープ + 段階的帯域幅拡張(max|H|制限) + 励起重み付きゲイン補償で解消。
            if (std::abs(formantStretch - 1.0f) > 0.01f)
            {
                double aS[LpcAnalyzer::kMaxOrder + 1];
                double lsf[LpcAnalyzer::kMaxOrder];
                LspConverter::reflToLpc(mKTarget.data(), order, aS);
                if (LspConverter::lpcToLsf(aS, order, lsf))
                {
                    // オールパス周波数ワーピング(VTLN): w' = w + 2*atan(rho*sin(w)/(1-rho*cos(w)))
                    // [0,π]→[0,π] の単調写像で、低域の傾き (1+rho)/(1-rho) = stretch。
                    // 端点が固定されるためクランプ密集による超高Q共振が構造的に起きない。
                    const double rho = ((double)formantStretch - 1.0) / ((double)formantStretch + 1.0);
                    constexpr double kGap = 0.02;              // 最小間隔 ≈51Hz@16k
                    constexpr double kLo = 0.02, kHi = 3.1215926;
                    for (int i = 0; i < order; ++i)
                    {
                        const double w = lsf[i];
                        const double wp = w + 2.0 * std::atan(rho * std::sin(w) / (1.0 - rho * std::cos(w)));
                        lsf[i] = std::min(kHi, std::max(kLo, wp));
                    }
                    for (int i = 1; i < order; ++i)
                        if (lsf[i] < lsf[i - 1] + kGap) lsf[i] = lsf[i - 1] + kGap;

                    double a2[LpcAnalyzer::kMaxOrder + 1];
                    LspConverter::lsfToLpc(lsf, order, a2);

                    // フィルタ評価: 対数グリッド64点で max|H|² と励起重み付き(1/f²)エネルギー
                    auto evalFilter = [order](const double* aa, double& maxH2, double& wE)
                    {
                        maxH2 = 0.0; wE = 0.0;
                        for (int m = 0; m < 64; ++m)
                        {
                            const double f = 60.0 * std::pow(7600.0 / 60.0, (double)m / 63.0);
                            const double w = 6.283185307179586 * f / kInternalSampleRate;
                            double re = 1.0, im = 0.0;
                            for (int i = 1; i <= order; ++i)
                            {
                                re += aa[i] * std::cos(w * (double)i);
                                im -= aa[i] * std::sin(w * (double)i);
                            }
                            // デエンファシス 1/(1-a z^-1) の利得も含めた
                            // 「実際に聴こえる」応答で評価する
                            const double dr = 1.0 - (double)kPreemph * std::cos(w);
                            const double di = (double)kPreemph * std::sin(w);
                            const double d2 = 1.0 / std::max(1e-12, dr * dr + di * di);
                            const double h2 = d2 / std::max(1e-18, re * re + im * im);
                            wE += (1.0 / (f * f)) * h2;
                            if (h2 > maxH2) maxH2 = h2;
                        }
                    };
                    double mh0, e0;
                    evalFilter(aS, mh0, e0);

                    // 段階的帯域幅拡張: 共振ピークが「元+12dB」以内に収まるγを探す。
                    // 圧縮方向(stretch<1)では基音付近の極がDC方向へ密集し爆音・変換失敗が
                    // 起きるため、成功しかつピークが安全域のγを段階的に採用する。
                    static const double kGammas[6] = { 0.995, 0.98, 0.955, 0.92, 0.87, 0.80 };
                    float kS[LpcAnalyzer::kMaxOrder];
                    for (int at = 0; at < 6; ++at)
                    {
                        double a3[LpcAnalyzer::kMaxOrder + 1];
                        double g = 1.0;
                        a3[0] = a2[0];
                        for (int i = 1; i <= order; ++i) { g *= kGammas[at]; a3[i] = a2[i] * g; }

                        if (!LspConverter::lpcToRefl(a3, order, kS))
                            continue;
                        double mh, e;
                        evalFilter(a3, mh, e);
                        if (mh <= mh0 * 15.85 || at == 5)   // +12dB (パワー比15.85) まで許容
                        {
                            // ゲイン補償: 励起重み付きエネルギー比で聴感レベルを維持しつつ、
                            // max|H|比によるピーク境界で「非ストレッチ時のピーク+3.5dB」を
                            // 超えないよう制限 (Limiter Off でもクリップしない)
                            if (e > 1e-18 && e0 > 1e-18 && mh > 1e-18)
                            {
                                const double eComp   = std::sqrt(e0 / e);
                                const double pkBound = 1.5 * std::sqrt(mh0 / mh);
                                mGTarget *= (float)std::min(20.0, std::max(0.01, std::min(eComp, pkBound)));
                            }
                            for (int p = 0; p < order; ++p)
                                mKTarget[(size_t)p] = kS[p];
                            break;
                        }
                    }
                }
            }

            // M5: 反射係数kのビット量子化（BitSpeek風レトロ）。
            //  LAR(対数面積比 atanh(k))領域の対称量子化 Q=2^(bits-1)-1。
            //  kドメイン一様量子化は |k|→1 付近で極半径への感度が極端に高く、丸め上げで
            //  共振が急増しクリップの原因になるため、感度が均等なLAR領域で丸める
            //  (TMS5220系実機の非一様量子化テーブル相当)。
            //  さらに量子化によるフィルタ利得変化を G *= sqrt(Π(1-kq²)/Π(1-k²)) で補償し、
            //  低ビット時の音量暴れ・クリップを防ぐ(レトロな粗さは維持される)。
            if (mQuantBits >= 2)
            {
                const double Q = (double)((1 << (mQuantBits - 1)) - 1);
                constexpr double kLarMax = 2.994;   // atanh(0.995)
                double num = 1.0, den = 1.0;        // Π(1-kq²) / Π(1-k²)
                for (int p = 0; p < order; ++p)
                {
                    const double k0   = std::min(0.995, std::max(-0.995, (double)mKTarget[(size_t)p]));
                    const double lar  = std::atanh(k0);
                    const double larQ = std::round(lar * (Q / kLarMax)) * (kLarMax / Q);
                    const double kq   = std::min(0.995, std::max(-0.995, std::tanh(larQ)));
                    mKTarget[(size_t)p] = (float)kq;
                    num *= (1.0 - kq * kq);
                    den *= (1.0 - k0 * k0);
                }
                if (den > 1e-12)
                    mGTarget *= (float)std::sqrt(num / den);
            }

            // M4: フルホップ補間セグメントを構築(現在値→新ターゲットをホップ全長でモーフ)
            setupSegment(order);
        }
    }

    // 3. コントロールブロック毎に補間ランプを再計算
    if (mCtrlCounter == 0)
    {
        // ゲインの非対称平滑化（att 5ms / rel 30ms）
        const float coef = (mGTarget > mGSmooth) ? mGAttCoef : mGRelCoef;
        mGSmooth = mGTarget + (mGSmooth - mGTarget) * coef;

        constexpr float inv = 1.0f / (float)kCtrlBlock;
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
        }
        mGInc = (mGSmooth - mGCur) * inv;
    }
    if (++mCtrlCounter >= kCtrlBlock)
        mCtrlCounter = 0;

    // 4. サンプル毎の線形ランプ（|k|<1 は補間途中でも保存 → 常時安定）
    for (int p = 0; p < order; ++p)
        mKCur[(size_t)p] += mKInc[(size_t)p];
    mGCur += mGInc;

    // 5. ラティス合成 + デエンファシス 1/(1 - kPreemph·z^-1)
    const float g = mGCur * kMakeupGain;
    const float yL = mLatticeL.processSample(g * carrierL, mKCur.data(), order);
    const float yR = mLatticeR.processSample(g * carrierR, mKCur.data(), order);
    mDeempL = yL + kPreemph * mDeempL;
    mDeempR = yR + kPreemph * mDeempR;
    outL = mDeempL;
    outR = mDeempR;
}
