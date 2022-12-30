#include "tfm.h"

#include "widget.h"
#include "space.h"
#include "sbfft.h"
#include "sse.h"

#include <complex>
#include <algorithm>

std::unique_ptr<filter_base> fltr_tfm_fft() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {
        float32 min_hz = 100.f;
        float32 max_hz = 20000.f;
        bool full_enc = true;
    };

    auto f = filter<data, none>{
        .name = "FFT",
        .kind = filter_kind::misc,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{30, 5};
            },
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
                s.write("Encoding: ");
                auto i_enc = static_cast<uint32>(d.full_enc);
                static const std::string enc_enum[2] = {"Norm", "Full"};
                ui_enum_sel(s, enc_enum, i_enc);
                d.full_enc = static_cast<bool>(i_enc);

                s.next_line();
                if (!d.full_enc) {
                    ui_float_ran(s, 0.f, d.max_hz, 100.f, d.min_hz);
                    s.write(" Hz .. ");
                    ui_float_ran(s, d.min_hz, 40000.f, 100.f, d.max_hz);
                    s.write(" Hz");
                }
            },
        .update = [](data&, const none&) {},
        .apply = [](const data& d, channels& chans) -> none {
            const auto& in = chans.chans[d.chan_in].samples;
            const auto fs = static_cast<float32>(chans.chans[d.chan_in].sample_rate);
            auto& out = chans.chans[d.chan_out].samples;

            const auto fft = r2c_fft(in);

            if (d.full_enc) {
                const auto fft_reals = fft.whole().as_real();
                out.assign(fft_reals.begin(), fft_reals.end());
            } else {
                const auto fft_sz = static_cast<float32>(fft.size());
                const auto min = static_cast<size_t>(std::floor(d.min_hz * fft_sz / fs));
                const auto max = static_cast<size_t>(std::ceil(d.max_hz * fft_sz / fs));
                const auto fft_slice = fft.slice(min, max - min);
                out.resize(fft_slice.size(), 0.f);
                std::transform(
                    fft_slice.begin(), fft_slice.end(), out.begin(), [](const std::complex<float32>& c) {
                        auto x = std::sqrt(c.real() * c.real() + c.imag() * c.imag());
                        sb_ASSERT(x >= 0.f);
                        return x;
                    });
            }

            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_tfm_ifft() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {};

    auto f = filter<data, none>{
        .name = "IFFT",
        .kind = filter_kind::misc,
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

            if (in.size() % 2 != 0) {
                return {};
            }

            const auto complex_in = in.whole().as_complex();
            const auto ifft = c2r_ifft(complex_in);
            out.resize(ifft.size(), 0.f);
            simd_mul1(ifft, out, 1.f / ifft.size());

            return {};
        }};

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
            const auto& in_v_a = chans.chans[d.chan_in[0]].samples;
            const auto& in_v_b = chans.chans[d.chan_in[1]].samples;
            auto& out = chans.chans[d.chan_out].samples;

            const auto sz = std::min(in_v_a.size(), in_v_b.size());

            const auto in_a = in_v_a.slice(0, sz);
            const auto in_b = in_v_b.slice(0, sz);

            const auto a_fft = r2c_fft(in_a, sz * 2 - 1);
            auto b_fft = r2c_fft(in_b, sz * 2 - 1);
            const auto fft_sz = b_fft.size();
            sb_ASSERT_EQ(a_fft.size(), b_fft.size());

            // x-correl = ifft(fft(a) * conj(fft(b)))

            auto i = size_t{0};
            for (; i < fft_sz / 2 * 2; i += 2) {
                static const auto _conj = _mm_setr_ps(1.f, -1.f, 1.f, -1.f);
                _mm_store_ps((float32*)(&b_fft[i]), _mm_mul_ps(_mm_load_ps((float32*)(&b_fft[i])), _conj));
            }
            for (; i < fft_sz; ++i) {
                b_fft[i] = std::conj(b_fft[i]);
            }

            i = 0;
            for (; i < fft_sz / 2 * 2; i += 2) {
                static const auto _fac = _mm_setr_ps(-1.f, 1.f, -1.f, 1.f);
                auto c1c2 = _mm_load_ps((float32*)(&a_fft[i]));
                auto c3c4 = _mm_load_ps((float32*)(&b_fft[i]));

                const auto shufc3c4 = _mm_shuffle_ps(c3c4, c3c4, 0xA0);
                c3c4 = _mm_shuffle_ps(c3c4, c3c4, 0xF5);
                auto shufc1c2 = _mm_shuffle_ps(c1c2, c1c2, 0xB1);

                c1c2 = _mm_mul_ps(c1c2, shufc3c4);
                shufc1c2 = _mm_mul_ps(shufc1c2, c3c4);
                shufc1c2 = _mm_mul_ps(shufc1c2, _fac);
                c1c2 = _mm_add_ps(c1c2, shufc1c2);

                _mm_store_ps((float32*)(&b_fft[i]), c1c2);
            }
            for (; i < fft_sz; ++i) {
                b_fft[i] = a_fft[i] * b_fft[i];
            }

            out = c2r_ifft(b_fft);

            // normalize
            simd_mul1(out, out, 1.f / simd_max(out));

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
            // hann window
            static const auto _0_5 = _mm_set1_ps(0.5f);
            static const auto _1_0 = _mm_set1_ps(1.f);
            static const auto _2_pi = _mm_set1_ps(M_PI * 2.f);
            const auto sz_inv = 1.f / static_cast<float32>(in.size() - 1);
            const auto _sz_inv = _mm_set1_ps(sz_inv);
            auto i = size_t{0};
            /*
            for (; i < in.size() / 4 * 4; i += 4) {
                const auto is = _mm_mul_ps(_mm_setr_ps(i, i + 1, i + 2, i + 3), _sz_inv);
                const auto window = _mm_mul_ps(_0_5, _mm_sub_ps(_1_0, simd_cos(_mm_mul_ps(_2_pi, is))));
                _mm_store_ps(&out[i], _mm_mul_ps(_mm_load_ps(&in[i]), window));
            }
            */
            for (; i < in.size(); ++i) {
                const auto window = 0.5f * (1.f - std::cos(2.f * M_PI * (static_cast<float32>(i) * sz_inv)));
                out[i] = in[i] * window;
            }

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

            const auto sz = std::min(in_a.size(), in_b.size());
            out.resize(sz);
            auto i = size_t{0};
            for (; i < sz / 4 * 4; i += 4) {
                _mm_store_ps(&out[i], _mm_sub_ps(_mm_load_ps(&in_a[i]), _mm_load_ps(&in_b[i])));
            }
            for (; i < sz; ++i) {
                out[i] = in_a[i] - in_b[i];
            }

            return {};
        }};

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
