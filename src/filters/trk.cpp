#include "trk.h"

#include "widget.h"
#include "space.h"
#include "executor.h"

#include <miniaudio.h>
#include <optional>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <cmath>

std::unique_ptr<filter_base> fltr_trk_sampler() {
    struct data : fd_chan_in<1>, fd_chan_out<2> {
        std::string path;
        std::string loaded_path;

        bool loaded = false;

        simd_vec<sample> left_samples;
        simd_vec<sample> right_samples;
        uint64 length = 0;

        int32 active_splice = 0;
        std::vector<int32> splices = {0};

        int32 viewer_min = 0;
        int32 viewer_max = 0;
    };

    struct out {
        std::optional<simd_vec<sample>> left_samples;
        std::optional<simd_vec<sample>> right_samples;
        uint64 length = 0;
    };

    struct locals {
        std::string path;
        ma_decoder decoder;
        bool loaded = false;
        uint64 length = 0;

        uint64 note_length = 0;
    };

    auto f = filter<data, out>{
        .name = "TRK-SAMPLER",
        .kind = filter_kind::trk,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{40, 13};
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

                s.next_line();
                ui_int_ran(s, 0, d.viewer_max, 4800, 4, d.viewer_min);
                s.set_rtl(true);
                ui_int_ran(s, d.viewer_min, d.length, 4800, 4, d.viewer_max);
                s.set_rtl(false);

                const auto step = d.length > 4800 ? d.length / 4800 : 1;
                ui_viz_sample(
                    s, nvgRGB(20, 100, 255), 2, d.length, step, d.viewer_min, d.viewer_max, d.left_samples,
                    d.active_splice, d.splices);
                ui_viz_sample(
                    s, nvgRGB(20, 100, 255), 2, d.length, step, d.viewer_min, d.viewer_max, d.right_samples,
                    d.active_splice, d.splices);

                ui_int_ran(s, 0, d.splices.size() - 1, 1, 2, d.active_splice);
                s.set_rtl(true);
                if (s.write_button("Add")) {
                    d.active_splice = d.splices.size();
                    d.splices.push_back(0);
                }

                const auto splice_step = (d.viewer_max - d.viewer_min) / 100;

                s.next_line();
                s.set_rtl(false);
                ui_int_ran(s, 0, d.length, splice_step, 6, d.splices[d.active_splice]);
            },
        .update =
            [](data& d, const out& o) {
                if (o.left_samples && o.right_samples) {
                    d.left_samples = o.left_samples.value();
                    d.right_samples = o.right_samples.value();

                    d.viewer_min = 0;
                    d.viewer_max = o.length;
                }

                d.length = o.length;
            },
        .apply = [l = locals{}](const data& d, channels& chans) mutable -> out {
            auto o = out{};
            o.length = l.length;

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
                    o.length = n_frames;
                    l.length = n_frames;
                    auto sams = std::vector<sample>{};
                    sams.resize(2 * n_frames, 0.f);
                    ma_decoder_read_pcm_frames(&l.decoder, sams.data(), n_frames, nullptr);
                    ma_decoder_seek_to_pcm_frame(&l.decoder, 0);

                    const auto step = n_frames / 4800;

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
                l.length = 0;
                o.left_samples = simd_vec<sample>{};
                o.right_samples = o.left_samples;
                o.length = 0;
                return o;
            }

            const auto& bytes_in = chans.byte_chans[d.chan_in];
            if (bytes_in.mode != byte_mode::tracker) {
                return o;
            }

            auto sams = std::vector<sample>{};
            sams.resize(2 * IO_FRAME_COUNT, 0.f);

            if (!bytes_in.bytes.empty()) {
                const auto note = *reinterpret_cast<const tracker_note*>(&bytes_in.bytes[0]);

                switch (note.mode) {
                case tracker_note_mode::on: {
                    if (note.index < d.splices.size()) {
                        const auto frame = d.splices[note.index];
                        const auto length = note.index == d.splices.size() - 1
                                                ? d.length - frame
                                                : d.splices[note.index + 1] - frame;
                        ma_decoder_seek_to_pcm_frame(&l.decoder, frame);
                        l.note_length = length;
                    }
                    break;
                }
                case tracker_note_mode::off: {
                    l.note_length = 0;
                    break;
                }
                default:
                    break;
                }
            }

            if (l.note_length > 0) {
                const auto readlen = std::min(l.note_length, (uint64)IO_FRAME_COUNT);
                ma_decoder_read_pcm_frames(&l.decoder, sams.data(), readlen, nullptr);
                l.note_length -= readlen;
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
    };

    return make_filter(std::move(f));
}

std::unique_ptr<filter_base> fltr_trk_tracker() {
    struct data : fd_chan_out<1> {
        std::array<tracker_note, 16> notes = fill_array<tracker_note, 16>({});
        size_t active_note;
    };

    struct out {
        size_t active_note;
    };

    struct locals {
        size_t t_m = 0;
        size_t i = 0;
    };

    auto f = filter<data, out>{
        .name = "TRK-TRACKER",
        .kind = filter_kind::trk,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{30, 18};
            },
        .draw =
            [](data& d, space& s) {
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Byte Channel TRK OUT ");

                for (uint32 i = 0; i < 16; ++i) {
                    s.next_line();
                    s.set_rtl(false);
                    s.set_color(i == d.active_note ? s.config().colors.yellow : s.config().colors.faint);
                    s.write(fmt::format("{:02} ", i));
                    ui_tracker_note(s, d.notes[i]);
                }
            },
        .update = [](data& d, const out& o) { d.active_note = o.active_note; },
        .apply = [l = locals{}](const data& d, channels& chans) mutable -> out {
            auto& bytes_out = chans.byte_chans[d.chan_out];
            bytes_out.mode = byte_mode::tracker;

            const auto bpm = 200.f;
            const auto bps = bpm / 60.f;
            const auto bpn = 0.5f;

            const auto spb = static_cast<float32>(IO_SAMPLE_RATE) / bps;
            const auto spn = static_cast<uint32>(spb * bpn);

            if (l.t_m > IO_FRAME_COUNT) {
                l.t_m -= IO_FRAME_COUNT;
                return {l.i};
            } else {
                l.t_m = 0;
                l.i = (l.i + 1) % 16;
            }

            if (l.t_m == 0) {
                const auto p_note = reinterpret_cast<const uint8*>(&d.notes[l.i]);
                bytes_out.bytes.insert(bytes_out.bytes.end(), p_note, p_note + sizeof(tracker_note));
                l.t_m = spn;
            }

            return {l.i};
        },
    };

    return make_filter(std::move(f));
}
