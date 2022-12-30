#pragma once

#ifdef __ARM_NEON
#include <sse2neon.h>
#else
#include <immintrin.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// __m128 simd_sin(__m128 x);
// __m128 simd_cos(__m128 x);

#ifdef __cplusplus
}
#endif
