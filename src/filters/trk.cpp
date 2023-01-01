#include "trk.h"

#include "widget.h"
#include "space.h"
#include "executor.h"

#include <miniaudio.h>
#include <optional>
#include <filesystem>

std::unique_ptr<filter_base> fltr_trk_sampler() {
    struct data : fd_chan_in<1>, fd_chan_out<2> {
        std::string path;
        std::string loaded_path;

        bool loaded = false;

        simd_vec<sample> left_samples;
        simd_vec<sample> right_samples;

        int32 viewer_min = 0;
        int32 viewer_max = 0;
    };

    struct out {
        std::optional<simd_vec<sample>> left_samples;
        std::optional<simd_vec<sample>> right_samples;
    };

    struct locals {
        std::string path;
        ma_decoder decoder;
        bool loaded = false;
    };

    auto f = filter<data, out>{
        .name = "TRK-SAMPLER",
        .kind = filter_kind::trk,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{40, 10};
            },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in);
                s.write(" Byte Channel TRK IN");

                s.next_line();
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

                const auto samples_sz = d.left_samples.size();
                const auto step = samples_sz > 1000 ? samples_sz / 100 : 1;
                s.next_line();
                ui_int_ran(s, 0, d.viewer_max, step, 4, d.viewer_min);
                s.set_rtl(true);
                ui_int_ran(s, d.viewer_min, samples_sz, step, 4, d.viewer_max);
                s.set_rtl(false);

                ui_viz_wf(
                    s, nvgRGB(20, 100, 255), 2, 1.f, 0.f,
                    d.left_samples.slice(d.viewer_min, d.viewer_max - d.viewer_min));
                ui_viz_wf(
                    s, nvgRGB(20, 100, 255), 2, 1.f, 0.f,
                    d.right_samples.slice(d.viewer_min, d.viewer_max - d.viewer_min));
            },
        .update =
            [](data& d, const out& o) {
                if (o.left_samples && o.right_samples) {
                    d.left_samples = o.left_samples.value();
                    d.right_samples = o.right_samples.value();

                    d.viewer_min = 0;
                    d.viewer_max = d.left_samples.size();
                }
            },
        .apply = [l = locals{}](const data& d, channels& chans) mutable -> out {
            auto o = out{};

            if (d.loaded && d.loaded_path != l.path) {
                l.path = d.loaded_path;
                if (l.loaded) {
                    ma_decoder_uninit(&l.decoder);
                }
                auto config = ma_decoder_config_init(ma_format_f32, 2, IO_SAMPLE_RATE);
                auto res = ma_decoder_init_file(l.path.c_str(), &config, &l.decoder);
                l.loaded = res == MA_SUCCESS;
                if (l.loaded) {
                    auto n_frames = uint64{0};
                    ma_decoder_get_length_in_pcm_frames(&l.decoder, &n_frames);
                    auto sams = std::vector<sample>{};
                    sams.resize(2 * n_frames, 0.f);
                    ma_decoder_read_pcm_frames(&l.decoder, sams.data(), n_frames, nullptr);
                    ma_decoder_seek_to_pcm_frame(&l.decoder, 0);

                    const auto step = n_frames > 100000 ? n_frames / 10000 : 1;

                    o.left_samples = simd_vec<sample>{};
                    o.left_samples->resize(n_frames / step, 0.f);
                    o.right_samples = o.left_samples;

                    for (auto i = uint32{0}; i < n_frames / step; ++i) {
                        const auto j = i * step;
                        (*o.left_samples)[i] = sams[j * 2 + 0];
                        (*o.right_samples)[i] = sams[j * 2 + 1];
                    }
                }
            } else if (!d.loaded || !l.loaded) {
                if (l.loaded) {
                    ma_decoder_uninit(&l.decoder);
                }
                l.loaded = false;
                o.left_samples = simd_vec<sample>{};
                o.right_samples = o.left_samples;
                return o;
            }

            auto sams = std::vector<sample>{};
            sams.resize(2 * IO_FRAME_COUNT, 0.f);

            const auto res = ma_decoder_read_pcm_frames(&l.decoder, sams.data(), IO_FRAME_COUNT, nullptr);

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
    };

    return make_filter(std::move(f));
}
