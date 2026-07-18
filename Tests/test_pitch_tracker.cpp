// ==========================================
// File: test_pitch_tracker.cpp
// PitchTracker スタブテスト (MPMピークピッキング修正の検証)
//
//  T1: 倍音リッチな模擬音声(強い第2倍音+800Hzフォルマント)で
//      急激な上方向ピッチジャンプ(旧バグ)が出ないこと
//  T2: ピッチグライド(110→190Hz)への追従
//  T3: ヴィブラート付き持続音の安定性
//
// ビルド (JUCEスタブ使用):
//   g++ -O2 -std=c++17 -I<stub_dir> -I../Source/DSP test_pitch_tracker.cpp -o test_pt
//   (stub_dir に jmax/jmin/jlimit/MathConstants を定義した JuceHeader.h を置く)
// ==========================================
#include "PitchTracker.h"
#include <cstdio>
#include <cmath>
#include <vector>

static constexpr double HOST_SR = 48000.0;

// 模擬音声: f0 + 強い倍音列 + フォルマント強調(800Hz付近の倍音を増幅)
static float speechLike(double t, double f0)
{
    double s = 0.0;
    for (int h = 1; h <= 12; ++h)
    {
        const double fh = f0 * h;
        if (fh > 7000.0) break;
        double amp = 1.0 / h;
        // 800Hz付近のフォルマント強調 (オクターブ誤検出を誘発しやすい条件)
        const double d = (fh - 800.0) / 250.0;
        amp *= 1.0 + 2.5 * std::exp(-d * d);
        // 第2倍音を意図的に強く (旧実装の2倍ピッチ誤検出の典型条件)
        if (h == 2) amp *= 1.8;
        s += amp * std::sin(6.283185307179586 * fh * t);
    }
    return (float)(s * 0.15);
}

int main()
{
    int failures = 0;

    // ---- T1: 一定ピッチ 120Hz、上方向ジャンプ検査 ----
    {
        PitchTracker pt;
        pt.prepare(HOST_SR);
        int jumps = 0, checks = 0;
        float prev = 0.0f;
        for (long i = 0; i < (long)(HOST_SR * 3.0); ++i)
        {
            const double t = i / HOST_SR;
            pt.pushSample(speechLike(t, 120.0));
            if (i > (long)(HOST_SR * 0.2) && pt.isVoiced())
            {
                const float hz = pt.getRawPitchHz();
                ++checks;
                if (prev > 0.0f && hz > prev * 1.6f) ++jumps; // 上方向ジャンプ(>+8半音/フレーム)
                if (hz < 90.0f || hz > 160.0f) ++jumps;       // 120Hz帯から逸脱
                prev = hz;
            }
        }
        printf("[T1] 120Hz固定: checks=%d 異常ジャンプ=%d -> %s\n",
               checks, jumps, (jumps == 0 && checks > 0) ? "PASS" : "FAIL");
        if (jumps != 0 || checks == 0) ++failures;
    }

    // ---- T2: グライド 110→190Hz (2秒) 追従 ----
    {
        PitchTracker pt;
        pt.prepare(HOST_SR);
        double phase = 0.0;
        int bad = 0, checks = 0;
        for (long i = 0; i < (long)(HOST_SR * 2.0); ++i)
        {
            const double t = i / HOST_SR;
            const double f0 = 110.0 + 40.0 * t;           // 110→190Hz
            phase += f0 / HOST_SR;
            // グライドは位相連続の必要があるため基本波+2倍音で簡易生成
            const float s = (float)(0.3 * std::sin(6.28318530718 * phase)
                                  + 0.15 * std::sin(2 * 6.28318530718 * phase));
            pt.pushSample(s);
            if (i > (long)(HOST_SR * 0.3) && pt.isVoiced())
            {
                ++checks;
                const float err = std::abs(pt.getRawPitchHz() - (float)f0) / (float)f0;
                if (err > 0.08f) ++bad;                    // ±8%超の誤差
            }
        }
        printf("[T2] グライド110→190Hz: checks=%d 誤差超過=%d -> %s\n",
               checks, bad, (checks > 0 && bad * 20 < checks) ? "PASS" : "FAIL");
        if (checks == 0 || bad * 20 >= checks) ++failures; // 5%まで許容
    }

    // ---- T3: ヴィブラート (130Hz ±3%, 5.5Hz) ----
    {
        PitchTracker pt;
        pt.prepare(HOST_SR);
        double phase = 0.0;
        int bad = 0, checks = 0;
        for (long i = 0; i < (long)(HOST_SR * 2.0); ++i)
        {
            const double t = i / HOST_SR;
            const double f0 = 130.0 * (1.0 + 0.03 * std::sin(6.28318530718 * 5.5 * t));
            phase += f0 / HOST_SR;
            pt.pushSample(speechLike(phase / f0 * f0, 1.0) * 0.0f
                          + (float)(0.3 * std::sin(6.28318530718 * phase)
                                  + 0.2  * std::sin(2 * 6.28318530718 * phase)
                                  + 0.1  * std::sin(3 * 6.28318530718 * phase)));
            if (i > (long)(HOST_SR * 0.3) && pt.isVoiced())
            {
                ++checks;
                const float hz = pt.getRawPitchHz();
                if (hz < 120.0f || hz > 140.0f) ++bad;
            }
        }
        printf("[T3] ヴィブラート130Hz: checks=%d 逸脱=%d -> %s\n",
               checks, bad, (checks > 0 && bad == 0) ? "PASS" : "FAIL");
        if (checks == 0 || bad != 0) ++failures;
    }

    printf("PITCH TRACKER TEST: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures;
}
