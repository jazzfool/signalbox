#include "fir.h"

#include "space.h"
#include "widget.h"

#include <algorithm>
#include <miniaudio.h>

std::unique_ptr<filter_base> fltr_fir_delay() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {
        int32 delay = 100;
    };

    struct locals {
        std::vector<sample> line;
        uint32 cursor = 0;
    };

    auto f = filter<data, none>{
        .name = "DELAY",
        .kind = filter_kind::fir,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{25, 4};
            },
        .draw =
            [](data& data, space& s) {
                ui_chan_sel(s, data.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, data.chan_out);
                s.write("Channel OUT ");

                s.next_line();
                s.set_rtl(false);
                s.write("Delay: ");
                ui_int_ran(s, 1, INT32_MAX, 10, 5, data.delay);
            },
        .update =
            [](data& data, const none& out) {

            },
        .apply = [l = locals{}](const data& data, channels& chans) mutable -> none {
            if (data.chan_in == data.chan_out)
                return {};

            if (l.line.size() != data.delay) {
                l.line = std::vector<sample>(data.delay, 0.f);
                l.cursor = 0;
            }

            for (size_t i = 0;
                 i < std::min(
                         chans.chans[data.chan_in].samples.size(), chans.chans[data.chan_out].samples.size());
                 ++i) {
                chans.chans[data.chan_out].samples[i] = l.line[l.cursor];
                l.line[l.cursor++] = chans.chans[data.chan_in].samples[i];
                if (l.cursor >= l.line.size())
                    l.cursor = 0;
            }

            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_fir_gain() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {
        float32 gain_db = 0.f;
    };

    auto f = filter<data, none>{
        .name = "GAIN",
        .kind = filter_kind::fir,
        .data = {},
        .size = [](const data& d) { return vector2<float32>(25, 4); },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Channel OUT ");

                s.next_line();
                s.set_rtl(false);
                s.write("Gain: ");
                ui_float(s, 0.1f, d.gain_db);
                s.write(" dB");
            },
        .update = [](data& d, const none&) {},
        .apply = [](const data& d, channels& chans) -> none {
            auto& in = chans.chans[d.chan_in].samples;
            auto& out = chans.chans[d.chan_out].samples;
            out.resize(in.size(), 0.f);
            const auto gain = ma_volume_db_to_linear(d.gain_db);
            std::transform(
                in.begin(), in.end(), out.begin(), [gain](sample s) -> sample { return s * gain; });
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
