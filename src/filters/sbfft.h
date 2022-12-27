#pragma once

#include "util.h"
#include "filters/sample.h"
#include "simd.h"

#include <complex>
#include <vector>
#include <span>

simd_vec<std::complex<float32>> r2c_fft(std::span<const sample> samples);
simd_vec<float32> c2r_ifft(std::span<const std::complex<float32>> c);
