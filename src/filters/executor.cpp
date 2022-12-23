#include "executor.h"

#include "filter.h"

#include <spdlog/fmt/fmt.h>
#include <chrono>

void ma_data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    reinterpret_cast<filter_executor*>(device->pUserData)
        ->data_callback(output, input, static_cast<uint32>(frame_count));
}

filter_executor::filter_executor(filter_list& filters) : out_status{8}, m_filters{filters} {
    out_l_chan.store(0);
    out_r_chan.store(1);
    out_mute.store(false);
}

void filter_executor::create() {
    auto config = ma_device_config_init(ma_device_type_duplex);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = IO_SAMPLE_RATE;
    config.periodSizeInFrames = IO_FRAME_COUNT;
    config.dataCallback = ma_data_callback;
    config.pUserData = this;

    ma_device_init(nullptr, &config, &m_device);
    ma_device_start(&m_device);
}

void filter_executor::destroy() {
    ma_device_uninit(&m_device);
}

void filter_executor::data_callback(void* output, const void* input, uint32 frame_count) {
    auto out = reinterpret_cast<sample*>(output);
    auto in = reinterpret_cast<const sample*>(input);

    channels chans;
    chans.frame_count = frame_count;
    auto base_chan = channel{};
    base_chan.samples = std::vector(frame_count, 0.f), base_chan.sample_rate = IO_SAMPLE_RATE;
    for (auto& chan : chans.chans) {
        chan = base_chan;
    }

    const auto t_start = std::chrono::steady_clock::now();

    execute_all(chans);

    const auto dur =
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t_start)
            .count();

    const auto& l_samples = chans.chans[out_l_chan.load()].samples;
    const auto& r_samples = chans.chans[out_r_chan.load()].samples;

    if (l_samples.size() < frame_count) {
        out_status.insert(
            fmt::format("FAULT: Channel LEFT Sample Count {} < {}", l_samples.size(), frame_count));
        return;
    }

    if (r_samples.size() < frame_count) {
        out_status.insert(
            fmt::format("FAULT: Channel RIGHT Sample Count {} < {}", r_samples.size(), frame_count));
        return;
    }
    
    const auto info = fmt::format("[{:04} frames out in {:05} mcs] @ {} Hz", frame_count, dur, IO_SAMPLE_RATE);

    if (out_mute.load()) {
        out_status.insert(fmt::format("MUTED {}", info));
        return;
    }

    for (uint32 i = 0; i < frame_count; ++i) {
        ::memcpy(out + i * 2 + 0, &l_samples[i], sizeof(float32));
        ::memcpy(out + i * 2 + 1, &r_samples[i], sizeof(float32));
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
