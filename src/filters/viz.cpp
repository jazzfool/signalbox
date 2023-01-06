#include "viz.h"

#include "widget.h"
#include "space.h"
#include "executor.h"

#include <float.h>

std::unique_ptr<filter_base> fltr_viz_waveform() {
    struct data : fd_chan_in<1> {
        float32 scale = 1.f;
        float32 offset = 0.f;
        simd_vec<sample> samples;
    };

    struct out {
        simd_vec<sample> samples;
    };

    auto f = filter<data, out>{
        .name = "WAVEFORM",
        .kind = filter_kind::viz,
        .data = {},
        .size =
            [](auto&) {
                return vector2<uint32>{30, 6};
            },
        .draw =
            [](data& data, space& s) {
                ui_chan_sel(s, data.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.write("Scale: ");
                ui_float(s, 0.1f, data.scale);

                s.next_line();
                s.write("Offset: ");
                ui_float(s, 0.1f, data.offset);

                ui_viz_wf(s, nvgRGB(0, 255, 0), 3, data.scale, data.offset, data.samples);
            },
        .update =
            [](data& data, const out& in) {
                //
                data.samples.assign(in.samples.begin(), in.samples.end());
            },
        .apply = [](const data& d, channels& chans) -> out {
            auto o = out{};
            o.samples = chans.chans[d.chan_in].samples;
            return o;
        },
        .encode =
            [](const data& d, std::ostream& os) {
                d.encode_chan_in(os);

                enc_encode_one<float32>(os, d.scale);
                enc_encode_one<float32>(os, d.offset);
            },
        .decode =
            [](data& d, std::istream& is) {
                d.decode_chan_in(is);

                d.scale = enc_decode_one<float32>(is);
                d.offset = enc_decode_one<float32>(is);
            }};

    return std::make_unique<filter_fwd<data, out>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_viz_vu() {
    struct data : fd_chan_in<1> {
        float32 peak = 0.f;
        float32 meter = 0.f;
    };

    struct out {
        float32 meter = 0.f;
    };

    auto f = filter<data, out>{
        .name = "VU",
        .kind = filter_kind::viz,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{40, 3};
            },
        .draw =
            [](data& d, space& s) {
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
            o.meter = -FLT_MAX;
            for (auto s : chans.chans[d.chan_in].samples) {
                o.meter = std::max(std::abs(s), o.meter);
            }
            return o;
        },
    };

    return std::make_unique<filter_fwd<data, out>>(std::move(f));
}
