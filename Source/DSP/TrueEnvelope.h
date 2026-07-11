#pragma once
#include <vector>
#include <complex>
#include <memory>
#include <juce_dsp/juce_dsp.h>

namespace DSP {

class TrueEnvelope {
public:
    TrueEnvelope();
    ~TrueEnvelope() = default;

    // FFTサイズなどをセットアップ
    // 16kHzで動作するため、FFTサイズは1024(fftOrder = 10)がデフォルト。
    void setup(int fftOrder);

    // True Envelope 振幅包絡の抽出を実行する
    // 入力: windowedFrame (ハミング窓適用済の入力フレーム、実数データ)
    //       windowSize (入力フレームの有効サンプル数)
    // f0: ピッチ検出器から得られたピッチ(Hz)。有声判定でない場合はデフォルト値を使用
    // 出力: envelope (サイズ N_FFT / 2 + 1 の振幅包絡。リニア振幅スケール)
    void estimate(const float* windowedFrame, int windowSize, float f0, std::vector<float>& envelope, float sampleRate = 16000.0f);

private:
    int mFftOrder;
    int mFftSize;
    std::unique_ptr<juce::dsp::FFT> mFft;

    std::vector<float> mFftBuffer;
    
    std::vector<float> mLogSpectrum;     // A_0(k) = log|X(k)|
    std::vector<float> mLogEnvelope;     // V_i(k)
    std::vector<float> mTempLogSpectrum; // A_i(k)
    
    std::vector<float> mCepstrum;
    std::vector<float> mPrevCepstrum;
    std::vector<float> mLifterWindow;
};

} // namespace DSP
