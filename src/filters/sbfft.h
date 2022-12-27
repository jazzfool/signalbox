#pragma once

#include "util.h"
#include "filters/sample.h"
#include "simd.h"

#include <complex>
#include <vector>
#include <span>

simd_vec<std::complex<float32>> r2c_fft(simd_slice<const sample> samples, std::size_t pad = 0);
simd_vec<float32> c2r_ifft(simd_slice<const std::complex<float32>> c);
