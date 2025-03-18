#include <immintrin.h>  // AVX头文件
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void sqrtAvx(int N,
    float initialGuess,
    float values[],
    float output[])
{

    const int avxWidth = 8; // AVX 寄存器可以处理 8 个 float
    __m256 threshold = _m256_set1_ps(kThreshold);
    for(int i = 0; i < N; i+=avxWidth) {
        __m256 x_vec = _mm256_loadu_ps(&values[i]);
        
    }
    
}
