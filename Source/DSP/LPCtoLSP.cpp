#include "LPCtoLSP.h"
#include <cmath>
#include <algorithm>

namespace DSP {

LPCtoLSP::LPCtoLSP()
    : mMaxOrder(24)
{
    setup(mMaxOrder);
}

void LPCtoLSP::setup(int maxOrder)
{
    // 次数は偶数に限定
    mMaxOrder = (maxOrder % 2 == 0) ? maxOrder : maxOrder + 1;
    mP.resize(mMaxOrder / 2 + 1, 0.0f);
    mQ.resize(mMaxOrder / 2 + 1, 0.0f);
}

void LPCtoLSP::evaluateChebyshev(float x, const float* coeffs, int degree, float& fx, float& dfx)
{
    // Clenshaw漸化式と自動微分による同時評価
    float u_k1 = 0.0f;  // u_{k+1}
    float u_k2 = 0.0f;  // u_{k+2}
    float du_k1 = 0.0f; // u'_{k+1}
    float du_k2 = 0.0f; // u'_{k+2}

    float twoX = 2.0f * x;

    for (int k = degree; k >= 1; --k)
    {
        // 1. 微分の漸化式
        float du_k = twoX * du_k1 - du_k2 + 2.0f * u_k1;
        du_k2 = du_k1;
        du_k1 = du_k;

        // 2. Chebyshev多項式の漸化式
        float u_k = twoX * u_k1 - u_k2 + coeffs[k];
        u_k2 = u_k1;
        u_k1 = u_k;
    }

    // x = cos(omega) 上での評価
    fx = x * u_k1 - u_k2 + coeffs[0];
    dfx = x * du_k1 - du_k2 + u_k1;
}

bool LPCtoLSP::convert(const std::vector<float>& lpcCoeffs, std::vector<float>& lspCoeffs, int order)
{
    if (order % 2 != 0 || order > mMaxOrder)
    {
        return false;
    }

    int M = order / 2;
    lspCoeffs.clear();
    lspCoeffs.reserve(order);

    // 1. 多項式の分解 (前進差分 / 後退差分)
    // p_i = a_i + a_{P+1-i}
    // q_i = a_i - a_{P+1-i}
    std::vector<float> p(M + 1, 0.0f);
    std::vector<float> q(M + 1, 0.0f);

    p[0] = 1.0f;
    q[0] = 1.0f;

    for (int i = 1; i <= M; ++i)
    {
        float a_i = lpcCoeffs[i];
        float a_p_i = lpcCoeffs[order + 1 - i];

        p[i] = a_i + a_p_i - p[i - 1];
        q[i] = a_i - a_p_i + q[i - 1];
    }

    // 2. Chebyshev級数の係数に変換 (c_0 = p_M, c_k = 2 * p_{M-k})
    std::vector<float> coeffsP(M + 1);
    std::vector<float> coeffsQ(M + 1);

    coeffsP[0] = p[M];
    coeffsQ[0] = q[M];
    for (int k = 1; k <= M; ++k)
    {
        coeffsP[k] = 2.0f * p[M - k];
        coeffsQ[k] = 2.0f * q[M - k];
    }

    // 3. インタレース型ハイブリッド求根
    // P'とQ'の根は交互に出現する。x = 1.0 (低周波) から x = -1.0 (高域) に向かって走査。
    float x = 1.0f;
    float delta0 = 0.005f; // 適応ステップ用の初期ステップ幅
    
    bool searchP = true; // 最初に見つかる根は P' の根
    float prevVal = 0.0f;

    // 初期の評価値
    float dummyDfx;
    if (searchP)
    {
        evaluateChebyshev(x, coeffsP.data(), M, prevVal, dummyDfx);
    }
    else
    {
        evaluateChebyshev(x, coeffsQ.data(), M, prevVal, dummyDfx);
    }

    int rootsFound = 0;
    const int maxScanSteps = 2000;
    int step = 0;

    while (rootsFound < order && step < maxScanSteps)
    {
        // 適応型ステップ幅: x=1, -1付近は細かく、x=0付近は広く走査
        float dx = delta0 * (1.0f - 0.9f * x * x);
        x -= dx;

        if (x < -1.0f)
        {
            x = -1.0f;
        }

        // 現在値の評価
        float curVal = 0.0f;
        float dfx = 0.0f;
        
        if (searchP)
        {
            evaluateChebyshev(x, coeffsP.data(), M, curVal, dfx);
        }
        else
        {
            evaluateChebyshev(x, coeffsQ.data(), M, curVal, dfx);
        }

        // 符号が変化した（根を跨いだ）
        if (curVal * prevVal <= 0.0f)
        {
            // Newton-Raphson法でさらに高精度化（通常3回で十分な精度に達する）
            float xRoot = x + dx * 0.5f; // 跨いだ中点から開始
            for (int iter = 0; iter < 4; ++iter)
            {
                float fx_nr = 0.0f;
                float dfx_nr = 0.0f;
                if (searchP)
                {
                    evaluateChebyshev(xRoot, coeffsP.data(), M, fx_nr, dfx_nr);
                }
                else
                {
                    evaluateChebyshev(xRoot, coeffsQ.data(), M, fx_nr, dfx_nr);
                }

                if (std::abs(dfx_nr) > 1e-6f)
                {
                    xRoot -= fx_nr / dfx_nr;
                }
            }

            // 見つかった根を保存
            if (std::isnan(xRoot))
            {
                xRoot = x + dx * 0.5f;
            }
            xRoot = std::clamp(xRoot, -0.999f, 0.999f);
            lspCoeffs.push_back(xRoot);
            rootsFound++;

            // 次の探索対象を切り替え (P' <-> Q' 交互に出現するため)
            searchP = !searchP;

            // 走査点を根の位置に移動し、少しだけ進める
            x = xRoot - 1e-4f;
            
            // 切り替えた多項式の評価値を再取得
            if (searchP)
            {
                evaluateChebyshev(x, coeffsP.data(), M, curVal, dfx);
            }
            else
            {
                evaluateChebyshev(x, coeffsQ.data(), M, curVal, dfx);
            }
        }

        prevVal = curVal;
        step++;

        if (x <= -1.0f)
        {
            break;
        }
    }

    // 順序の整合性をチェック（LSP根は1.0から-1.0へ降順に交互配置されている必要がある）
    if (rootsFound == order)
    {
        // 交互配置の安全ガード（昇順、降順のソート一貫性）
        std::sort(lspCoeffs.begin(), lspCoeffs.end(), std::greater<float>());
        return true;
    }

    // 全ての根が見つからなかった場合、フォールバックとして等間隔に配置する
    lspCoeffs.clear();
    for (int i = 0; i < order; ++i)
    {
        float angle = static_cast<float>(i + 1) * 3.14159265f / static_cast<float>(order + 1);
        lspCoeffs.push_back(std::cos(angle));
    }
    
    return false;
}

} // namespace DSP
