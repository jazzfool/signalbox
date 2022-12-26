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
#include <span>

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

    void reload_device();
    std::span<const std::string> playback_devs() const;
    std::span<const std::string> capture_devs() const;

    std::atomic_uint8_t playback_l_chan;
    std::atomic_uint8_t playback_r_chan;
    std::atomic_bool playback_mute;

    std::atomic_uint8_t capture_l_chan;
    std::atomic_uint8_t capture_r_chan;
    std::atomic_bool capture_mute;

    jnk0le::Ringbuffer<std::string, 1> out_status;

    uint32 playback_dev_idx;
    uint32 capture_dev_idx;

private:
    friend void ma_data_callback(ma_device*, void*, const void*, ma_uint32);

    void create_device(bool uninit);
    void data_callback(void* output, const void* input, uint32 frame_count);
    void execute_one(filter_base& f, channels& chans);
    void execute_all(channels& chans);

    filter_list& m_filters;
    ma_context m_context;
    ma_device m_device;

    std::vector<std::string> m_playback_dev_names;
    std::vector<ma_device_id> m_playback_dev_ids;
    std::vector<std::string> m_capture_dev_names;
    std::vector<ma_device_id> m_capture_dev_ids;
};
