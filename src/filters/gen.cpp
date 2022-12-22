#include "gen.h"

#include "space.h"
#include "widget.h"

#define _USE_MATH_DEFINES
#include <math.h>

std::unique_ptr<filter_base> fltr_gen_sine() {
    struct data {
        uint8 chan;
        float32 ampl;
        float32 freq;
    };

    auto f = filter<data, none>{
        .name = "SINE",
        .data = {0, 0.1f, 440.f},
        .size =
            [](auto&) {
                return vector2<uint32>{25, 7};
            },
        .draw =
            [](auto& data, auto& s) {
                s.set_rtl(true);
                ui_chan_sel(s, data.chan);
                s.write("Channel OUT ");

                s.set_rtl(false);
                s.next_line();
                s.write("Amplitude: ");
                ui_float(s, 0.1f, data.ampl);

                s.next_line();
                s.write("Frequency: ");
                ui_float_ran(s, 0.f, INFINITY, 1.f, data.freq);

                ui_viz_sine(s, nvgRGB(255, 0, 0), 3, s.rect().size.x, data.ampl, data.freq);
            },
        .update = [](auto& data, const auto& in) {},
        .apply = [x = float64{0.0}](const auto& data, auto& chans) mutable -> none {
            for (auto& s : chans.chans[data.chan].samples) {
                s = data.ampl * std::sin(2.0 * M_PI * x);
                x += 1.0 / (static_cast<float64>(chans.chans[data.chan].sample_rate) /
                            static_cast<float64>(data.freq));
            }
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
