#pragma once
#include <cmath>
#include <algorithm>

namespace DSP {

class CharacterProcessor {
public:
    CharacterProcessor()
        : mSampleRate(44100.0),
          mSampleCounter(0.0),
          mLastSampleL(0.0f),
          mLastSampleR(0.0f)
    {}
    
    ~CharacterProcessor() = default;

    void setup(double sampleRate)
    {
        mSampleRate = sampleRate;
        mSampleCounter = 0.0;
        mLastSampleL = 0.0f;
        mLastSampleR = 0.0f;
    }

    // Characterパラメータ (0.0 = Lo-Fi, 1.0 = Hi-Fi) を適用してサンプルを処理する
    inline void processSample(float& sampleL, float& sampleR, float character)
    {
        character = std::clamp(character, 0.0f, 1.0f);

        // 1. 実効サンプリングレート低減 (エイリアシング効果)
        // character=0 のとき 8kHz、character=1 のときホストサンプリングレートに連続可変
        double minRate = 8000.0;
        double targetRate = minRate + (mSampleRate - minRate) * static_cast<double>(character * character);
        double sampleInterval = mSampleRate / targetRate;

        mSampleCounter += 1.0;
        if (mSampleCounter >= sampleInterval)
        {
            mSampleCounter = std::fmod(mSampleCounter, sampleInterval);
            mLastSampleL = sampleL;
            mLastSampleR = sampleR;
        }
        else
        {
            // ダウンサンプリングされたホールド出力
            sampleL = mLastSampleL;
            sampleR = mLastSampleR;
        }

        // 2. ビットクラッシュ (量子化ノイズ)
        // character=0 で 4ビット、character=1 で 32ビット(浮動小数点)
        // character >= 0.95f のときは音質優先でビットクラッシュをスキップ
        if (character < 0.95f)
        {
            float bits = 4.0f + 24.0f * (character * character); // 4bit 〜 28bit
            float steps = std::pow(2.0f, bits - 1.0f);

            // 左
            float valL = std::clamp(sampleL, -1.0f, 1.0f);
            sampleL = std::round(valL * steps) / steps;

            // 右
            float valR = std::clamp(sampleR, -1.0f, 1.0f);
            sampleR = std::round(valR * steps) / steps;
        }
    }

private:
    double mSampleRate;
    double mSampleCounter;
    float mLastSampleL;
    float mLastSampleR;
};

} // namespace DSP
