#include "executor.h"

#include "filter.h"

void ma_data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    reinterpret_cast<filter_executor*>(device->pUserData)
        ->data_callback(output, input, static_cast<uint32>(frame_count));
}

filter_executor::filter_executor(filter_list& filters) : m_filters{filters} {
    out_l_chan = 0;
    out_r_chan = 1;
}

void filter_executor::create() {
    auto config = ma_device_config_init(ma_device_type_duplex);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = 48000;
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
    base_chan.samples = std::vector(frame_count, 0.f), base_chan.sample_rate = 48000;
    for (auto& chan : chans.chans) {
        chan = base_chan;
    }

    execute_all(chans);

    const auto& l_samples = chans.chans[out_l_chan].samples;
    const auto& r_samples = chans.chans[out_r_chan].samples;
    sb_ASSERT(l_samples.size() >= frame_count);
    sb_ASSERT(r_samples.size() >= frame_count);
    for (uint32 i = 0; i < frame_count; ++i) {
        ::memcpy(out + i * 2 + 0, &l_samples[i], sizeof(float32));
        ::memcpy(out + i * 2 + 1, &r_samples[i], sizeof(float32));
    }
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
