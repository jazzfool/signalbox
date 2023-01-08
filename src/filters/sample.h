#pragma once

#include "util.h"
#include "simd.h"

#include <vector>
#include <array>

static constexpr uint32 MAX_CHANNELS = 99;

using sample = float32;

struct channel final {
    simd_vec<sample> samples;
    uint32 sample_rate;
};

enum class byte_mode { raw, tracker };

struct byte_channel final {
    simd_vec<uint8> bytes;
    byte_mode mode;
};

struct channels final {
    std::array<channel, MAX_CHANNELS> chans;
    std::array<byte_channel, MAX_CHANNELS> byte_chans;
    uint32 frame_count;
};
