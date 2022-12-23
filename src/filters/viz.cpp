#include "viz.h"

#include "widget.h"
#include "space.h"
#include "executor.h"

std::unique_ptr<filter_base> fltr_viz_waveform() {
    struct data {
        uint8 chan1;
        float32 ampl;
        std::vector<sample> samples;
    };

    struct out {
        std::array<sample, IO_FRAME_COUNT> samples;
    };

    auto f = filter<data, out>{
        .name = "WAVEFORM",
        .data = {0, 1.f, {}},
        .size =
            [](auto&) {
                return vector2<uint32>{30, 6};
            },
        .draw =
            [](auto& data, auto& s) {
                ui_chan_sel(s, data.chan1);
                s.write(" Channel IN");

                s.next_line();
                s.write("Amplify: ");
                ui_float(s, 0.1f, data.ampl);

                ui_viz_wf(s, nvgRGB(0, 255, 0), 3, data.ampl, data.samples);
            },
        .update =
            [](auto& data, const auto& in) {
                //
                data.samples.assign(in.samples.begin(), in.samples.end());
            },
        .apply = [](const auto& data, auto& chans) -> out {
            auto o = out{};
            std::copy_n(chans.chans[data.chan1].samples.begin(), IO_FRAME_COUNT, o.samples.begin());
            return o;
        },
    };

    return std::make_unique<filter_fwd<data, out>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_viz_vu() {
    struct data {
        uint8 chan_in = 0;
        float32 peak = 0.f;
        float32 meter = 0.f;
    };

    struct out {
        float32 meter = 0.f;
    };

    auto f = filter<data, out>{
        .name = "VU",
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{35, 3};
            },
        .draw = [](data& d, space& s) {
            ui_chan_sel(s, d.chan_in);
            s.write(" Channel IN");

            ui_viz_meter(s, d.meter);
        },
        .update =
            [](data& d, const out& o) {
                d.meter = o.meter;
                d.peak = std::max(d.peak, d.meter);
            },
        .apply = [](const data& d, channels& chans) -> out {
            auto o = out{};

            auto min = float32{1.f};
            auto max = float32{-1.f};

            for (auto s : chans.chans[d.chan_in].samples) {
                min = std::min(min, s);
                max = std::max(max, s);
            }

            min = std::abs(std::min(0.f, min));
            max = std::max(0.f, max);

            o.meter = std::max(min, max);

            return o;
        },
    };

    return std::make_unique<filter_fwd<data, out>>(std::move(f));
}
