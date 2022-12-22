#pragma once

#include "util.h"
#include "sample.h"

#include <miniaudio.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>
#include <ringbuffer.hpp>

static constexpr uint32 IO_SAMPLE_RATE = 48000;
static constexpr uint32 IO_FRAME_COUNT = 480;

struct filter_base;

struct filter_list final {
    std::vector<std::unique_ptr<filter_base>> filters;
    std::mutex mut;
};

class filter_executor final {
public:
    filter_executor(filter_list& filters);
    
    void create();
    void destroy();

    std::atomic_uint8_t out_l_chan;
    std::atomic_uint8_t out_r_chan;
    std::atomic_bool out_mute;
    jnk0le::Ringbuffer<std::string, 1> out_status;

private:
    friend void ma_data_callback(ma_device*, void*, const void*, ma_uint32);

    void data_callback(void* output, const void* input, uint32 frame_count);
    void execute_one(filter_base& f, channels& chans);
    void execute_all(channels& chans);

    filter_list& m_filters;
    ma_device m_device;
};
