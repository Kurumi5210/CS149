#include <chrono>
#include <cstdio>
#include <immintrin.h>
#include <omp.h>

void sinx_scalar(int N, int terms, float *x, float *y)
{
	for(int i = 0; i < N; i++)
	{
		float value = x[i];
		float numer = x[i] * x[i] * x[i];
		int denom = 6; //3!
		int sign = -1;
		for(int j = 1; j<= terms; j++)
		{
			value += sign * numer / denom;
			numer *= x[i] * x[i];
			denom *= (2*j+2) * (2*j+3);
			sign *= -1;
		}
		y[i] = value;
	}
}


void sinx_avx(int N, int terms, float *x, float *y)
{
	int i;
	for(i = 0; i < N - 7; i += 8)
	{
		__m256 x_vec = _mm256_loadu_ps(&x[i]);

		__m256 value = x_vec;

		__m256 x_n = _mm256_mul_ps(_mm256_mul_ps(x_vec, x_vec), x_vec);
		__m256 denom = _mm256_set1_ps(6.0f);
		__m256 sign = _mm256_set1_ps(-1.0f);

		for(int j = 1; j <= terms; j++)
		{
			__m256 term = _mm256_div_ps(_mm256_mul_ps(sign, x_n), denom);

			value = _mm256_add_ps(value, term);

			x_n = _mm256_mul_ps(_mm256_mul_ps(x_vec, x_vec), x_n);

			denom = _mm256_mul_ps(_mm256_set1_ps((2*j+2) * (2*j+3)), denom);

			sign = _mm256_mul_ps(sign, _mm256_set1_ps(-1.0f));

		}

		_mm256_storeu_ps(&y[i], value);
	}
	// 处理剩余的元素
	for(; i < N; i++)
	{
		float value = x[i];
		float numer = x[i] * x[i] * x[i];
		int denom = 6; //3!
		int sign = -1;
		for(int j = 1; j<= terms; j++)
		{
			value += sign * numer / denom;
			numer *= x[i] * x[i];
			denom *= (2*j+2) * (2*j+3);
			sign *= -1;
		}
		y[i] = value;
	}
}

void sinx_avx_parallel(int N, int terms, float *x, float *y)
{
	#pragma omp parallel
	{
		#pragma omp parallel for
		for(int i = 0; i < N - 7; i += 8)
		{
			__m256 x_vec = _mm256_loadu_ps(&x[i]);
	
			__m256 value = x_vec;
	
			__m256 x_n = _mm256_mul_ps(_mm256_mul_ps(x_vec, x_vec), x_vec);
			__m256 denom = _mm256_set1_ps(6.0f);
			__m256 sign = _mm256_set1_ps(-1.0f);
	
			for(int j = 1; j <= terms; j++)
			{
				__m256 term = _mm256_div_ps(_mm256_mul_ps(sign, x_n), denom);
	
				value = _mm256_add_ps(value, term);
	
				x_n = _mm256_mul_ps(_mm256_mul_ps(x_vec, x_vec), x_n);
	
				denom = _mm256_mul_ps(_mm256_set1_ps((2*j+2) * (2*j+3)), denom);
	
				sign = _mm256_mul_ps(sign, _mm256_set1_ps(-1.0f));
	
			}
	
			_mm256_storeu_ps(&y[i], value);
		}
		// 处理剩余的元素
		#pragma omp for
		for(int i = N - (N % 8); i < N; i++)
		{
			float value = x[i];
			float numer = x[i] * x[i] * x[i];
			int denom = 6; //3!
			int sign = -1;
			for(int j = 1; j<= terms; j++)
			{
				value += sign * numer / denom;
				numer *= x[i] * x[i];
				denom *= (2*j+2) * (2*j+3);
				sign *= -1;
			}
			y[i] = value;
		}
	}
}


int main()
{
	const int N = 1000000;
	const int terms = 10;
	float *x = new float[N];
	float *y_scalar = new float[N];
	float *y_avx = new float[N];

	// 初始化输入数据
	for(int i = 0; i < N; i++)
	{
		x[i] = static_cast<float>(i) / N;
	}
	// 无优化
	auto start = std::chrono::high_resolution_clock::now();
	sinx_scalar(N, terms, x, y_scalar);
	auto end = std::chrono::high_resolution_clock::now();
	auto duration_scalar = std::chrono::duration_cast<std::chrono::microseconds>(end -  start);
	// avx
	start = std::chrono::high_resolution_clock::now();
	sinx_avx(N, terms, x, y_scalar);
	end = std::chrono::high_resolution_clock::now();
	auto duration_avx = std::chrono::duration_cast<std::chrono::microseconds>(end -  start);

	// avx parallel
	start = std::chrono::high_resolution_clock::now();
	sinx_avx_parallel(N, terms, x, y_scalar);
	end = std::chrono::high_resolution_clock::now();
	auto duration_avx_parallel = std::chrono::duration_cast<std::chrono::microseconds>(end -  start);

	printf("标量版本耗时：%ld 微秒\n", duration_scalar.count());
	printf("AVX 版本耗时：%ld 微秒\n", duration_avx.count());
	printf("并行 AVX 版本耗时：%ld 微秒\n", duration_avx_parallel.count());
	printf("加速比：%.2f\n",
		   static_cast<float>(duration_scalar.count()) / duration_avx.count());
	printf("并行 AVX 加速比：%.2f\n",
		   static_cast<float>(duration_scalar.count()) /
			   duration_avx_parallel.count());
	delete[] x;
	delete[] y_scalar;
	delete[] y_scalar;

	return 0;

}