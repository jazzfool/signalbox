#pragma once

#include "util.h"

#include <vector>
#include <array>

static constexpr uint32 MAX_CHANNELS = 99;

using sample = float32;

struct channel final {
    std::vector<sample> samples;
    uint32 sample_rate;
};

struct channels final {
    std::array<channel, MAX_CHANNELS> chans;
    uint32 frame_count;
};
