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
                return vector2<float32>{25, 3};
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
            const auto fft = r2c_fft(chans.chans[d.chan_in].samples);
            chans.chans[d.chan_out].samples.resize(fft.size(), 0.f);
            std::transform(
                fft.begin(), fft.end(), chans.chans[d.chan_out].samples.begin(),
                [](const std::complex<float32>& c) { return c.real(); });
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_tfm_xcorrel() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {};

    auto f = filter<data, none>{
        .name = "FFT",
        .kind = filter_kind::misc,
        .data = {},
        .size =
            [](const data&) {
                return vector2<float32>{25, 3};
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
            const auto fft = r2c_fft(chans.chans[d.chan_in].samples);
            chans.chans[d.chan_out].samples.resize(fft.size(), 0.f);
            std::transform(
                fft.begin(), fft.end(), chans.chans[d.chan_out].samples.begin(),
                [](const std::complex<float32>& c) { return c.real(); });
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
