#include "gen.h"

#include "space.h"
#include "widget.h"
#include "executor.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <miniaudio.h>
#include <filesystem>
#include <spdlog/fmt/fmt.h>

std::unique_ptr<filter_base> fltr_gen_sine() {
    struct data : fd_chan_out<1> {
        float32 ampl = 0.1f;
        float32 freq = 440.f;
    };

    auto f = filter<data, none>{
        .name = "SINE",
        .kind = filter_kind::gen,
        .data = {},
        .size =
            [](auto&) {
                return vector2<uint32>{25, 7};
            },
        .draw =
            [](auto& data, auto& s) {
                s.set_rtl(true);
                ui_chan_sel(s, data.chan_out);
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
        .apply = [x = float64{0.0}](const data& data, channels& chans) mutable -> none {
            for (auto& s : chans.chans[data.chan_out].samples) {
                s = data.ampl * std::sin(2.0 * M_PI * x);
                x += 1.0 / (static_cast<float64>(chans.chans[data.chan_out].sample_rate) /
                            static_cast<float64>(data.freq));
            }
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}

std::unique_ptr<filter_base> fltr_gen_file() {
    struct data : fd_chan_out<2> {
        std::string path;
        std::string loaded_path;

        bool loaded = false;
        bool looping = true;
        bool muted = false;
        bool paused = false;

        uint32 progress = 0;
        uint32 restart_sema = 0;
    };

    struct out {
        uint64 cursor = 0;
        uint64 length = 1;
    };

    struct locals {
        std::string path;
        ma_decoder decoder;
        bool loaded = false;
        uint32 restart_sema = 0;
        uint64 length = 1;
    };

    auto f = filter<data, out>{
        .name = "FILE",
        .kind = filter_kind::gen,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{30, 7};
            },
        .draw =
            [](data& d, space& s) {
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out[0]);
                s.write("Channel Left OUT ");

                s.next_line();
                ui_chan_sel(s, d.chan_out[1]);
                s.write("Channel Right OUT ");

                s.next_line();
                s.set_rtl(false);
                s.write("File: ");
                ui_text_in(s, !d.loaded ? s.config().colors.red : s.config().colors.fg, d.path, 24);

                if (s.focus != &d.path && d.path != d.loaded_path) {
                    d.loaded = std::filesystem::exists(d.path) && std::filesystem::is_regular_file(d.path);
                    d.loaded_path = d.path;
                }

                s.next_line();
                s.write(fmt::format("{:02}%", d.progress));

                s.next_line();
                s.set_color(s.config().colors.media_control);
                if (s.write_button("Restart")) {
                    ++d.restart_sema;
                }
                s.set_rtl(true);
                s.set_color(s.config().colors.media_control);
                if (s.write_button(d.looping ? "No Looping" : "Looping")) {
                    d.looping = !d.looping;
                }

                s.next_line();
                s.set_color(s.config().colors.media_control);
                if (s.write_button(d.paused ? "Play" : "Pause")) {
                    d.paused = !d.paused;
                }
                s.set_rtl(false);
                s.set_color(s.config().colors.media_control);
                if (s.write_button(d.muted ? "Unmute" : "Mute")) {
                    d.muted = !d.muted;
                }
            },
        .update =
            [](data& d, const out& o) {
                d.progress = static_cast<uint32>(
                    std::round(100.f * static_cast<float32>(o.cursor) / static_cast<float32>(o.length)));
            },
        .apply = [l = locals{}](const data& d, channels& chans) mutable -> out {
            if (d.loaded && d.loaded_path != l.path) {
                l.path = d.loaded_path;
                if (l.loaded) {
                    ma_decoder_uninit(&l.decoder);
                }
                auto config = ma_decoder_config_init(ma_format_f32, 2, IO_SAMPLE_RATE);
                auto res = ma_decoder_init_file(l.path.c_str(), &config, &l.decoder);
                l.loaded = res == MA_SUCCESS;
                if (l.loaded) {
                    ma_decoder_get_length_in_pcm_frames(&l.decoder, &l.length);
                }
            } else if (!d.loaded || !l.loaded) {
                if (l.loaded) {
                    ma_decoder_uninit(&l.decoder);
                }
                l.loaded = false;
                l.length = 1;
                return {};
            }

            if (d.restart_sema != l.restart_sema) {
                l.restart_sema = d.restart_sema;
                ma_decoder_seek_to_pcm_frame(&l.decoder, 0);
            }

            auto o = out{};
            ma_decoder_get_cursor_in_pcm_frames(&l.decoder, &o.cursor);
            o.length = l.length;

            if (d.paused) {
                return o;
            }

            auto sams = std::vector<sample>{};
            sams.resize(2 * IO_FRAME_COUNT, 0.f);

            const auto res = ma_decoder_read_pcm_frames(&l.decoder, sams.data(), IO_FRAME_COUNT, nullptr);
            if (res == MA_AT_END && d.looping) {
                ma_decoder_seek_to_pcm_frame(&l.decoder, 0);
                ma_decoder_read_pcm_frames(&l.decoder, sams.data(), IO_FRAME_COUNT, nullptr);
            }

            if (d.muted) {
                return o;
            }

            auto& left = chans.chans[d.chan_out[0]].samples;
            auto& right = chans.chans[d.chan_out[1]].samples;

            left.resize(IO_FRAME_COUNT, 0.f);
            right.resize(IO_FRAME_COUNT, 0.f);

            for (uint32 i = 0; i < IO_FRAME_COUNT; ++i) {
                left[i] = sams[i * 2 + 0];
                right[i] = sams[i * 2 + 1];
            }

            return o;
        },
        .encode =
            [](const data& d, std::ostream& os) {
                d.encode_chan_out(os);

                enc_encode_string(os, d.path);
                enc_encode_string(os, d.loaded_path);
                enc_encode_one<bool>(os, d.loaded);
                enc_encode_one<bool>(os, d.looping);
                enc_encode_one<bool>(os, d.muted);
                enc_encode_one<bool>(os, d.paused);
            },
        .decode =
            [](data& d, std::istream& is) {
                d.decode_chan_out(is);

                d.path = enc_decode_string(is);
                d.loaded_path = enc_decode_string(is);
                d.loaded = enc_decode_one<bool>(is);
                d.looping = enc_decode_one<bool>(is);
                d.muted = enc_decode_one<bool>(is);
                d.paused = enc_decode_one<bool>(is);
            },
    };

    return std::make_unique<filter_fwd<data, out>>(std::move(f));
}
