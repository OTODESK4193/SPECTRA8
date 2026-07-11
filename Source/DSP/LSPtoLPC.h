#pragma once
#include <vector>

namespace DSP {

class LSPtoLPC {
public:
    LSPtoLPC();
    ~LSPtoLPC() = default;

    // 初期化
    void setup(int maxOrder = 24);

    // LSP（余弦ドメイン x = cos(omega)）からLPC係数へ逆変換する
    // 入力: lspCoeffs (サイズ order、降順で格納された LSP 根)
    // 出力: lpcCoeffs (サイズ order + 1、lpcCoeffs[0] = 1.0, lpcCoeffs[1..order] = a_1..a_p)
    bool convert(const std::vector<float>& lspCoeffs, std::vector<float>& lpcCoeffs, int order);

private:
    // 2次セクションのカスケードを多項式乗算 (FIRフィルタのインパルス応答) によって展開する
    // roots: LSP根のサブセット (奇数インデックスまたは偶数インデックス)
    // numRoots: 根の個数 (M = order / 2)
    // outputCoeffs: 展開された多項式係数 (サイズ M + 1)
    void expandFilter(const float* roots, int numRoots, float* outputCoeffs);

    int mMaxOrder;
    
    // リアルタイムスレッド内のアロケーション排除用バッファ
    std::vector<float> mFilterRootsBuffer;
    std::vector<float> mPPrime;
    std::vector<float> mQPrime;
    std::vector<float> mP;
    std::vector<float> mQ;
};

} // namespace DSP
