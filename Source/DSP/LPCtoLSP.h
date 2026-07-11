#pragma once
#include <vector>

namespace DSP {

class LPCtoLSP {
public:
    LPCtoLSP();
    ~LPCtoLSP() = default;

    // 初期化
    void setup(int maxOrder = 24);

    // LPC係数からLSP係数（余弦ドメイン x = cos(omega)）へ変換する
    // 入力: lpcCoeffs (サイズ order + 1、lpcCoeffs[0] = 1.0)
    // 出力: lspCoeffs (サイズ order、余弦ドメイン [-1, 1]、降順)
    // ※ 根の探索は x = 1.0 から -1.0 に向かって行うため、余弦値は 1.0 から -1.0 へ降順となり、
    //    これは物理周波数（omega = acos(x)）の 0 から π への昇順に対応します。
    bool convert(const std::vector<float>& lpcCoeffs, std::vector<float>& lspCoeffs, int order);

private:
    // Clenshaw漸化式によるChebyshev多項式とその微分の同時評価 (自動微分)
    void evaluateChebyshev(float x, const float* coeffs, int degree, float& fx, float& dfx);

    int mMaxOrder;
    
    // 多項式係数バッファ (サイズ maxOrder / 2 + 1)
    std::vector<float> mP;
    std::vector<float> mQ;
};

} // namespace DSP
