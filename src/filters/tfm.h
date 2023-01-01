#pragma once

#include "filter.h"

#include <memory>

std::unique_ptr<filter_base> fltr_tfm_fft();
std::unique_ptr<filter_base> fltr_tfm_ifft();
std::unique_ptr<filter_base> fltr_tfm_xcorrel();
std::unique_ptr<filter_base> fltr_tfm_hann_window();
std::unique_ptr<filter_base> fltr_tfm_arithmetic();
std::unique_ptr<filter_base> fltr_tfm_normalize();
