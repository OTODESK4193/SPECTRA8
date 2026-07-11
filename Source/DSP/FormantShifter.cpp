#include "FormantShifter.h"
#include <cmath>
#include <algorithm>

namespace DSP {

void FormantShifter::process(const std::vector<float>& lspCoeffs, 
                             std::vector<float>& shiftedLsp, 
                             float shift, 
                             float stretch, 
                             int order)
{
    shiftedLsp.resize(order);

    // 1. 双一次共形写像によるシフティング (余弦ドメインで直接実行、超越関数不使用)
    // x' = ((1 + a^2)*x - 2a) / (1 + a^2 - 2a*x)
    float alpha = std::clamp(shift, -0.9f, 0.9f); // 極端な変形を防ぐために制限
    float alphaSq = alpha * alpha;
    float onePlusAlphaSq = 1.0f + alphaSq;
    float twoAlpha = 2.0f * alpha;

    std::vector<float> tempLsp(order);
    for (int i = 0; i < order; ++i)
    {
        float x = lspCoeffs[i];
        float numerator = onePlusAlphaSq * x - twoAlpha;
        float denominator = onePlusAlphaSq - twoAlpha * x;
        if (std::abs(denominator) > 1e-5f)
        {
            tempLsp[i] = numerator / denominator;
        }
        else
        {
            tempLsp[i] = x;
        }
        tempLsp[i] = std::clamp(tempLsp[i], -0.999f, 0.999f);
    }

    // 2. 角度ドメイン (LSF) への変換
    std::vector<float> lsfs(order);
    float sumOmega = 0.0f;
    for (int i = 0; i < order; ++i)
    {
        // tempLspは降順なので、acosによって得られるlsfs(角度)は昇順になる
        lsfs[i] = std::acos(tempLsp[i]);
        sumOmega += lsfs[i];
    }

    // 3. アフィン変換によるフォルマントのストレッチ/スクィーズ
    // w' = mean + sigma * (w - mean)
    if (std::abs(stretch - 1.0f) > 1e-4f)
    {
        float meanOmega = sumOmega / static_cast<float>(order);
        for (int i = 0; i < order; ++i)
        {
            lsfs[i] = meanOmega + stretch * (lsfs[i] - meanOmega);
            lsfs[i] = std::clamp(lsfs[i], 0.001f, 3.1415f);
        }
        // 角度ドメインのソートを維持
        std::sort(lsfs.begin(), lsfs.end());
    }

    // 4. LSFガードバンドの適用によるフィルタ安定化
    float minDistance = 0.05f * 3.14159265f / static_cast<float>(order + 1);
    applyGuardBand(lsfs, minDistance, order);

    // 5. 余弦ドメイン (LSP) に逆変換
    for (int i = 0; i < order; ++i)
    {
        // 角度 lsfs は昇順なので、cos変換された shiftedLsp は降順になる
        shiftedLsp[i] = std::cos(lsfs[i]);
    }
    
    // 安全のため降順ソートを確認
    std::sort(shiftedLsp.begin(), shiftedLsp.end(), std::greater<float>());
}

void FormantShifter::applyGuardBand(std::vector<float>& lsfs, float minDistance, int order)
{
    // 双方向プッシュ・プルガードバンド処理
    
    // 前進パス
    lsfs[0] = std::max(lsfs[0], minDistance);
    for (int i = 1; i < order; ++i)
    {
        lsfs[i] = std::max(lsfs[i], lsfs[i - 1] + minDistance);
    }

    // 後退パス
    lsfs[order - 1] = std::min(lsfs[order - 1], 3.14159265f - minDistance);
    for (int i = order - 2; i >= 0; --i)
    {
        lsfs[i] = std::min(lsfs[i], lsfs[i + 1] - minDistance);
    }
}

} // namespace DSP
