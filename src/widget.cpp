#include "widget.h"

#include "space.h"
#include "filters/sample.h"
#include "filters/trk.h"
#include "draw.h"

#include <sstream>
#include <iomanip>
#include <spdlog/fmt/fmt.h>
#include <GLFW/glfw3.h>
#include <numbers>
#include <codecvt>
#include <miniaudio.h>
#include <cmath>
#include <nfd.hpp>

void ui_chan_btn(space& space, uint8& chan, bool inc) {
    space.set_color(space.config().colors.blue);
    if (space.write_button(inc ? ">" : "<") && (inc ? chan < MAX_CHANNELS : chan > 0)) {
        chan += ((int8)inc << 1) - 1;
    }
}

void ui_chan_sel(space& space, uint8& chan) {
    const auto rtl = space.rtl();

    ui_chan_btn(space, chan, rtl);

    space.set_color(space.config().colors.fg);
    const auto s = fmt::format("{:02}", chan);
    if (space.write_hover(s, space.config().colors.hover, space.color())) {
        const auto scroll = clamp<uint8>(0, 1, static_cast<uint8>(std::abs(space.input().scroll_wheel)));
        if (space.input().scroll_wheel > 0.f && chan < MAX_CHANNELS)
            chan += scroll;
        else if (space.input().scroll_wheel < 0.f && chan > 0)
            chan -= scroll;
    }

    ui_chan_btn(space, chan, !rtl);

    space.set_color(space.config().colors.fg);
}

void ui_enum_btn(space& space, uint32& i, uint32 size, bool wrap, bool inc) {
    space.set_color(space.config().colors.blue);
    if (space.write_button(inc ? ">" : "<") && (wrap || (inc ? i < size - 1 : i > 0))) {
        i += ((int8)inc << 1) - 1;
        i %= size;
    }
}

void ui_enum_sel(space& space, std::span<const std::string> options, bool wrap, uint32& i) {
    const auto rtl = space.rtl();

    auto max_sz = std::size_t{0};
    for (const auto& s : options) {
        max_sz = std::max(max_sz, s.size());
    }

    ui_enum_btn(space, i, options.size(), wrap, rtl);

    if (options.size() > 0) {
        space.set_color(space.config().colors.fg);
        if (space.write_hover(
                options[i] + std::string(max_sz - options[i].size(), '.'), space.config().colors.hover,
                space.color())) {
            const auto scroll = static_cast<uint8>(std::abs(space.input().scroll_wheel));
            if (space.input().scroll_wheel > 0.f && (wrap || i < options.size() - 1))
                i += scroll;
            else if (space.input().scroll_wheel < 0.f && (wrap || i > 0))
                i -= scroll;
            i %= options.size();
        }
    }

    ui_enum_btn(space, i, options.size(), wrap, !rtl);

    space.set_color(space.config().colors.fg);
}

void ui_float_btn(space& space, float32& x, float32 min, float32 max, float32 step, bool inc) {
    space.set_color(inc ? space.config().colors.green : space.config().colors.red);
    if (space.write_button(inc ? "+" : "-")) {
        x = clamp(min, max, x + (float32)(((int8)inc << 1) - 1) * step);
    }
}

void ui_float_ran(space& space, float32 min, float32 max, float32 step, float32& x) {
    const auto rtl = space.rtl();

    ui_float_btn(space, x, min, max, step, rtl);

    space.set_color(space.config().colors.fg);
    const auto prec =
        std::max(static_cast<int32>(fmt::format("{:g}", step - std::floor(step)).size()) - 2, 0);
    if (space.write_hover(fmt::format("[{:+.{}f}]", x, prec), space.config().colors.hover, space.color())) {
        x = clamp(min, max, x + step * std::round(space.input().scroll_wheel));
    }

    ui_float_btn(space, x, min, max, step, !rtl);

    space.set_color(space.config().colors.fg);
}

void ui_float(space& space, float32 step, float32& x) {
    ui_float_ran(space, -INFINITY, INFINITY, step, x);
}

void ui_int_btn(space& space, int32& x, int32 min, int32 max, int32 step, bool inc) {
    space.set_color(inc ? space.config().colors.green : space.config().colors.red);
    if (space.write_button(inc ? "+" : "-")) {
        x = clamp(min, max, x + step * (int32)(((int8)inc << 1) - 1));
    }
}

void ui_int_ran(space& space, int32 min, int32 max, int32 step, uint8 pad, int32& x) {
    const auto rtl = space.rtl();

    ui_int_btn(space, x, min, max, step, rtl);

    space.set_color(space.config().colors.fg);
    if (space.write_hover(fmt::format("[{:0{}}]", x, pad), space.config().colors.hover, space.color())) {
        x = clamp(min, max, x + step * static_cast<int32>(std::lround(space.input().scroll_wheel)));
    }

    ui_int_btn(space, x, min, max, step, !rtl);

    space.set_color(space.config().colors.fg);
}

void ui_int(space& space, int32 step, uint8 pad, int32& x) {
    ui_int_ran(space, INT32_MIN, INT32_MAX, step, pad, x);
}

void ui_tracker_note(space& space, tracker_note& note) {
    auto hover = false;

    switch (note.mode) {
    case tracker_note_mode::empty: {
        space.set_color(space.config().colors.semifaint);
        hover = space.write_hover("...", space.config().colors.hover, space.config().colors.semifaint);
        break;
    }
    case tracker_note_mode::on: {
        space.set_color(space.config().colors.fg);
        hover = space.write_hover(
            fmt::format("{:03}", note.index), space.config().colors.hover, space.config().colors.fg);
        break;
    }
    case tracker_note_mode::off: {
        space.set_color(space.config().colors.red);
        hover = space.write_hover("OFF", space.config().colors.hover, space.config().colors.red);
        break;
    }
    default:
        break;
    }

    if (hover) {
        if (note.mode == tracker_note_mode::on) {
            const auto scroll = space.input().scroll_wheel;
            if (scroll > 0.f && note.index < 255) {
                ++note.index;
            } else if (scroll < 0.f && note.index > 0) {
                --note.index;
            }
        }

        if (space.input().mouse_just_pressed[0]) {
            note.mode = (tracker_note_mode)(((uint8)note.mode + 1) % (uint8)tracker_note_mode::_max);
        }
    }
}

void ui_text_in(space& space, NVGcolor color, std::string& s, const std::string& placeholder, uint32 len) {
    const auto base_str = s.empty() ? placeholder : s;
    const auto str = std::string{
        std::string_view{base_str.size() < len ? base_str + std::string(len - s.size(), ' ') : base_str}
            .substr(
                space.focus == &base_str && base_str.size() > len ? s.size() - len : 0,
                space.focus == &base_str ? std::string::npos : len)};
    const auto bounds = space.measure(str);

    const auto nvg = space.nvg();

    draw_fill_rrect(nvg, bounds, 2.f, space.config().colors.editable);

    if (s.empty()) {
        color.a = 0.6f;
    }

    space.set_color(color);
    const auto hover = space.write_hover(str, space.config().colors.editable, color);
    space.set_color(space.config().colors.fg);

    if (space.focus == &s) {
        draw_stroke_rrect(
            nvg, bounds.inflate(2.f).translate({0.f, -2.f}).half_round(), 2.f,
            space.config().colors.focus_frame, 1.f);
    } else {
        draw_stroke_rrect(
            nvg, bounds.inflate(1.f).translate({0.f, -2.f}).half_round(), 2.f, space.config().colors.frame,
            1.f);
    }

    if (space.focus == &s) {
        if (space.input().text) {
            std::wstring_convert<std::codecvt_utf8<char32>, char32> cvt;
            s += cvt.to_bytes(*space.input().text);
        }

        if (!s.empty() && space.input().keys_just_pressed[GLFW_KEY_BACKSPACE]) {
            s.pop_back();
        }

        if (space.input().keys_just_pressed[GLFW_KEY_ENTER]) {
            space.focus = nullptr;
        }
    } else if (hover && space.input().mouse_just_pressed[0]) {
        space.focus = &s;
        space.did_focus = true;
        s.clear();
    }
}

bool ui_filepath_in(
    space& space, NVGcolor color, std::string& s, const std::string& placeholder, uint32 len, bool dir_mode) {
    const auto base_str = s.empty() ? placeholder : s;
    const auto str =
        base_str.size() < len ? base_str + std::string(len - base_str.size(), ' ') : base_str.substr(0, len);
    const auto bounds = space.measure(str).inflate(1.f).translate({0.f, -2.f});

    const auto nvg = space.nvg();

    color = blend_color(space.config().colors.editable, color, 0.25f);
    draw_fill_rrect(nvg, bounds, 2.f, color);

    space.set_color(space.config().colors.fg);
    auto ok = false;
    if (space.write_hover(str, color, space.config().colors.fg) && space.input().mouse_just_pressed[0]) {
        NFD::UniquePath path;

        nfdresult_t res;
        if (dir_mode) {
            res = NFD::PickFolder(path);
        } else {
            res = NFD::OpenDialog(path);
        }

        ok = res == NFD_OKAY;
        if (ok) {
            s = path.get();
        }
    }

    draw_stroke_rrect(nvg, bounds.half_round(), 2.f, space.config().colors.frame, 1.f);

    return ok;
}

void draw_grid(space& space, NVGcolor color, const rect2<float32>& rect, float32 spacing) {
    const auto nvg = space.nvg();

    nvgSave(nvg);

    nvgBeginPath(nvg);

    for (uint32 i = 1; i < static_cast<uint32>(std::ceil(rect.size.x / spacing)); ++i) {
        const auto x = rect.pos.x + static_cast<float32>(i) * spacing;
        nvgMoveTo(nvg, x, rect.pos.y);
        nvgLineTo(nvg, x, rect.max().y);
    }

    for (uint32 i = 1; i < static_cast<uint32>(std::ceil(rect.size.y / spacing)); ++i) {
        const auto y = rect.pos.y + static_cast<float32>(i) * spacing;
        nvgMoveTo(nvg, rect.pos.x, y);
        nvgLineTo(nvg, rect.max().x, y);
    }

    nvgStrokeWidth(nvg, 1.f);
    nvgStrokeColor(nvg, color);
    nvgStroke(nvg);

    nvgRestore(nvg);
}

void ui_viz_sine(space& space, NVGcolor stroke, uint32 lines, uint32 samples, float32 ampl, float32 freq) {
    const auto nvg = space.nvg();
    const auto outer_rect = space.draw_block(lines);
    const auto border_rect = outer_rect.half_round();

    freq *= 2.f * std::numbers::pi;

    nvgSave(nvg);

    draw_grid(
        space, space.config().colors.faint, outer_rect, std::min(outer_rect.size.x, outer_rect.size.y) / 2.f);

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, outer_rect.pos.x, outer_rect.center().y);
    nvgLineTo(nvg, outer_rect.max().x, outer_rect.center().y);
    nvgStrokeColor(nvg, space.config().colors.fg);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, border_rect.pos.x, border_rect.pos.y, border_rect.size.x, border_rect.size.y, 2.f);
    nvgStrokeColor(nvg, space.config().colors.frame);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    const auto rect = outer_rect.inflate(-2.f);

    nvgSave(nvg);
    nvgIntersectScissor(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgBeginPath(nvg);
    for (uint32 i = 0; i < samples; ++i) {
        const auto x = static_cast<float32>(i) / static_cast<float32>(samples);
        const auto y = ampl * std::sin(freq * x);

        (i == 0 ? nvgMoveTo
                : nvgLineTo)(nvg, rect.pos.x + x * rect.size.x, rect.pos.y + rect.size.y * (1.f - y) / 2.f);
    }
    nvgStrokeColor(nvg, stroke);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);
    nvgRestore(nvg);

    nvgRestore(nvg);
}

void ui_viz_wf(
    space& space, NVGcolor stroke, uint32 lines, float32 scale, float32 offset,
    simd_slice<const sample> samples) {
    const auto nvg = space.nvg();
    const auto outer_rect = space.draw_block(lines);
    const auto border_rect = outer_rect.half_round();
    const auto y_range = outer_rect.size.y / 2.f;
    const auto samples_sz = std::max<size_t>(samples.size(), 1);

    nvgSave(nvg);

    draw_grid(
        space, space.config().colors.faint, outer_rect, std::min(outer_rect.size.x, outer_rect.size.y) / 2.f);

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, outer_rect.pos.x, outer_rect.center().y - offset * y_range);
    nvgLineTo(nvg, outer_rect.max().x, outer_rect.center().y - offset * y_range);
    nvgStrokeColor(nvg, space.config().colors.fg);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, border_rect.pos.x, border_rect.pos.y, border_rect.size.x, border_rect.size.y, 2.f);
    nvgStrokeColor(nvg, space.config().colors.frame);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    const auto rect = outer_rect.inflate(-2.f);

    nvgSave(nvg);
    nvgIntersectScissor(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgBeginPath(nvg);
    for (uint32 i = 0; i < samples.size(); ++i) {
        const auto x = static_cast<float32>(i) / static_cast<float32>(samples_sz);
        const auto y = scale * samples[i] + offset;

        (i == 0 ? nvgMoveTo
                : nvgLineTo)(nvg, rect.pos.x + x * rect.size.x, rect.pos.y + rect.size.y * (1.f - y) / 2.f);
    }
    nvgStrokeColor(nvg, stroke);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);
    nvgRestore(nvg);

    nvgRestore(nvg);
}

void ui_viz_sample(
    space& space, NVGcolor stroke, uint32 lines, uint64 length, uint64 scale, uint64 first, uint64 last,
    simd_slice<const sample> samples, size_t active_splice, std::span<int32> splices) {
    const auto nvg = space.nvg();
    const auto outer_rect = space.draw_block(lines);
    const auto border_rect = outer_rect.half_round();
    const auto y_range = outer_rect.size.y / 2.f;

    nvgSave(nvg);

    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, border_rect.pos.x, border_rect.pos.y, border_rect.size.x, border_rect.size.y, 2.f);
    nvgStrokeColor(nvg, space.config().colors.frame);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    const auto rect = outer_rect.inflate(-2.f);

    if (first / scale <= samples.size() && last / scale <= samples.size()) {
        const auto samples_slice = samples.slice(first / scale, (last - first) / scale);
        const auto inv_samples_slice_sz = 1.f / static_cast<float32>(samples_slice.size());

        nvgSave(nvg);
        nvgIntersectScissor(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
        nvgBeginPath(nvg);
        for (uint64 i = 0; i < samples_slice.size(); ++i) {
            const auto x = static_cast<float32>(i) * inv_samples_slice_sz;
            const auto y = samples_slice[i];

            (i == 0 ? nvgMoveTo : nvgLineTo)(
                nvg, rect.pos.x + x * rect.size.x, rect.pos.y + rect.size.y * (1.f - y) / 2.f);
        }
        nvgStrokeColor(nvg, stroke);
        nvgStrokeWidth(nvg, 1.f);
        nvgStroke(nvg);
        nvgRestore(nvg);
    }

    const auto inv_length = 1.f / (float32)length;

    auto splice_fg = space.config().colors.fg;
    splice_fg.a = 0.7f;
    nvgBeginPath(nvg);
    nvgStrokeColor(nvg, splice_fg);
    nvgStrokeWidth(nvg, 1.f);
    for (size_t i = 0; i < splices.size(); ++i) {
        const auto splice = splices[i];
        if (i == active_splice || splice <= first || splice >= last)
            continue;
        const auto f_splice = remap((float32)first, (float32)last, 0.f, 1.f, (float32)splice);
        const auto x = rect.pos.x + rect.size.x * f_splice;
        nvgMoveTo(nvg, x, rect.pos.y);
        nvgLineTo(nvg, x, rect.max().y);
    }
    nvgStroke(nvg);

    if (splices[active_splice] >= first && splices[active_splice] <= last) {
        nvgBeginPath(nvg);
        const auto x =
            rect.pos.x +
            rect.size.x * remap((float32)first, (float32)last, 0.f, 1.f, (float32)splices[active_splice]);
        nvgMoveTo(nvg, std::round(x) + 0.5f, rect.pos.y);
        nvgLineTo(nvg, std::round(x) + 0.5f, rect.max().y);
        nvgStrokeColor(nvg, space.config().colors.yellow);
        nvgStrokeWidth(nvg, 1.f);
        nvgStroke(nvg);
    }

    nvgRestore(nvg);
}

void ui_viz_meter(space& space, float32 level) {
    const auto nvg = space.nvg();
    const auto rect = space.draw_block(1);

    const auto count = 100;
    const auto height = space.config().line_height;
    const auto width = rect.size.x / (float32)count;

    const auto db_min = -21.f;
    const auto db_max = 3.1f;

    const auto linear_min = ma_volume_db_to_linear(db_min);
    const auto linear_max = ma_volume_db_to_linear(db_max);

    nvgSave(nvg);
    nvgBeginPath(nvg);
    nvgFillColor(nvg, space.config().colors.meter_lo);
    auto range = 0;
    auto labels =
        std::vector<std::optional<float32>>{-20.f, -10.f, -7.f, -5.f, -3.f, -2.f, -1.f, 0.f, 1.f, 2.f, 3.f};
    for (uint32 i = 0; i < count; ++i) {
        const auto db =
            remap(0.f, static_cast<float32>(count) - 1.f, db_min, db_max, static_cast<float32>(i));
        const auto next_db =
            remap(0.f, static_cast<float32>(count) - 1.f, db_min, db_max, static_cast<float32>(i + 1));

        const auto linear = ma_volume_db_to_linear(db);

        if (db > 0.f && range < 2) {
            range = 2;
            nvgFill(nvg);
            nvgBeginPath(nvg);
            nvgFillColor(nvg, space.config().colors.meter_hi);
        } else if (db > -5.f && range < 1) {
            range = 1;
            nvgFill(nvg);
            nvgBeginPath(nvg);
            nvgFillColor(nvg, space.config().colors.meter_md);
        }

        for (auto& label : labels) {
            if (label && db > *label) {
                nvgSave(nvg);
                nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE);
                nvgFontFace(nvg, "monoB");
                nvgFontSize(nvg, height / 2.f + 1.f);
                nvgFillColor(nvg, space.config().colors.fg);
                nvgText(
                    nvg, rect.pos.x + width * (float32)i, rect.center().y - 1.f,
                    fmt::format("{:+}", *label).c_str(), nullptr);
                nvgRestore(nvg);
                label.reset();
            }
        }

        if (linear < level)
            nvgRect(nvg, rect.pos.x + width * (float32)i, rect.center().y, width / 2.f, height / 2.f);
    }
    nvgFill(nvg);
    nvgRestore(nvg);
}
