#pragma once
#include <vector>

namespace DSP {

class BarkFilterBank {
public:
    BarkFilterBank();
    ~BarkFilterBank() = default;

    // フィルタバンクの初期化
    // fftSize: FFTのサイズ（例: 1024）
    // sampleRate: 分析用サンプリングレート（例: 16000.0f）
    void setup(int fftSize, float sampleRate = 16000.0f);

    // 入力複素パワースペクトル（サイズ fftSize/2 + 1）から各バンドのエネルギーを計算する
    // 入力: powerSpectrum (リニアパワースペクトル、すなわち 実部^2 + 虚部^2)
    // 出力: bandEnergies (各Barkバンドのエネルギー値、サイズは mNumBands = 24)
    void process(const float* powerSpectrum, std::vector<float>& bandEnergies);

    // 各バンドのエネルギーから、スペクトル全体（サイズ fftSize/2 + 1）の下限拘束エンベロープを再構成する
    // 出力: lowerBoundSpectrum (再構成されたパワースペクトル、リニアスケール)
    void reconstructLowerBound(const std::vector<float>& bandEnergies, std::vector<float>& lowerBoundSpectrum);

    // ヘルパー：周波数(Hz)からBarkに変換
    static float hzToBark(float hz);
    // ヘルパー：Barkから周波数(Hz)に変換
    static float barkToHz(float bark);

private:
    void calculateFilterWeights();

    int mFftSize;
    int mNumBins; // fftSize / 2 + 1
    float mSampleRate;
    int mNumBands; // 24バンド

    // mFilterWeights[bandIndex][binIndex]
    std::vector<std::vector<float>> mFilterWeights;
};

} // namespace DSP
