#pragma once
#include <immintrin.h>

namespace SimdUtils {

// Xorshift32の状態ベクトルを更新し、新しい乱数を返す
inline __m256i xorshift32(__m256i& state) {
    // x ^= (x << 13);
    __m256i temp1 = _mm256_slli_epi32(state, 13);
    state = _mm256_xor_si256(state, temp1);

    // x ^= (x >> 17);
    __m256i temp2 = _mm256_srli_epi32(state, 17);
    state = _mm256_xor_si256(state, temp2);

    // x ^= (x << 5);
    __m256i temp3 = _mm256_slli_epi32(state, 5);
    state = _mm256_xor_si256(state, temp3);

    return state;
}

// [1.0, 2.0) の範囲のfloat乱数ベクトルを生成し、1.0fを引いて [0.0, 1.0) に変換する
inline __m256 randomFloat01(__m256i& state) {
    __m256i randInt = xorshift32(state);
    
    // 下位23ビットを抽出
    __m256i mantissa = _mm256_and_si256(randInt, _mm256_set1_epi32(0x007FFFFF));
    
    // 指数部に 127 (0x3F800000) をセットして [1.0, 2.0) の浮動小数点数を構築
    __m256i exponent = _mm256_or_si256(mantissa, _mm256_set1_epi32(0x3F800000));
    
    // floatにキャストして 1.0f を引く
    __m256 rawFloat = _mm256_castsi256_ps(exponent);
    return _mm256_sub_ps(rawFloat, _mm256_set1_ps(1.0f));
}

// [-1.0, 1.0) の範囲のfloat乱数ベクトルを生成
inline __m256 randomFloatBi(__m256i& state) {
    __m256 r = randomFloat01(state); // [0.0, 1.0)
    // r * 2.0 - 1.0
    return _mm256_fmadd_ps(r, _mm256_set1_ps(2.0f), _mm256_set1_ps(-1.0f));
}

// 8要素の__m256レジスタを水平加算してスカラーfloatで返す
inline float horizontalSum(__m256 v) {
    // 上位と下位の128ビットを抽出して加算
    __m128 low = _mm256_castps256_ps128(v);
    __m128 high = _mm256_extractf128_ps(v, 1);
    __m128 sum = _mm_add_ps(low, high);
    
    // 128ビットレジスタ内の加算 [a, b, c, d] -> [a+b, c+d, a+b, c+d] -> [a+b+c+d, ...]
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    
    return _mm_cvtss_f32(sum);
}

} // namespace SimdUtils
