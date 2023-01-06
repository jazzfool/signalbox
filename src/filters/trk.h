#pragma once

#include "filter.h"

#include <memory>

enum class tracker_note_mode : uint8 { empty, on, off, _max };

struct tracker_note final {
    tracker_note_mode mode = tracker_note_mode::empty;
    uint8 index = 0;
};

std::unique_ptr<filter_base> fltr_trk_sampler();
std::unique_ptr<filter_base> fltr_trk_sample_browser();
std::unique_ptr<filter_base> fltr_trk_tracker();
std::unique_ptr<filter_base> fltr_trk_clock();
