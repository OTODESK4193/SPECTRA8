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

    int M = order / 2;
    float alpha = std::clamp(shift, -0.9f, 0.9f); // 極端な変形を防ぐために制限
    float alphaSq = alpha * alpha;
    float onePlusAlphaSq = 1.0f + alphaSq;
    float twoAlpha = 2.0f * alpha;

    std::vector<float> lsfsP;
    std::vector<float> lsfsQ;
    lsfsP.reserve(M);
    lsfsQ.reserve(M);

    // 1. P極 と Q極 を分離し、双一次写像によるワープを適用
    for (int i = 0; i < M; ++i)
    {
        float xP = lspCoeffs[2 * i];
        float xQ = lspCoeffs[2 * i + 1];

        // Warping P
        float numP = onePlusAlphaSq * xP - twoAlpha;
        float denP = onePlusAlphaSq - twoAlpha * xP;
        float tempP = (std::abs(denP) > 1e-5f) ? numP / denP : xP;
        tempP = std::clamp(tempP, -0.999f, 0.999f);
        lsfsP.push_back(std::acos(tempP));

        // Warping Q
        float numQ = onePlusAlphaSq * xQ - twoAlpha;
        float denQ = onePlusAlphaSq - twoAlpha * xQ;
        float tempQ = (std::abs(denQ) > 1e-5f) ? numQ / denQ : xQ;
        tempQ = std::clamp(tempQ, -0.999f, 0.999f);
        lsfsQ.push_back(std::acos(tempQ));
    }

    // 2. アフィン変換によるフォルマントのストレッチ/スクィーズ (P極とQ極を個別に適用)
    if (std::abs(stretch - 1.0f) > 1e-4f)
    {
        // P極の平均とストレッチ
        float sumP = 0.0f;
        for (float val : lsfsP) sumP += val;
        float meanP = sumP / static_cast<float>(M);
        for (int i = 0; i < M; ++i)
        {
            lsfsP[i] = meanP + stretch * (lsfsP[i] - meanP);
            lsfsP[i] = std::clamp(lsfsP[i], 0.001f, 3.1415f);
        }

        // Q極の平均とストレッチ
        float sumQ = 0.0f;
        for (float val : lsfsQ) sumQ += val;
        float meanQ = sumQ / static_cast<float>(M);
        for (int i = 0; i < M; ++i)
        {
            lsfsQ[i] = meanQ + stretch * (lsfsQ[i] - meanQ);
            lsfsQ[i] = std::clamp(lsfsQ[i], 0.001f, 3.1415f);
        }
    }

    // 3. それぞれ個別にソートして順序を保証
    std::sort(lsfsP.begin(), lsfsP.end());
    std::sort(lsfsQ.begin(), lsfsQ.end());

    // 4. 交互にマージして交互配置を100%保証
    std::vector<float> lsfs(order);
    for (int i = 0; i < M; ++i)
    {
        lsfs[2 * i] = lsfsP[i];
        lsfs[2 * i + 1] = lsfsQ[i];
    }

    // 5. LSFガードバンドの適用によるフィルタ安定化 (PとQの交互関係を崩さないように全体ソート)
    float minDistance = 0.25f * 3.14159265f / static_cast<float>(order + 1);
    std::sort(lsfs.begin(), lsfs.end()); // PとQは交互に並んでいるため、全体ソートしても関係は維持されます
    applyGuardBand(lsfs, minDistance, order);

    // 6. 余弦ドメイン (LSP) に逆変換 (角度昇順なので、cosは降順になる)
    for (int i = 0; i < order; ++i)
    {
        shiftedLsp[i] = std::cos(lsfs[i]);
    }
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
