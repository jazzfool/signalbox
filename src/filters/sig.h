#pragma once

#include "filter.h"

#include <memory>

std::unique_ptr<filter_base> fltr_sig_delay();
std::unique_ptr<filter_base> fltr_sig_gain();
