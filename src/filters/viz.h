#pragma once

#include "filter.h"

#include <memory>

std::unique_ptr<filter_base> fltr_viz_waveform();
std::unique_ptr<filter_base> fltr_viz_vu();
