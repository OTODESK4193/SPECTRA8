#include "LSPtoLPC.h"
#include <cmath>
#include <algorithm>

namespace DSP {

LSPtoLPC::LSPtoLPC()
    : mMaxOrder(24)
{
    setup(mMaxOrder);
}

void LSPtoLPC::setup(int maxOrder)
{
    mMaxOrder = (maxOrder % 2 == 0) ? maxOrder : maxOrder + 1;
    
    // バッファの確保
    mFilterRootsBuffer.resize(mMaxOrder);
    mPPrime.resize(mMaxOrder + 1, 0.0f);
    mQPrime.resize(mMaxOrder + 1, 0.0f);
    mP.resize(mMaxOrder + 2, 0.0f);
    mQ.resize(mMaxOrder + 2, 0.0f);
}

void LSPtoLPC::expandFilter(const float* roots, int numRoots, float* outputCoeffs)
{
    // 多項式の畳み込みによる FIR 係数の展開
    // 展開される多項式の最大次数は 2 * numRoots (すなわち order)
    int order = 2 * numRoots;
    std::fill(outputCoeffs, outputCoeffs + order + 1, 0.0f);
    
    outputCoeffs[0] = 1.0f;

    for (int i = 0; i < numRoots; ++i)
    {
        float twoX = 2.0f * roots[i];
        int maxK = 2 * i + 2;

        // インプレースで (1 - 2*x*z^-1 + z^-2) を畳み込む
        // C_new[k] = C_old[k] - 2*x*C_old[k-1] + C_old[k-2]
        
        outputCoeffs[maxK] += outputCoeffs[maxK - 2];
        if (maxK - 1 >= 1)
        {
            outputCoeffs[maxK - 1] += -twoX * outputCoeffs[maxK - 2] + (maxK - 3 >= 0 ? outputCoeffs[maxK - 3] : 0.0f);
        }

        for (int k = maxK - 2; k >= 2; --k)
        {
            outputCoeffs[k] = outputCoeffs[k] - twoX * outputCoeffs[k - 1] + outputCoeffs[k - 2];
        }

        outputCoeffs[1] = outputCoeffs[1] - twoX * outputCoeffs[0];
    }
}

bool LSPtoLPC::convert(const std::vector<float>& lspCoeffs, std::vector<float>& lpcCoeffs, int order)
{
    if (order % 2 != 0 || order > mMaxOrder || lspCoeffs.size() < static_cast<size_t>(order))
    {
        return false;
    }

    int M = order / 2;
    lpcCoeffs.assign(order + 1, 0.0f);
    lpcCoeffs[0] = 1.0f; // a_0 = 1.0

    // LSP根 (余弦ドメインの x_i) を奇数インデックス (P') と偶数インデックス (Q') に分離する。
    // LSP 根は降順（物理周波数は昇順）で格納されている。
    // lspCoeffs[0] -> P' の最初の根
    // lspCoeffs[1] -> Q' の最初の根
    // lspCoeffs[2] -> P' の2番目の根...
    std::vector<float> rootsP(M);
    std::vector<float> rootsQ(M);

    for (int i = 0; i < M; ++i)
    {
        rootsP[i] = lspCoeffs[2 * i];
        rootsQ[i] = lspCoeffs[2 * i + 1];
    }

    // 1. 各2次セクションを展開
    mPPrime.assign(order + 1, 0.0f);
    mQPrime.assign(order + 1, 0.0f);

    expandFilter(rootsP.data(), M, mPPrime.data());
    expandFilter(rootsQ.data(), M, mQPrime.data());

    // 2. 自明な根を掛け合わせる
    // P(z) = (1 + z^-1) * P'(z)  =>  p_k = p'_k + p'_{k-1}
    // Q(z) = (1 - z^-1) * Q'(z)  =>  q_k = q'_k - q'_{k-1}
    mP.assign(order + 2, 0.0f);
    mQ.assign(order + 2, 0.0f);

    mP[0] = mPPrime[0];
    mQ[0] = mQPrime[0];

    for (int k = 1; k <= order; ++k)
    {
        mP[k] = mPPrime[k] + mPPrime[k - 1];
        mQ[k] = mQPrime[k] - mQPrime[k - 1];
    }
    mP[order + 1] = mPPrime[order];
    mQ[order + 1] = -mQPrime[order];

    // 3. 元のLPC係数の復元
    // a_k = 0.5 * (p_k + q_k) (1 <= k <= order)
    for (int k = 1; k <= order; ++k)
    {
        lpcCoeffs[k] = 0.5f * (mP[k] + mQ[k]);
    }

    return true;
}

} // namespace DSP
