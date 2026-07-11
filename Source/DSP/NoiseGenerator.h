#pragma once
#include <immintrin.h>
#include "SimdUtils.h"

namespace DSP {

class NoiseGenerator {
public:
    NoiseGenerator()
    {
        // 8ボイス用の初期シード（Xorshift32では0以外にする必要がある）
        mState = _mm256_setr_epi32(123456789, 362436069, 521288629, 88675123,
                                   5789021, 987654321, 135790246, 246801357);
    }
    
    ~NoiseGenerator() = default;

    // 8ボイス分の独立した白色ノイズ（双極性 [-1.0f, 1.0f)）を __m256 ベクトルで生成する
    inline __m256 nextBlockAVX2()
    {
        return SimdUtils::randomFloatBi(mState);
    }

private:
    __m256i mState;
};

} // namespace DSP
