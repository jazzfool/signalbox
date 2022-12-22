#include "board.h"
#include "widget.h"

#include <spdlog/fmt/fmt.h>
#define _USE_MATH_DEFINES
#include <math.h>

int main() {
    struct filter0_data {
        uint8 chan1;
        float32 ampl;
        float32 freq;
    };

    struct filter1_data {
        uint8 chan1;
        float32 ampl;
        std::vector<sample> samples;
    };

    struct filter1_out {
        std::array<sample, IO_FRAME_COUNT> samples;
    };

    struct filter2_data {
        uint8 chan1;
        uint8 chan2;
        uint8 chan3;
    };

    board{}
        .create()
        .add_filter(filter<filter0_data, none>{
            .name = "SINE",
            .data = {0, 0.1f, 1.f},
            .size =
                [](auto&) {
                    return vector2<uint32>{25, 7};
                },
            .draw =
                [](auto& data, auto& s) {
                    s.set_rtl(true);
                    ui_chan_sel(s, data.chan1);
                    s.write("Channel A OUT ");

                    s.set_rtl(false);
                    s.next_line();
                    s.write("Amplitude: ");
                    ui_float(s, 0.1f, data.ampl);

                    s.next_line();
                    s.write("Frequency: ");
                    ui_float_ran(s, 0.f, INFINITY, 1.f, data.freq);

                    ui_viz_sine(s, nvgRGB(255, 0, 0), 3, 500, data.ampl, data.freq);
                },
            .update = [](auto& data, const auto& in) {},
            .apply = [x = float64{0.0}](const auto& data, auto& chans) mutable -> none {
                for (auto& s : chans.chans[data.chan1].samples) {
                    s = data.ampl * std::sin(2.0 * M_PI * x);
                    x += 1.0 / (static_cast<float64>(chans.chans[data.chan1].sample_rate) /
                                static_cast<float64>(data.freq));
                }
                return {};
            },
        })
        .add_filter(filter<filter1_data, filter1_out>{
            .name = "WAVEFORM",
            .data = {0, 1.f, {}},
            .size =
                [](auto&) {
                    return vector2<uint32>{30, 6};
                },
            .draw =
                [](auto& data, auto& s) {
                    ui_chan_sel(s, data.chan1);
                    s.write(" Channel A IN");

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
            .apply = [](const auto& data, auto& chans) -> filter1_out {
                auto out = filter1_out{};
                std::copy_n(chans.chans[data.chan1].samples.begin(), IO_FRAME_COUNT, out.samples.begin());
                return out;
            },
        })
        .add_filter(filter<filter2_data, none>{
            .name = "SPLIT",
            .data = {0, 0, 1},
            .size =
                [](auto&) {
                    return vector2<uint32>{20, 4};
                },
            .draw =
                [](auto& data, auto& s) {
                    ui_chan_sel(s, data.chan1);
                    s.write(" Channel A IN");

                    s.next_line();
                    s.set_rtl(true);
                    ui_chan_sel(s, data.chan2);
                    s.write("Channel B OUT ");

                    s.next_line();
                    ui_chan_sel(s, data.chan3);
                    s.write("Channel C OUT ");
                },
            .update = [](auto& data, const auto& in) {},
            .apply = [](const auto& data, auto& chans) -> none {
                const auto in = chans.chans[data.chan1];
                for (auto chan : {data.chan2, data.chan3}) {
                    if (chan == data.chan1)
                        continue;
                    chans.chans[chan] = in;
                }
                return {};
            },
        })
        .run_loop()
        .destroy();
}