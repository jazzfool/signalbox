#pragma once

#include "util.h"

#include <miniaudio.h>
#include <vector>
#include <string>

class tracker final {
  public:
    static constexpr uint32 SAMPLE_RATE = 48000;
    static constexpr uint32 FRAME_COUNT = 480;

    void create();
    void destroy();

    void ui();

  private:
    friend void ma_data_callback(ma_device*, void*, const void*, ma_uint32);

    void create_device();
    void data_callback(void* output, const void* input, uint32 frame_count);

    ma_context m_context;
    ma_device m_device;

    std::vector<std::string> m_playback_dev_names;
    std::vector<ma_device_id> m_playback_dev_ids;
    std::vector<std::string> m_capture_dev_names;
    std::vector<ma_device_id> m_capture_dev_ids;

    uint32 m_playback_dev_idx;
    uint32 m_capture_dev_idx;
};
