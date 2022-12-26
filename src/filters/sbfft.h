#pragma once

#include "util.h"
#include "filters/sample.h"

#include <complex>
#include <vector>
#include <span>

std::vector<std::complex<float32>> r2c_fft(std::span<const sample> samples);
