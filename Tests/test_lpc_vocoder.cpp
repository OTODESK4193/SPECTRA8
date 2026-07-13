// ==========================================
// File: test_lpc_vocoder.cpp
// M2 統合スタブテスト: LpcVocoder 全チェーン（分析→補間→ラティス合成）
//
//  T1: 合成母音/a/ + ノコギリキャリア → 出力有限・無音でない・RMS計測（レベル較正）
//  T2: 出力の再分析でフォルマント保存を確認（±8%: 補間・平滑化込みの実効値）
//  T3: 極端パラメータ連打（次数切替/フリーズ/ノイズ入力連打）で NaN/爆音ゼロ
//  T4: 無音入力 → 出力が減衰して無音化（ゲート挙動）
//
// ビルド:
//   g++ -O2 -std=c++17 -I../Source/DSP test_lpc_vocoder.cpp \
//       ../Source/DSP/LpcAnalyzer.cpp ../Source/DSP/LpcVocoder.cpp -o test_voc
// ==========================================
#include "LpcVocoder.h"
#include "LpcAnalyzer.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include <complex>

static constexpr int FS = 16000;

// 合成母音 /a/ (F1=730, F2=1090, F3=2440Hz)
static std::vector<float> synthVowelA(int n, double f0 = 100.0)
{
    std::vector<double> exc(n, 0.0);
    for (double t = 0.0; t < n; t += FS / f0)
        exc[(size_t)t] = 1.0;
    std::vector<double> y = exc;
    const double fbw[3][2] = { {730, 90}, {1090, 110}, {2440, 170} };
    for (auto& fb : fbw)
    {
        const double r = std::exp(-M_PI * fb[1] / FS);
        const double th = 2.0 * M_PI * fb[0] / FS;
        const double a1 = -2.0 * r * std::cos(th), a2 = r * r;
        std::vector<double> o(n, 0.0);
        for (int i = 0; i < n; ++i)
            o[(size_t)i] = y[(size_t)i]
                         - a1 * (i >= 1 ? o[(size_t)(i - 1)] : 0.0)
                         - a2 * (i >= 2 ? o[(size_t)(i - 2)] : 0.0);
        y = o;
    }
    double peak = 1e-12;
    for (double v : y) peak = std::max(peak, std::fabs(v));
    std::vector<float> out(n);
    for (int i = 0; i < n; ++i) out[(size_t)i] = (float)(y[(size_t)i] / peak);
    return out;
}

// A(z) 多項式をk係数から再構成 → Durand-Kerner法で根 → フォルマント抽出（検証専用）
static std::vector<double> formantsFromK(const float* k, int order)
{
    // step-up: k -> a
    std::vector<double> a(order + 1, 0.0), an(order + 1, 0.0);
    for (int i = 1; i <= order; ++i)
    {
        const double ki = k[i - 1];
        for (int j = 1; j < i; ++j) an[(size_t)j] = a[(size_t)j] + ki * a[(size_t)(i - j)];
        an[(size_t)i] = ki;
        for (int j = 1; j <= i; ++j) a[(size_t)j] = an[(size_t)j];
    }
    // 多項式 P(z) = z^N + a1 z^(N-1) + ... + aN （z^-1系のA(z)と根は共役同値）
    const int N = order;
    std::vector<std::complex<double>> c(N + 1);
    c[0] = 1.0;
    for (int i = 1; i <= N; ++i) c[(size_t)i] = a[(size_t)i];
    // Durand-Kerner
    std::vector<std::complex<double>> roots(N);
    for (int i = 0; i < N; ++i)
        roots[(size_t)i] = std::polar(0.7, 2.0 * M_PI * (i + 0.25) / N);
    for (int iter = 0; iter < 300; ++iter)
    {
        double delta = 0.0;
        for (int i = 0; i < N; ++i)
        {
            std::complex<double> num = 0.0;
            {   // evaluate poly at roots[i] (Horner)
                std::complex<double> v = c[0];
                for (int j = 1; j <= N; ++j) v = v * roots[(size_t)i] + c[(size_t)j];
                num = v;
            }
            std::complex<double> den = 1.0;
            for (int j = 0; j < N; ++j)
                if (j != i) den *= (roots[(size_t)i] - roots[(size_t)j]);
            const auto step = num / den;
            roots[(size_t)i] -= step;
            delta = std::max(delta, std::abs(step));
        }
        if (delta < 1e-12) break;
    }
    std::vector<double> f;
    for (auto& r : roots)
    {
        if (r.imag() > 0.01)
        {
            const double freq = std::arg(r) * FS / (2.0 * M_PI);
            const double bw = -std::log(std::abs(r)) * FS / M_PI;
            if (bw < 700.0 && freq > 90.0 && freq < FS / 2 - 200.0)
                f.push_back(freq);
        }
    }
    std::sort(f.begin(), f.end());
    return f;
}

static double rms(const std::vector<float>& v, size_t from = 0)
{
    double s = 0.0; size_t n = 0;
    for (size_t i = from; i < v.size(); ++i) { s += (double)v[i] * v[i]; ++n; }
    return std::sqrt(s / (double)std::max<size_t>(1, n));
}

int main()
{
    int fails = 0;

    // ===== T1: 母音 + ノコギリキャリア =====
    const int n = FS * 2;
    auto vowel = synthVowelA(n);
    LpcVocoder voc;
    voc.prepare(44100.0);
    voc.setWindowType(0);

    std::vector<float> outL(n), outR(n);
    double sawPhase = 0.0;
    const double sawInc = 110.0 / FS;
    bool finite = true;
    for (int i = 0; i < n; ++i)
    {
        sawPhase += sawInc; if (sawPhase >= 1.0) sawPhase -= 1.0;
        const float saw = (float)(2.0 * sawPhase - 1.0);
        float l = 0, r = 0;
        voc.processSample(vowel[(size_t)i], saw, saw, l, r, 16, false);
        outL[(size_t)i] = l; outR[(size_t)i] = r;
        if (!std::isfinite(l) || !std::isfinite(r)) finite = false;
    }
    const double inRms = rms(vowel, FS / 2);
    const double outRms = rms(outL, FS / 2);
    const double ratioDb = 20.0 * std::log10(outRms / std::max(1e-12, inRms));
    const bool t1 = finite && outRms > 1e-4;
    std::printf("[T1] 有限=%d, inRMS=%.4f outRMS=%.4f 比=%.1fdB  -> %s\n",
                (int)finite, inRms, outRms, ratioDb, t1 ? "PASS" : "FAIL");
    if (!t1) ++fails;

    // ===== T2: 出力を再分析してフォルマント保存確認 =====
    // ノコギリ波の -6dB/oct スペクトル傾斜がフォルマント根抽出を妨げるため、
    // ソース・フィルタモデルに忠実なパルス列キャリアで再レンダリングして評価する
    voc.reset();
    {
        double ph = 0.0;
        for (int i = 0; i < n; ++i)
        {
            ph += 110.0 / FS;
            float imp = 0.0f;
            if (ph >= 1.0) { ph -= 1.0; imp = 1.0f; }
            float l = 0, r = 0;
            voc.processSample(vowel[(size_t)i], imp, imp, l, r, 16, false);
            outL[(size_t)i] = l;
        }
    }
    LpcAnalyzer ana;
    ana.prepare();
    const double targets[3] = { 730.0, 1090.0, 2440.0 };
    int okFrames = 0, totFrames = 0;
    double avgErr[3] = { 0, 0, 0 };
    for (int start = FS / 2; start + 256 < n; start += 320 * 4)
    {
        float kc[16];
        ana.analyzeFrame(&outL[(size_t)start], 16, kc);
        auto f = formantsFromK(kc, 16);
        if (f.size() >= 3)
        {
            ++totFrames;
            bool ok = true;
            for (int t = 0; t < 3; ++t)
            {
                double best = 1e9;
                for (double ff : f) best = std::min(best, std::fabs(ff - targets[t]));
                const double errPct = best / targets[t] * 100.0;
                avgErr[t] += errPct;
                if (errPct > 8.0) ok = false;
            }
            if (ok) ++okFrames;
        }
    }
    for (double& e : avgErr) e /= std::max(1, totFrames);
    const bool t2 = totFrames > 0 && okFrames >= (totFrames * 7) / 10;
    std::printf("[T2] フォルマント保存: %d/%d フレームOK, 平均誤差%% F1=%.1f F2=%.1f F3=%.1f  -> %s\n",
                okFrames, totFrames, avgErr[0], avgErr[1], avgErr[2], t2 ? "PASS" : "FAIL");
    if (!t2) ++fails;

    // ===== T3: 極端パラメータ連打ストレス =====
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> uni(-1.0f, 1.0f);
    voc.reset();
    bool stressOk = true;
    float peak = 0.0f;
    const int orders[4] = { 8, 10, 12, 16 };
    for (int i = 0; i < FS * 30; ++i)
    {
        const float mod = (i / 1600) % 3 == 0 ? uni(rng) : vowel[(size_t)(i % n)] * (1.0f + 0.5f * uni(rng));
        const float car = uni(rng) * 2.0f;                    // 過大キャリア
        const int order = orders[(i / 777) % 4];              // 次数を高速切替
        const bool freeze = ((i / 1234) % 2) == 1;            // フリーズ連打
        if ((i % 5000) == 0) voc.setWindowType((i / 5000) % 3); // 窓切替
        float l = 0, r = 0;
        voc.processSample(mod, car, car, l, r, order, freeze);
        if (!std::isfinite(l) || !std::isfinite(r)) { stressOk = false; break; }
        peak = std::max(peak, std::fabs(l));
    }
    const bool t3 = stressOk && peak <= LpcLattice::kStateSat + 1e-3f;
    std::printf("[T3] ストレス30s: NaN/Inf=%s, ピーク=%.2f (上限%.1f)  -> %s\n",
                stressOk ? "なし" : "あり", peak, (double)LpcLattice::kStateSat, t3 ? "PASS" : "FAIL");
    if (!t3) ++fails;

    // ===== T4: 無音入力でゲート =====
    voc.reset();
    voc.setWindowType(0);
    double tailRms = 0.0; int cnt = 0;
    for (int i = 0; i < FS; ++i)
    {
        sawPhase += sawInc; if (sawPhase >= 1.0) sawPhase -= 1.0;
        const float saw = (float)(2.0 * sawPhase - 1.0);
        float l = 0, r = 0;
        voc.processSample(0.0f, saw, saw, l, r, 16, false);
        if (i > FS / 2) { tailRms += (double)l * l; ++cnt; }
    }
    tailRms = std::sqrt(tailRms / std::max(1, cnt));
    const bool t4 = tailRms < 1e-5;
    std::printf("[T4] 無音入力の残留RMS=%.2e  -> %s\n", tailRms, t4 ? "PASS" : "FAIL");
    if (!t4) ++fails;

    std::printf(fails == 0 ? "M2 STUB TEST: PASS\n" : "M2 STUB TEST: FAIL (%d)\n", fails);
    return fails == 0 ? 0 : 1;
}
