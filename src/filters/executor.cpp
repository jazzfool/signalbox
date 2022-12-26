#include "executor.h"

#include "filter.h"

#include <spdlog/fmt/fmt.h>
#include <chrono>

void ma_data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    reinterpret_cast<filter_executor*>(device->pUserData)
        ->data_callback(output, input, static_cast<uint32>(frame_count));
}

filter_executor::filter_executor(filter_list& filters) : out_status{8}, m_filters{filters} {
    playback_l_chan.store(10);
    playback_r_chan.store(11);
    playback_mute.store(false);

    capture_l_chan.store(0);
    capture_r_chan.store(1);
    capture_mute.store(false);

    playback_dev_idx = 0;
    capture_dev_idx = 0;
}

void filter_executor::create() {
    sb_ASSERT(ma_context_init(nullptr, 0, nullptr, &m_context) == MA_SUCCESS);

    ma_device_info* playback_devs = nullptr;
    uint32 playback_dev_count = 0;
    ma_device_info* capture_devs = nullptr;
    uint32 capture_dev_count = 0;
    ma_context_get_devices(
        &m_context, &playback_devs, &playback_dev_count, &capture_devs, &capture_dev_count);

    m_playback_dev_names.reserve(playback_dev_count);
    m_playback_dev_ids.reserve(playback_dev_count);
    for (uint32 i = 0; i < playback_dev_count; ++i) {
        m_playback_dev_names.emplace_back(playback_devs[i].name);
        m_playback_dev_ids.push_back(playback_devs[i].id);
    }

    m_capture_dev_names.reserve(capture_dev_count);
    m_capture_dev_ids.reserve(capture_dev_count);
    for (uint32 i = 0; i < capture_dev_count; ++i) {
        m_capture_dev_names.emplace_back(capture_devs[i].name);
        m_capture_dev_ids.push_back(capture_devs[i].id);
    }

    create_device(false);
}

void filter_executor::destroy() {
    ma_device_uninit(&m_device);
    ma_context_uninit(&m_context);
}

void filter_executor::reload_device() {
    create_device(true);
}

std::span<const std::string> filter_executor::playback_devs() const {
    return m_playback_dev_names;
}

std::span<const std::string> filter_executor::capture_devs() const {
    return m_capture_dev_names;
}

void filter_executor::create_device(bool uninit) {
    if (uninit) {
        ma_device_uninit(&m_device);
    }

    auto config = ma_device_config_init(ma_device_type_duplex);

    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.playback.pDeviceID = &m_playback_dev_ids[playback_dev_idx];

    config.capture.format = ma_format_f32;
    config.capture.channels = 2;
    config.capture.pDeviceID = &m_capture_dev_ids[capture_dev_idx];
    config.capture.shareMode = ma_share_mode_shared;

    config.sampleRate = IO_SAMPLE_RATE;
    config.periodSizeInFrames = IO_FRAME_COUNT;
    config.dataCallback = ma_data_callback;
    config.pUserData = this;

    ma_device_init(nullptr, &config, &m_device);
    ma_device_start(&m_device);
}

void filter_executor::data_callback(void* output, const void* input, uint32 frame_count) {
    auto out = reinterpret_cast<sample*>(output);
    auto in = reinterpret_cast<const sample*>(input);

    sb_ASSERT_EQ(frame_count, IO_FRAME_COUNT);

    channels chans;
    chans.frame_count = frame_count;
    auto base_chan = channel{};
    base_chan.samples = std::vector(frame_count, 0.f);
    base_chan.sample_rate = IO_SAMPLE_RATE;
    for (auto& chan : chans.chans) {
        chan = base_chan;
    }

    if (!capture_mute.load()) {
        auto& capture_l_samples = chans.chans[capture_l_chan.load()].samples;
        auto& capture_r_samples = chans.chans[capture_r_chan.load()].samples;
        for (uint32 i = 0; i < frame_count; ++i) {
            capture_l_samples[i] = *(in + i * 2 + 0);
            capture_r_samples[i] = *(in + i * 2 + 1);
        }
    }

    const auto t_start = std::chrono::steady_clock::now();

    execute_all(chans);

    const auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t_start)
            .count();

    auto& playback_l_samples = chans.chans[playback_l_chan.load()].samples;
    auto& playback_r_samples = chans.chans[playback_r_chan.load()].samples;

    if (playback_l_samples.size() < frame_count) {
        out_status.insert(
            fmt::format("FAULT: Channel LEFT Sample Count {} < {}", playback_l_samples.size(), frame_count));
        return;
    }

    if (playback_r_samples.size() < frame_count) {
        out_status.insert(
            fmt::format("FAULT: Channel RIGHT Sample Count {} < {}", playback_r_samples.size(), frame_count));
        return;
    }

    const auto info =
        fmt::format("[{:04} frames out in {:05} mcs] @ {} Hz", frame_count, dur, IO_SAMPLE_RATE);

    if (playback_mute.load()) {
        out_status.insert(fmt::format("MUTED {}", info));
        return;
    }

    for (uint32 i = 0; i < frame_count; ++i) {
        *(out + i * 2 + 0) = playback_l_samples[i];
        *(out + i * 2 + 1) = playback_r_samples[i];
    }

    out_status.insert(fmt::format("ACTIVE {}", info));
}

void filter_executor::execute_one(filter_base& f, channels& chans) {
    f.apply(chans);
}

void filter_executor::execute_all(channels& chans) {
    auto lock = std::lock_guard{m_filters.mut};
    for (auto& f : m_filters.filters) {
        execute_one(*f, chans);
    }
}
