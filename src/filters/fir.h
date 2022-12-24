#pragma once

#include "filter.h"

#include <memory>

std::unique_ptr<filter_base> fltr_fir_delay();
std::unique_ptr<filter_base> fltr_fir_gain();
