// ==========================================
// File: PitchTracker.h
// McLeod Pitch Method (MPM) ピッチ検出 & V/UV判定（開発計画書 第2版 ③準拠）
//
//  - ホストSRの入力を内部で16kHz相当にデシメーションして解析
//  - NSDF (正規化自乗差関数) + 放物線補間
//  - Clarity / ZCR / 高域エネルギー比を統合した有声判定
//  - シュミットトリガー遷移によるチャタリング防止
//  すべてスカラー処理・事前確保バッファのみ（リアルタイム安全）
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class PitchTracker
{
public:
    static constexpr int kAnalysisRate = 16000;
    static constexpr int kWindowSize = 512;   // 32ms @16kHz
    static constexpr int kHopSize = 128;      // 8ms (旧16ms。スピーチ抑揚への追従を倍化)
    static constexpr float kMinHz = 55.0f;
    // 検出上限。旧1000Hzはフォルマント周期(700〜1000Hz帯)を基音として拾う余地があり、
    // スピーチで突然ピッチが跳ね上がる一因だった。人声の基音域(〜600Hz)に制限。
    static constexpr float kMaxHz = 600.0f;

    void prepare(double hostSampleRate)
    {
        hostSr = juce::jmax(8000.0, hostSampleRate);
        decimRatio = hostSr / (double)kAnalysisRate;
        decimAccum = 0.0;
        writePos = 0;
        filled = 0;
        hopCounter = 0;
        buffer.fill(0.0f);

        // デシメーション前のアンチエイリアスLP (2段バイクアッド, fc≈6.8kHz@ホストSR)
        const double fc = 6800.0;
        for (auto& s : lpState) s = {};
        computeLowpass(fc);

        pitchHz = 130.0f;
        smoothedHz = 130.0f;
        clarity = 0.0f;
        voiced = false;
        unvoicedHfRatio = 0.0f;
        voicedAmount = 0.0f;
        wasVoicedPrev = false;
        rawHist[0] = rawHist[1] = rawHist[2] = 130.0f;
    }

    // ホストレートのモノラル入力を1サンプル供給
    void pushSample(float x) noexcept
    {
        // アンチエイリアスLP
        for (int st = 0; st < 2; ++st)
        {
            auto& s = lpState[(size_t)st];
            const float y = lpB0 * x + s.z1;
            s.z1 = lpB1 * x - lpA1 * y + s.z2;
            s.z2 = lpB2 * x - lpA2 * y;
            x = y;
        }

        // 16kHzグリッドへのデシメーション（サンプルホールドで十分）
        decimAccum += 1.0;
        if (decimAccum >= decimRatio)
        {
            decimAccum -= decimRatio;

            buffer[(size_t)writePos] = x;
            writePos = (writePos + 1) % kWindowSize;
            if (filled < kWindowSize) ++filled;

            if (++hopCounter >= kHopSize && filled >= kWindowSize)
            {
                hopCounter = 0;
                analyze();
            }
        }
    }

    float getPitchHz() const noexcept { return smoothedHz; }
    float getRawPitchHz() const noexcept { return pitchHz; }
    bool  isVoiced() const noexcept { return voiced; }
    float getClarity() const noexcept { return clarity; }
    float getUnvoicedHfRatio() const noexcept { return unvoicedHfRatio; }

    // 有声らしさの連続値 (0=完全に無声/雑音的, 1=完全に有声)。
    //  isVoiced() が使う voicedScore (clarity - 0.5·zcr - 0.4·hf比) を、
    //  同関数のシュミットトリガー閾値 0.40/0.55 を跨ぐ帯 0.35〜0.60 で 0→1 に正規化したもの。
    //  LPCの有声/無声自動励起切替 (LpcVocoder::processSample の voicing 引数) に使う。
    float getVoicedAmount() const noexcept { return voicedAmount; }

private:
    void computeLowpass(double fc)
    {
        const double w0 = juce::MathConstants<double>::twoPi * fc / hostSr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double q = 0.7071;
        const double alpha = sw / (2.0 * q);
        const double a0 = 1.0 + alpha;
        lpB0 = (float)(((1.0 - cw) * 0.5) / a0);
        lpB1 = (float)((1.0 - cw) / a0);
        lpB2 = lpB0;
        lpA1 = (float)((-2.0 * cw) / a0);
        lpA2 = (float)((1.0 - alpha) / a0);
    }

    void analyze() noexcept
    {
        // 窓を時系列順に展開
        std::array<float, kWindowSize> x;
        for (int n = 0; n < kWindowSize; ++n)
            x[(size_t)n] = buffer[(size_t)((writePos + n) % kWindowSize)];

        // エネルギー & ZCR & 高域比
        float energy = 0.0f;
        int zc = 0;
        float hfEnergy = 0.0f;
        for (int n = 0; n < kWindowSize; ++n)
        {
            const float v = x[(size_t)n];
            energy += v * v;
            if (n > 0)
            {
                if ((x[(size_t)n - 1] < 0.0f) != (v < 0.0f)) ++zc;
                const float d = v - x[(size_t)n - 1];  // 1次差分 ≒ 高域強調
                hfEnergy += d * d;
            }
        }

        const float rms = std::sqrt(energy / (float)kWindowSize);
        unvoicedHfRatio = (energy > 1e-9f) ? juce::jlimit(0.0f, 1.0f, hfEnergy / (energy * 2.0f)) : 0.0f;
        const float zcr = (float)zc / (float)kWindowSize;

        if (rms < 1e-4f)  // 無音
        {
            voicedAmount *= 0.5f;   // 無音では徐々に無声側へ
            setVoiced(false);
            if (!voiced) wasVoicedPrev = false;
            return;
        }

        // --- NSDF ---
        const int tauMin = (int)((float)kAnalysisRate / kMaxHz);          // ≈16
        const int tauMax = juce::jmin(kWindowSize / 2, (int)((float)kAnalysisRate / kMinHz)); // ≈291→256

        std::array<float, kWindowSize / 2 + 1> nsdf {};

        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            float acf = 0.0f, m = 0.0f;
            const int lim = kWindowSize - tau;
            for (int n = 0; n < lim; ++n)
            {
                const float a = x[(size_t)n];
                const float b = x[(size_t)(n + tau)];
                acf += a * b;
                m += a * a + b * b;
            }
            nsdf[(size_t)tau] = (m > 1e-9f) ? (2.0f * acf / m) : 0.0f;
        }

        // --- ピークピッキング (MPM論文準拠) ---
        // 旧実装は τ の小さい側から「最初の局所最大 >= 0.9*max」を採っていたが、
        // MPM法が要求する「最初の負方向ゼロ交差の後から探索」の条件が無く、
        // τ が小さい正領域で倍音/フォルマント周期の擬似ピークを拾いやすかった
        // (→ スピーチで突然ピッチが1〜3倍へ跳ね上がる主因)。
        // ゼロ交差条件を追加し、真の周期ピークのみを候補にする。
        int zcTau = tauMin;
        while (zcTau <= tauMax && nsdf[(size_t)zcTau] > 0.0f) ++zcTau;   // 正領域をスキップ

        float maxNsdf = 0.0f;
        for (int tau = zcTau; tau <= tauMax; ++tau)
            maxNsdf = juce::jmax(maxNsdf, nsdf[(size_t)tau]);

        int bestTau = 0;
        if (zcTau <= tauMax && maxNsdf > 0.0f)
        {
            // k=0.93: 男声などH2(第2倍音)が強い声では半周期(=1オクターブ上)の
            // ピークが0.9×maxを超えて先に拾われやすい。閾値を上げ、
            // グローバル最大(通常は真の基本周期)寄りの選択にする。
            const float threshold = 0.93f * maxNsdf;
            const int start = juce::jmax(zcTau, tauMin) + 1;
            for (int tau = start; tau < tauMax; ++tau)
            {
                if (nsdf[(size_t)tau] > nsdf[(size_t)tau - 1]
                 && nsdf[(size_t)tau] >= nsdf[(size_t)tau + 1]
                 && nsdf[(size_t)tau] >= threshold)
                {
                    bestTau = tau;
                    break;
                }
            }
        }

        if (bestTau <= 0)
        {
            voicedAmount = 0.0f;    // 周期ピーク無し = 明確に無声(雑音)
            setVoiced(false);
            if (!voiced) wasVoicedPrev = false;
            return;
        }

        // ※「2倍周期にもピークがあればそちらを採る」式のサブハーモニック確認は
        //   行わない。周期Tの信号は2Tでも必ず高いNSDFピークを持つため、
        //   その方式は常にオクターブ下へ倒れる誤りになる。
        //   倍音誤検出はゼロ交差条件(上記)と3フレームメディアン(下記)で対処する。

        // 放物線補間
        const float y1 = nsdf[(size_t)(bestTau - 1)];
        const float y2 = nsdf[(size_t)bestTau];
        const float y3 = nsdf[(size_t)(bestTau + 1)];
        const float denom = (y1 - 2.0f * y2 + y3);
        float tauF = (float)bestTau;
        if (std::abs(denom) > 1e-9f)
            tauF += 0.5f * (y1 - y3) / denom;

        clarity = y2;
        const float hz = (float)kAnalysisRate / juce::jmax(1.0f, tauF);

        // --- 多角的V/UV判定 + シュミットトリガー ---
        //   有声: clarity高 & ZCR低 & 高域比低
        const float voicedScore = clarity - 0.5f * zcr - 0.4f * unvoicedHfRatio;
        // シュミットトリガー閾値(0.40/0.55)を跨ぐ帯で 0→1 に正規化した連続版。
        // 無声励起の自動切替はこの連続値でクロスフェードするのでパチつかない。
        voicedAmount = juce::jlimit(0.0f, 1.0f, (voicedScore - 0.35f) / 0.25f);
        const bool wantVoiced = voiced ? (voicedScore > 0.40f)   // 維持しきい値（低）
                                       : (voicedScore > 0.55f);  // 開始しきい値（高）
        setVoiced(wantVoiced && hz >= kMinHz && hz <= kMaxHz);

        // ピッチ値の更新は「今フレームが確実に有声」(wantVoiced) かつ
        // 明瞭度が十分なときのみ行う。ハングオーバー中(子音・語尾の余韻)の
        // 低品質なNSDFピークからの更新は跳びの原因になるため凍結し、
        // 直前の有声ピッチを保持する。
        if (voiced && wantVoiced && clarity > 0.5f)
        {
            // オクターブエラー（ダブルピッチ / ハーフピッチ）の証拠ベース自動補正。
            // 旧実装は「前回比≈2倍なら無条件に半分へ」だったため、一度誤オクターブに
            // 入ると正しい検出まで引き戻し続ける双安定ラッチになっていた
            // (高いピッチに張り付き→時々窓を外れて元に戻る症状の原因)。
            // 補正は、補正先の周期にNSDF上の強い裏付け(≥0.95×現ピーク)がある
            // 場合のみ適用する。裏付けが無ければ現フレームの検出を信じる。
            float correctedHz = hz;
            if (pitchHz > 30.0f && wasVoicedPrev)
            {
                const float rVal = hz / pitchHz;
                auto peakSupport = [&](int tc) -> float
                {
                    float s = 0.0f;
                    for (int t = juce::jmax(tauMin, tc - 2); t <= juce::jmin(tauMax, tc + 2); ++t)
                        s = juce::jmax(s, nsdf[(size_t)t]);
                    return s;
                };
                // 1オクターブ上の誤検出 → 2倍周期側に裏付けがあれば引き戻す
                if (rVal >= 1.8f && rVal <= 2.2f && 2 * bestTau <= tauMax)
                {
                    if (peakSupport(2 * bestTau) >= 0.95f * nsdf[(size_t)bestTau])
                        correctedHz = hz * 0.5f;
                }
                // 1オクターブ下の誤検出 → 半分周期側に裏付けがあれば引き上げる
                else if (rVal >= 0.45f && rVal <= 0.55f && bestTau / 2 >= tauMin)
                {
                    if (peakSupport(bestTau / 2) >= 0.95f * nsdf[(size_t)bestTau])
                        correctedHz = hz * 2.0f;
                }
            }

            // 3フレーム(24ms)メディアンで単発スパイク(1フレームの誤検出)を除去。
            // 高速な応答(Fast)設定でも単発の跳びが出力に到達しなくなる。
            if (!wasVoicedPrev)
            {
                // 有声開始フレーム: 前フレーズの古い履歴に引っ張られないよう初期化
                rawHist[0] = rawHist[1] = rawHist[2] = correctedHz;
            }
            else
            {
                rawHist[2] = rawHist[1];
                rawHist[1] = rawHist[0];
                rawHist[0] = correctedHz;
            }
            const float a = rawHist[0], b = rawHist[1], c = rawHist[2];
            const float med = juce::jmax(juce::jmin(a, b), juce::jmin(juce::jmax(a, b), c));

            pitchHz = med;
            smoothedHz += 0.5f * (pitchHz - smoothedHz);
            smoothedHz = juce::jlimit(kMinHz, kMaxHz, smoothedHz);
            wasVoicedPrev = true;
        }
        else
        {
            wasVoicedPrev = false;
        }
    }

    void setVoiced(bool v) noexcept
    {
        if (v) { voicedHold = 3; voiced = true; }
        else if (voicedHold > 0) { --voicedHold; }   // 3ホップ(≈48ms)のハングオーバー
        else { voiced = false; }
    }

    struct BiquadState { float z1 = 0.0f, z2 = 0.0f; };

    double hostSr = 44100.0;
    double decimRatio = 2.75625;
    double decimAccum = 0.0;

    float lpB0 = 0, lpB1 = 0, lpB2 = 0, lpA1 = 0, lpA2 = 0;
    std::array<BiquadState, 2> lpState;

    std::array<float, kWindowSize> buffer {};
    int writePos = 0;
    int filled = 0;
    int hopCounter = 0;

    float pitchHz = 130.0f;
    float smoothedHz = 130.0f;
    float clarity = 0.0f;
    float unvoicedHfRatio = 0.0f;
    float voicedAmount = 0.0f;   // 有声らしさの連続値 (getVoicedAmount)
    bool voiced = false;
    int voicedHold = 0;

    // 3フレームメディアン用の生ピッチ履歴と有声継続フラグ
    float rawHist[3] = { 130.0f, 130.0f, 130.0f };
    bool wasVoicedPrev = false;
};
