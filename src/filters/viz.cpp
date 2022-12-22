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

    auto f = filter<data, out> {
        .name = "WAVEFORM", .data = {0, 1.f, {}},
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
