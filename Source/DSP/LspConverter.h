#pragma once
#include <cmath>
#include <algorithm>
// LPC(a) <-> LSF(線スペクトル周波数) 変換 (M4, フェーズ2計画書 §4/§5.x)
//  A(z)=1+Σ a_i z^-i (a[0]=1, 次数p=偶数を想定: 8/10/12/16)
//  P(z)=B+rev(B) (対称, 根 z=-1), Q(z)=B-rev(B) (反対称, 根 z=+1), B=[a 0]
//  LSFは P,Q の単位円上零点角(0,π)で交互に並ぶ。グリッド走査+二分法で求根(複素根計算不要)。
//  JUCE非依存・アロケーションなし・RTセーフ。全演算double。
class LspConverter
{
public:
    static constexpr int kMaxOrder = 16;
    static constexpr double kPi = 3.14159265358979323846; // MSVCは<cmath>でM_PI未定義のため自前定義

    // 反射係数k[0..p-1] → LPC係数a[0..p] (step-up, a[0]=1)
    static void reflToLpc(const float* k, int p, double* a) noexcept
    {
        a[0] = 1.0; for (int i = 1; i <= p; ++i) a[i] = 0.0;
        for (int i = 1; i <= p; ++i)
        {
            double ki = k[i - 1], an[kMaxOrder + 1] = {};
            for (int j = 1; j < i; ++j) an[j] = a[j] + ki * a[i - j];
            an[i] = ki;
            for (int j = 1; j <= i; ++j) a[j] = an[j];
        }
    }
    // LPC係数a[0..p] → 反射係数k[0..p-1] (step-down)。不安定段でfalse。
    static bool lpcToRefl(const double* a, int p, float* k) noexcept
    {
        double ai[kMaxOrder + 1]; for (int i = 0; i <= p; ++i) ai[i] = a[i];
        for (int i = p; i >= 1; --i)
        {
            double ki = ai[i];
            if (ki >= 0.999 || ki <= -0.999) return false;
            k[i - 1] = (float)ki;
            const double d = 1.0 - ki * ki;
            double prev[kMaxOrder + 1] = {};
            for (int j = 1; j < i; ++j) prev[j] = (ai[j] - ki * ai[i - j]) / d;
            for (int j = 1; j < i; ++j) ai[j] = prev[j];
        }
        return true;
    }


    // a[0..p](a[0]=1) → lsf[0..p-1](rad昇順)。成功true。根数不一致(根喪失)でfalse。
    static bool lpcToLsf(const double* a, int p, double* lsf) noexcept
    {
        const int L = p + 2;
        double B[kMaxOrder + 2], Pc[kMaxOrder + 2], Qc[kMaxOrder + 2];
        for (int i = 0; i <= p; ++i) B[i] = a[i];
        B[p + 1] = 0.0;
        for (int i = 0; i < L; ++i) { Pc[i] = B[i] + B[L - 1 - i]; Qc[i] = B[i] - B[L - 1 - i]; }

        // P零点(p/2個)とQ零点(p/2個)をω∈(0,π)で走査。端(0,π)の自明根は除外。
        double rp[kMaxOrder], rq[kMaxOrder];
        int np = findRoots(Pc, L, p, true,  rp);
        int nq = findRoots(Qc, L, p, false, rq);
        if (np != p / 2 || nq != p / 2) return false;

        // 交互マージ(Q,P,Q,P,...): LSFは昇順で交互。まず全部集めてソート。
        int c = 0;
        for (int i = 0; i < nq; ++i) lsf[c++] = rq[i];
        for (int i = 0; i < np; ++i) lsf[c++] = rp[i];
        std::sort(lsf, lsf + p);
        // 昇順かつ (0,π) 内を確認
        for (int i = 0; i < p; ++i) if (lsf[i] <= 1e-6 || lsf[i] >= kPi - 1e-6) return false;
        for (int i = 1; i < p; ++i) if (lsf[i] <= lsf[i - 1]) return false;
        return true;
    }

    // lsf[0..p-1] → a[0..p] (a[0]=1)
    static void lsfToLpc(const double* lsf, int p, double* a) noexcept
    {
        // 昇順LSFを交互に P群/Q群 へ振り分け(sorted[0]はQ由来=最小)
        double Pp[kMaxOrder + 2] = {1.0}, Qq[kMaxOrder + 2] = {1.0};
        int pl = 1, ql = 1; // 現在の多項式長
        for (int i = 0; i < p; ++i)
        {
            const double c2 = -2.0 * std::cos(lsf[i]);
            // 昇順LSFは P(対称,根-1) と Q(反対称,根+1) が交互。最小(index0)はP側。
            if ((i & 1) == 0) convMul(Pp, pl, c2); // 偶数index → P群(対称)
            else              convMul(Qq, ql, c2); // 奇数index → Q群(反対称)
        }
        // P群に (1+z^-1), Q群に (1-z^-1) を掛ける
        convLin(Pp, pl, +1.0);
        convLin(Qq, ql, -1.0);
        // A(z) = (P(z)+Q(z))/2 の先頭 p+1 係数
        for (int i = 0; i <= p; ++i) a[i] = 0.5 * (Pp[i] + Qq[i]);
        a[0] = 1.0;
    }

private:
    // 対称(sym=true)/反対称(sym=false)多項式 C(length L) の単位円上零点角を走査
    static int findRoots(const double* C, int L, int p, bool sym, double* out) noexcept
    {
        const int want = p / 2;
        const int NG = 1024;
        double prev = evalG(C, L, p, sym, 1e-4);
        double prevw = 1e-4;
        int n = 0;
        for (int g = 1; g <= NG && n < want; ++g)
        {
            double w = kPi * (double)g / (double)(NG + 1);
            double cur = evalG(C, L, p, sym, w);
            if ((prev <= 0.0 && cur > 0.0) || (prev >= 0.0 && cur < 0.0))
            {
                // 二分法で精密化 (グリッド間隔~3e-3 → 40回で~3e-15、十分)
                double lo = prevw, hi = w, flo = prev;
                for (int it = 0; it < 40; ++it)
                {
                    double mid = 0.5 * (lo + hi);
                    double fm = evalG(C, L, p, sym, mid);
                    if ((flo <= 0.0 && fm <= 0.0) || (flo >= 0.0 && fm >= 0.0)) { lo = mid; flo = fm; }
                    else hi = mid;
                }
                out[n++] = 0.5 * (lo + hi);
            }
            prev = cur; prevw = w;
        }
        return n;
    }
    // C(e^{jω})·e^{j(p+1)ω/2} の実部(sym)または虚部(antisym) を返す(単位円上零点で0)
    //  M6最適化: cos(iω)/sin(iω) は角度加算の漸化式で生成 (2L回のtrig呼び出し→4回+4L乗算)。
    //  L≤18項の漸化式誤差は~1e-15規模で二分法精度(~1e-12)に影響しない。
    static double evalG(const double* C, int L, int p, bool sym, double w) noexcept
    {
        const double c1 = std::cos(w), s1 = std::sin(w);
        double cw = 1.0, sw = 0.0, re = 0.0, im = 0.0;
        for (int i = 0; i < L; ++i)
        {
            re += C[i] * cw;
            im -= C[i] * sw;
            const double t = cw * c1 - sw * s1;
            sw = sw * c1 + cw * s1;
            cw = t;
        }
        const double rot = 0.5 * (double)(p + 1) * w;
        const double cr = std::cos(rot), sr = std::sin(rot);
        // (re+j im)(cr+j sr)
        return sym ? (re * cr - im * sr) : (re * sr + im * cr);
    }
    // poly *= (1 + c2 z^-1 + z^-2)
    static void convMul(double* poly, int& len, double c2) noexcept
    {
        double kern[3] = {1.0, c2, 1.0};
        double tmp[kMaxOrder + 2] = {};
        for (int i = 0; i < len; ++i)
            for (int j = 0; j < 3; ++j) tmp[i + j] += poly[i] * kern[j];
        len += 2;
        for (int i = 0; i < len; ++i) poly[i] = tmp[i];
    }
    // poly *= (1 + s z^-1)
    static void convLin(double* poly, int& len, double s) noexcept
    {
        double tmp[kMaxOrder + 2] = {};
        for (int i = 0; i < len; ++i) { tmp[i] += poly[i]; tmp[i + 1] += poly[i] * s; }
        len += 1;
        for (int i = 0; i < len; ++i) poly[i] = tmp[i];
    }
};
