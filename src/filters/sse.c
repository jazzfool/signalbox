#include "sse.h"

#define SSE_MATHFUN_WITH_CODE
#include <sse_mathfun.h>

__m128 simd_sin(__m128 x) {
    return sin_ps(x);
}

__m128 simd_cos(__m128 x) {
    return cos_ps(x);
}
