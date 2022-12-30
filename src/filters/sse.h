#pragma once

#include <immintrin.h>

#ifdef __cplusplus
extern "C" {
#endif

__m128 simd_sin(__m128 x);
__m128 simd_cos(__m128 x);

#ifdef __cplusplus
}
#endif
