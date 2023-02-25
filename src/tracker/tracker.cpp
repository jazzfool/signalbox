#include "tracker.h"

#include "ui.h"

void Tracker::create() {
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
        if (playback_devs[i].isDefault)
            m_playback_dev_idx = i;
        m_playback_dev_names.emplace_back(playback_devs[i].name);
        m_playback_dev_ids.push_back(playback_devs[i].id);
    }

    m_capture_dev_names.reserve(capture_dev_count);
    m_capture_dev_ids.reserve(capture_dev_count);
    for (uint32 i = 0; i < capture_dev_count; ++i) {
        if (capture_devs[i].isDefault)
            m_capture_dev_idx = i;
        m_capture_dev_names.emplace_back(capture_devs[i].name);
        m_capture_dev_ids.push_back(capture_devs[i].id);
    }
}

void Tracker::destroy() {
    ma_device_uninit(&m_device);
    ma_context_uninit(&m_context);
}

void Tracker::ui() {
    static std::vector<std::string_view> enums = {"Short Option", "Really Long Option"};
    static uint32 i = 0;
    static std::string txt = "Hello!";

    using namespace ui;
    Vector2_F32 sz;
    // clang-format off
    vstack(Spacing{10.f})
        (hstack(Spacing{5.f})
            (text("Label")())
            (button(
                text("Value")(),
                onclick([]{ spdlog::info("click!"); }))()))
        (text("World!")())
        (dropdown(enums, i)())
        (dropdown(enums, i)())
        (textbox(txt)())
        (sz)
        ({{0.f, 0.f}, {200.f, 200.f}});
    // clang-format on
}

void ma_data_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    reinterpret_cast<Tracker*>(device->pUserData)
        ->data_callback(output, input, static_cast<uint32>(frame_count));
}

void Tracker::create_device() {
    auto config = ma_device_config_init(ma_device_type_duplex);

    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.playback.pDeviceID = &m_playback_dev_ids[m_playback_dev_idx];

    config.capture.format = ma_format_f32;
    config.capture.channels = 2;
    config.capture.pDeviceID = &m_capture_dev_ids[m_capture_dev_idx];
    config.capture.shareMode = ma_share_mode_shared;

    config.sampleRate = Tracker::SAMPLE_RATE;
    config.periodSizeInFrames = Tracker::FRAME_COUNT;
    config.dataCallback = ma_data_callback;
    config.pUserData = this;

    ma_device_init(nullptr, &config, &m_device);
    ma_device_start(&m_device);
}

void Tracker::data_callback(void* output, const void* input, uint32 frame_count) {
}
