#include "tfm.h"

#include "widget.h"
#include "space.h"
#include "sbfft.h"

#include <complex>
#include <algorithm>

std::unique_ptr<filter_base> fltr_tfm_fft() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {};

    auto f = filter<data, none>{
        .name = "FFT",
        .kind = filter_kind::misc,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{25, 3};
            },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Channel OUT ");
            },
        .update = [](data&, const none&) {},
        .apply = [](const data& d, channels& chans) -> none {
            const auto& in = chans.chans[d.chan_in].samples;
            auto& out = chans.chans[d.chan_out].samples;

            const auto fft = r2c_fft(in);
            out.resize(fft.size(), 0.f);
            std::transform(
                fft.begin(), fft.end(), chans.chans[d.chan_out].samples.begin(),
                [](const std::complex<float32>& c) { return c.real(); });
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_tfm_xcorrel() {
    struct data : fd_chan_in<2>, fd_chan_out<1> {};

    auto f = filter<data, none>{
        .name = "X-CORREL",
        .kind = filter_kind::misc,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{25, 4};
            },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in[0]);
                s.write(" Channel A IN");

                s.next_line();
                ui_chan_sel(s, d.chan_in[1]);
                s.write(" Channel B IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Channel OUT ");
            },
        .update = [](data&, const none&) {},
        .apply = [](const data& d, channels& chans) -> none {
            static const auto conj_kernel = simd_const<float32, 2>{{1.f, -1.f}};

            const auto& in_v_a = chans.chans[d.chan_in[0]].samples;
            const auto& in_v_b = chans.chans[d.chan_in[1]].samples;
            auto& out = chans.chans[d.chan_out].samples;

            const auto sz = std::min(in_v_a.size(), in_v_b.size());

            const auto in_a = in_v_a.slice(0, sz);
            const auto in_b = in_v_b.slice(0, sz);

            const auto v1 = simd_vec<float32>{
                r2c_fft(in_a, sz * 2 - 1).slice().real_slice<float32>() *
                (conj_kernel * r2c_fft(in_b, sz * 2 - 1).slice().real_slice<float32>())};

            out = c2r_ifft(v1.slice().complex_slice());

            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_tfm_hann_window() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {};

    auto f = filter<data, none>{
        .name = "W-HANN",
        .kind = filter_kind::misc,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{25, 3};
            },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Channel OUT ");
            },
        .update = [](data&, const none&) {},
        .apply = [](const data& d, channels& chans) -> none {
            const auto& in = chans.chans[d.chan_in].samples;
            auto& out = chans.chans[d.chan_out].samples;

            out.resize(in.size());
            out = simd_hann_window(0.f, 1.f, in.size()) * in;

            return {};
        }};

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_tfm_sub() {
    struct data : fd_chan_in<2>, fd_chan_out<1> {};

    auto f = filter<data, none>{
        .name = "SUBTRACT",
        .kind = filter_kind::misc,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{25, 4};
            },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in[0]);
                s.write(" Channel A IN");

                s.next_line();
                ui_chan_sel(s, d.chan_in[1]);
                s.write(" Channel B IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Channel OUT ");
            },
        .update = [](data&, const none&) {},
        .apply = [](const data& d, channels& chans) -> none {
            const auto& in_a = chans.chans[d.chan_in[0]].samples;
            const auto& in_b = chans.chans[d.chan_in[1]].samples;
            auto& out = chans.chans[d.chan_out].samples;

            out.resize(std::min(in_a.size(), in_b.size()));
            out = in_a - in_b;

            return {};
        }};

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
