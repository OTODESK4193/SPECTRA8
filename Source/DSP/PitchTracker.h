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
    static constexpr float kMaxHz = 1000.0f;

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
            setVoiced(false);
            return;
        }

        // --- NSDF ---
        const int tauMin = (int)((float)kAnalysisRate / kMaxHz);          // ≈16
        const int tauMax = juce::jmin(kWindowSize / 2, (int)((float)kAnalysisRate / kMinHz)); // ≈291→256

        float maxNsdf = 0.0f;
        int bestTau = 0;
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

        // ピークピッキング（最初の主要ピーク優先: k=0.9）
        for (int tau = tauMin + 1; tau < tauMax; ++tau)
            if (nsdf[(size_t)tau] > maxNsdf) maxNsdf = nsdf[(size_t)tau];

        const float threshold = 0.9f * maxNsdf;
        for (int tau = tauMin + 1; tau < tauMax; ++tau)
        {
            if (nsdf[(size_t)tau] > nsdf[(size_t)tau - 1]
             && nsdf[(size_t)tau] >= nsdf[(size_t)tau + 1]
             && nsdf[(size_t)tau] >= threshold)
            {
                bestTau = tau;
                break;
            }
        }

        if (bestTau <= 0)
        {
            setVoiced(false);
            return;
        }

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
        const bool wantVoiced = voiced ? (voicedScore > 0.40f)   // 維持しきい値（低）
                                       : (voicedScore > 0.55f);  // 開始しきい値（高）
        setVoiced(wantVoiced && hz >= kMinHz && hz <= kMaxHz);

        if (voiced)
        {
            // オクターブエラー（ダブルピッチ / ハーフピッチ）の自動補正
            // 参照は前回の生ピッチ (遅れの大きい smoothedHz を使うと
            // 有声開始直後に誤補正→スウープの原因になる)
            float correctedHz = hz;
            if (pitchHz > 30.0f)
            {
                const float rVal = hz / pitchHz;
                // 1オクターブ上の誤検出を元のオクターブに引き戻す
                if (rVal >= 1.8f && rVal <= 2.2f)
                {
                    correctedHz = hz * 0.5f;
                }
                // 1オクターブ下の誤検出を元のオクターブに引き上げる
                else if (rVal >= 0.45f && rVal <= 0.55f)
                {
                    correctedHz = hz * 2.0f;
                }
            }

            // 生ピッチ・平滑化ピッチの両方に補正値を適用
            pitchHz = correctedHz;
            smoothedHz += 0.5f * (pitchHz - smoothedHz);
            smoothedHz = juce::jlimit(kMinHz, kMaxHz, smoothedHz);
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
    bool voiced = false;
    int voicedHold = 0;
};
