#pragma once
#include <vector>
#include <cstdint>

namespace DSP {

class PitchDetector {
public:
    PitchDetector();
    ~PitchDetector() = default;

    // バッファをリセット
    void reset();

    // 16kHzでサンプリングされた入力信号からピッチを検出する。
    // 入力信号は W + max_delay = 512 + 320 = 832 サンプル以上必要。
    // 返り値: 検出された周波数 (Hz)。有声判定でなかった場合、または検出されなかった場合は 0.0f。
    float detectPitch(const float* signal, int signalLength, float sampleRate = 16000.0f);

    // 有声判定（Voiced）の結果を返す
    bool isVoiced() const { return mIsVoiced; }
    
    // 周期性スコアを返す（0.0 = 完全な周期性、1.0 = 完全なランダムノイズ）
    float getPeriodicityScore() const { return mPeriodicityScore; }

private:
    // YIN アルゴリズムの各ステップ
    void difference(const float* signal, int signalLength);
    void cumulativeMeanNormalizedDifference();
    int absoluteThreshold(float threshold);
    float parabolicInterpolation(int tau);

    int mWindowSize; // 512
    int mMaxDelay;   // 320 (50Hz at 16kHz)
    int mMinDelay;   // 20  (800Hz at 16kHz)
    
    std::vector<float> mDifference;
    std::vector<float> mNormalizedDifference;
    
    bool mIsVoiced;
    float mPeriodicityScore;
};

} // namespace DSP
