#include <immintrin.h>  // AVX头文件
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

extern void sqrtSerial(int N, float startGuess, float* values, float* output);

void sqrtAvx(int N,
    float initialGuess,
    float values[],
    float output[])
{
    const int avx_width = 8;    // avx2可以同时处理8个float
    static const __m256 kThreshold = _mm256_set1_ps(0.000001f);   // 判断浮点误差是否够小

    for(int i = 0; i < N; i+=avx_width) {
        if(i + 8 > N)   {
            sqrtSerial(N-i, initialGuess, values+i, output+i);  // 不够进行一次SIMD计算的情况下，调用串行版本的平方根来计算
            __m256 x_vec = _mm256_loadu_ps(values+i);   // 加载数据
            __m256 guess = _mm256_set1_ps(initialGuess);    // 初始化guess

            while(1) {  // error_vec = (guess * guess * x) - 1.0f
                __m256 error_vec = _mm256_sub_ps(
                    _mm256_mul_ps(_mm256_mul_ps(guess, guess), x_vec),
                    _mm256_set1_ps(1.0f)
                );
                error_vec = _mm256_andnot_ps(_mm256_set1_ps(-0.f), error_vec);  // abs(error_vec)
                __m256 cmp = _mm256_cmp_ps(error_vec, kThreshold, _CMP_GT_OQ);
                if(_mm256_testz_ps(cmp, cmp)) { // 达到精度，跳出循环
                    break;
                }
                // newGuess = (3.f - x_vec * guess * guess) * guess * 0.5f
                __m256 new_guess = _mm256_mul_ps(
                    _mm256_mul_ps(
                        _mm256_sub_ps(
                            _mm256_set1_ps(3.0f),
                            _mm256_mul_ps(x_vec, _mm256_mul_ps(guess, guess))
                        ), guess
                    ),
                    _mm256_set1_ps(0.5f)
                );
                guess = _mm256_blendv_ps(guess, new_guess, cmp);    // 选择保留哪个guess
            }
            _mm256_storeu_ps(output+i, _mm256_mul_ps(x_vec, guess));
        }
    }
}
