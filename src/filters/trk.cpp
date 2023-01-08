#include "trk.h"

#include "widget.h"
#include "space.h"
#include "executor.h"
#include "enc.h"
#include "draw.h"

#include <miniaudio.h>
#include <optional>
#include <filesystem>
#include <spdlog/fmt/fmt.h>
#include <cmath>
#include <sstream>
#include <nfd.hpp>

static constexpr uint8 TRK_LINESEL = 1;

struct trk_sampler_data : fd_chan_in<1>, fd_chan_out<2> {
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

std::unique_ptr<filter_base> fltr_trk_sampler() {
    using data = trk_sampler_data;

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
                ui_filepath_in(
                    s, !d.loaded ? s.config().colors.red : s.config().colors.editable, d.path,
                    "Select audio file", 24);

                if (d.path != d.loaded_path) {
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
                    d.splices.push_back((d.viewer_min + d.viewer_max) / 2);
                }
                s.write(" ");
                if (s.write_button("Del") && d.splices.size() > 1) {
                    d.splices.erase(d.splices.begin() + d.active_splice);
                    d.active_splice =
                        d.active_splice == d.splices.size() ? d.active_splice - 1 : d.active_splice;
                }

                const auto splice_step = (d.viewer_max - d.viewer_min) / 100;

                s.next_line();
                s.set_rtl(false);
                ui_int_ran(s, 0, d.length, splice_step, 6, d.splices[d.active_splice]);

                if (d.active_splice > 0 && d.splices[d.active_splice] < d.splices[d.active_splice - 1]) {
                    std::swap(d.splices[d.active_splice], d.splices[d.active_splice - 1]);
                    --d.active_splice;
                }
                if (d.active_splice < d.splices.size() - 1 &&
                    d.splices[d.active_splice] > d.splices[d.active_splice + 1]) {
                    std::swap(d.splices[d.active_splice], d.splices[d.active_splice + 1]);
                    ++d.active_splice;
                }
                for (size_t i = 0; i < d.splices.size(); ++i) {
                    if (i > 0 && d.splices[i] == d.splices[i - 1])
                        d.splices[i] += splice_step / 2;
                }
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
                const auto line = *reinterpret_cast<const tracker_line*>(&bytes_in.bytes[0]);
                const auto note0 = line.notes[0];

                switch (get_note_mode(note0)) {
                case tracker_note_mode::on: {
                    const auto val = get_trkline_value(note0);
                    if (val < d.splices.size()) {
                        const auto frame = d.splices[val];
                        const auto length =
                            val == d.splices.size() - 1 ? d.length - frame : d.splices[val + 1] - frame;
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
        .encode =
            [](const data& d, std::ostream& os) {
                d.encode_chan_in(os);
                d.encode_chan_out(os);

                enc_encode_string(os, d.path);
                enc_encode_string(os, d.loaded_path);
                enc_encode_one<bool>(os, d.loaded);
                enc_encode_one<int32>(os, d.active_splice);
                enc_encode_span<int32>(os, d.splices);
                enc_encode_one<int32>(os, d.viewer_min);
                enc_encode_one<int32>(os, d.viewer_max);
            },
        .decode =
            [](data& d, std::istream& is) {
                d.decode_chan_in(is);
                d.decode_chan_out(is);

                d.path = enc_decode_string(is);
                d.loaded_path = enc_decode_string(is);
                d.loaded = enc_decode_one<bool>(is);
                d.active_splice = enc_decode_one<int32>(is);
                d.splices = enc_decode_vec<int32>(is);
                d.viewer_min = enc_decode_one<int32>(is);
                d.viewer_max = enc_decode_one<int32>(is);
            },
    };

    return make_filter(std::move(f));
}

std::unique_ptr<filter_base> fltr_trk_sample_browser() {
    static constexpr auto populate = [](const std::string& dir, std::vector<std::string>& out_folders,
                                        std::vector<std::string>& out_files) {
        out_folders.clear();
        out_files.clear();
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_directory()) {
                out_folders.push_back(entry.path().filename().string());
            } else if (entry.is_regular_file()) {
                const auto ext = entry.path().extension().string();
                const auto is_audio = ext == ".mp3" || ext == ".wav" || ext == ".flac";
                if (is_audio)
                    out_files.push_back(entry.path().filename().string());
            }
        }
    };

    struct data : fd_chan_out<2> {
        data() : dir{"C:/"} {
            populate(dir, folders, files);
        }

        int32 page_num = 1;
        std::string dir;
        std::vector<std::string> folders;
        std::vector<std::string> files;

        std::optional<std::string> preview_file;
        uint32 restart_sema = 0;
    };

    struct locals {
        std::string path;
        std::string loaded_path;
        ma_decoder decoder;
        bool loaded = false;
        uint32 restart_sema = 0;
    };

    auto f = filter<data, none>{
        .name = "TRK-SAMPLE-BROWSER",
        .kind = filter_kind::trk,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{35, 21};
            },
        .draw =
            [](data& d, space& s) {
                auto changed_dir = false;
                const auto fs_dir = std::filesystem::path{d.dir};

                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out[0]);
                s.write("Channel Preview OUT LEFT ");

                s.next_line();
                ui_chan_sel(s, d.chan_out[1]);
                s.write("Channel Preview OUT RIGHT ");

                s.next_line();
                s.set_rtl(false);
                if (ui_filepath_in(s, s.config().colors.editable, d.dir, "", 35, true)) {
                    changed_dir = true;
                }

                s.next_line();
                if (s.write_button("..") && fs_dir.has_parent_path()) {
                    changed_dir = true;
                    d.dir = fs_dir.parent_path().string();
                }
                s.set_rtl(true);
                const auto total = d.folders.size() + d.files.size();
                const auto num_pages = total / 16 + (total % 16 > 0 ? 1 : 0);
                ui_int_ran(s, 1, num_pages, 1, 2, d.page_num);
                s.write("Page ");
                s.set_rtl(false);

                auto i = (d.page_num - 1) * 16;
                const auto i_max = i + 16;

                for (; i < d.folders.size() && i < i_max; ++i) {
                    s.next_line();
                    s.set_color(s.config().colors.yellow);
                    if (s.write_button(d.folders[i])) {
                        changed_dir = true;
                        d.dir = (fs_dir / d.folders[i]).string();
                    }
                }

                i -= d.folders.size();
                for (; i < d.files.size() && i < i_max; ++i) {
                    s.next_line();
                    s.set_color(s.config().colors.green);
                    if (s.write_hover(d.files[i], s.color(), s.config().colors.bg)) {
                        const auto path = (fs_dir / d.files[i]).string();
                        if (s.input().mouse_just_pressed[0]) {
                            d.preview_file = path;
                            ++d.restart_sema;
                        }
                        if (s.input().mouse_just_pressed[1]) {
                            s.cb_add_filter("TRK-SAMPLER", [path](filter_base* sampler) {
                                const auto p = reinterpret_cast<trk_sampler_data*>(sampler->data());
                                p->path = path;
                            });
                        }
                    }
                }

                if (changed_dir) {
                    populate(d.dir, d.folders, d.files);
                }
            },
        .update = [](data&, const none&) {},
        .apply = [l = locals{}](const data& d, channels& chans) mutable -> none {
            if (d.preview_file && d.preview_file != l.path) {
                l.path = *d.preview_file;
                if (l.loaded) {
                    ma_decoder_uninit(&l.decoder);
                }
                auto config = ma_decoder_config_init(ma_format_f32, 2, IO_SAMPLE_RATE);
                auto res = ma_decoder_init_file(l.path.c_str(), &config, &l.decoder);
                l.loaded = res == MA_SUCCESS;
            } else if (!d.preview_file || !l.loaded) {
                if (l.loaded) {
                    ma_decoder_uninit(&l.decoder);
                }
                l.loaded = false;
                return {};
            }

            if (d.restart_sema != l.restart_sema) {
                l.restart_sema = d.restart_sema;
                ma_decoder_seek_to_pcm_frame(&l.decoder, 0);
            }

            auto sams = std::vector<sample>{};
            sams.resize(2 * IO_FRAME_COUNT, 0.f);

            ma_decoder_read_pcm_frames(&l.decoder, sams.data(), IO_FRAME_COUNT, nullptr);

            auto& left = chans.chans[d.chan_out[0]].samples;
            auto& right = chans.chans[d.chan_out[1]].samples;

            left.resize(IO_FRAME_COUNT, 0.f);
            right.resize(IO_FRAME_COUNT, 0.f);

            for (uint32 i = 0; i < IO_FRAME_COUNT; ++i) {
                left[i] = sams[i * 2 + 0];
                right[i] = sams[i * 2 + 1];
            }

            return {};
        },
        .encode =
            [](const data& d, std::ostream& os) {
                d.encode_chan_out(os);
                enc_encode_string(os, d.dir);
            },
        .decode =
            [](data& d, std::istream& is) {
                d.decode_chan_out(is);
                d.dir = enc_decode_string(is);
            },
    };

    return make_filter(std::move(f));
}

std::unique_ptr<filter_base> fltr_trk_tracker() {
    struct data : fd_chan_in<1>, fd_chan_out<1> {
        std::array<tracker_line, 256> lines = fill_array<tracker_line, 256>({});

        bool pause = false;
        float32 bpm = 200.f;
        int32 lpb = 4;
        int32 num_lines = 64;

        uint8 num_notes = 1;
        uint8 num_fx = 1;

        uint32 active_line = 0;

        ui_scroll_state scroll;
    };

    struct out {
        bool pause = false;
        uint32 active_line = 0;
    };

    struct locals {
        size_t t_m = 0;
        uint32 line = 0;
    };

    auto f = filter<data, out>{
        .name = "TRK-TRACKER",
        .kind = filter_kind::trk,
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{UINT32_MAX, 16 + 5};
            },
        .draw =
            [](data& d, space& s) {
                ui_chan_sel(s, d.chan_in);
                s.write(" Byte Channel CLK IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, d.chan_out);
                s.write("Byte Channel TRK OUT ");

                s.next_line();
                s.set_rtl(false);
                s.write("BPM: ");
                ui_float_ran(s, 1.f, 999.f, 1.f, d.bpm);
                s.write(" LPB: ");
                ui_int_ran(s, 1, 16, 1, 2, d.lpb);
                s.write(" Lines: ");
                ui_int_ran(s, d.lpb, 256, 1, 2, d.num_lines);
                s.set_rtl(true);
                s.set_color(s.config().colors.media_control);
                if (s.write_button(d.pause ? "Play" : "Pause")) {
                    d.pause = !d.pause;
                }

                s.next_line();
                s.set_rtl(false);
                s.set_color(s.config().colors.fg);
                if (s.write_button("-") && d.num_notes > 0) {
                    --d.num_notes;
                }
                if (s.write_button("+") && d.num_notes < 12) {
                    ++d.num_notes;
                }
                s.set_rtl(true);
                if (s.write_button("+") && d.num_fx < 8) {
                    ++d.num_fx;
                }
                if (s.write_button("-") && d.num_fx > 0) {
                    --d.num_fx;
                }

                static const uint32 TOP_SCROLL_PADDING = 4;

                d.scroll.scrollable = false;
                d.scroll.scroll = d.active_line;
                d.scroll.lines = 16;
                d.scroll.inner_lines = d.num_lines + TOP_SCROLL_PADDING;
                auto vs = ui_begin_vscroll(s, d.scroll);

                for (uint32 i = 0; i < TOP_SCROLL_PADDING; ++i) {
                    vs.next_line();
                }

                for (uint32 i = 0; i < d.num_lines; ++i) {
                    if (i % d.lpb == 0 || i == d.active_line) {
                        const auto color =
                            i == d.active_line
                                ? blend_color(vs.config().colors.bg, vs.config().colors.fg, 0.1f)
                                : vs.config().colors.bg;
                        draw_fill_rrect(
                            vs.nvg(),
                            rect2<float32>{
                                {vs.rect().pos.x, vs.layout_cursor().y},
                                {vs.rect().size.x, vs.config().line_height}},
                            0.f, color);
                    }

                    vs.set_rtl(false);
                    vs.set_color(vs.config().colors.faint);
                    vs.write(fmt::format("{:02} ", i));
                    vs.set_rtl(true);
                    vs.write(fmt::format(" {:02}", i));
                    ui_tracker_line(vs, d.lines[i], d.num_notes, d.num_fx);

                    vs.next_line();
                }

                ui_end_vscroll(vs, d.scroll);

                if (vs.scissor().contains(vs.input().cursor_pos)) {
                    const auto scroll = vs.input().take_scroll();
                    if (scroll < 0.f && d.active_line < d.num_lines - 1) {
                        ++d.active_line;
                    } else if (scroll > 0.f && d.active_line > 0) {
                        --d.active_line;
                    }
                }
            },
        .update =
            [](data& d, const out& o) {
                if (!o.pause) {
                    d.active_line = o.active_line;
                }
            },
        .apply = [l = locals{}](const data& d, channels& chans) mutable -> out {
            size_t cur = 0;

            auto& bytes_out = chans.byte_chans[d.chan_out];
            bytes_out.mode = byte_mode::tracker;

            if (d.pause)
                return {true, 0};

            if (l.t_m > IO_FRAME_COUNT) {
                l.t_m -= IO_FRAME_COUNT;
            } else {
                l.t_m = 0;
                l.line = (l.line + 1) % d.num_lines;
                const auto p_note = reinterpret_cast<const uint8*>(&d.lines[l.line]);
                bytes_out.bytes.insert(bytes_out.bytes.end(), p_note, p_note + sizeof(tracker_line));
            }

            if (l.t_m == 0) {
                const auto bpl = 1.f / d.lpb;                                // beats per line
                const auto bps = d.bpm / 60.f;                               // beats per second
                const auto spb = static_cast<float32>(IO_SAMPLE_RATE) / bps; // samples per beat
                const auto spl = static_cast<uint32>(spb * bpl);             // samples per line
                l.t_m = spl;
            }

            return {false, l.line};
        },
    };

    return make_filter(std::move(f));
}