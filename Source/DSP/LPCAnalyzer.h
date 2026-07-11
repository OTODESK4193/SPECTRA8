#pragma once
#include <vector>

namespace DSP {

class LPCAnalyzer {
public:
    LPCAnalyzer();
    ~LPCAnalyzer() = default;

    // 分析の初期化
    void setup(int maxOrder = 24);

    // LPC分析を実行する
    // 入力: frame (ハミング窓未適用の素の信号フレーム)
    // 出力: coeffs (予測係数ベクトル、サイズ order + 1、coeffs[0] = 1.0, coeffs[1..order] = a_1..a_p)
    //       ※予測フィルタは A(z) = 1 - sum_{i=1}^p a_i z^-i
    //       gain (予測残差エネルギーの平方根)
    // 返り値: 成功したら true
    bool analyze(const float* frame, int frameSize, std::vector<float>& coeffs, float& gain, int order);

    // プリエンファシスフィルタを適用する
    void applyPreEmphasis(const float* input, float* output, int size, float coeff = 0.97f);
    void resetPreEmphasis();

    // ハミング窓を適用する
    void applyWindow(const float* input, float* output, int size);

private:
    // 自己相関関数を計算する
    void autocorrelation(const float* signal, int size, std::vector<float>& r, int order);

    // Levinson-Durbinアルゴリズム
    bool levinsonDurbin(const std::vector<float>& r, std::vector<float>& a, float& E, int order);

    int mMaxOrder;
    float mPrevSample;
    std::vector<float> mWindow;
    std::vector<float> mAutocorr;
    std::vector<float> mWindowedFrame;
    std::vector<float> mPreEmphasizedFrame;
};

} // namespace DSP
