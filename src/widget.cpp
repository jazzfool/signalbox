#include "widget.h"

#include "space.h"
#include "filters/sample.h"

#include <sstream>
#include <iomanip>
#include <spdlog/fmt/fmt.h>
#include <GLFW/glfw3.h>
#include <numbers>
#include <codecvt>

void ui_chan_btn(space& space, uint8& chan, bool inc) {
    space.set_color(space.config().colors.blue);
    if (space.write_button(inc ? ">" : "<") && (inc ? chan < 99 : chan > 0)) {
        chan += ((int8)inc << 1) - 1;
    }
}

void ui_chan_sel(space& space, uint8& chan) {
    const auto rtl = space.rtl();

    ui_chan_btn(space, chan, rtl);

    space.set_color(space.config().colors.fg);
    const auto s = fmt::format("{:02}", chan);
    if (space.write_hover(s, space.config().colors.hover, space.color())) {
        const auto scroll = static_cast<uint8>(std::abs(space.input().scroll_wheel));
        if (space.input().scroll_wheel > 0.f && chan < MAX_CHANNELS)
            chan += scroll;
        else if (space.input().scroll_wheel < 0.f && chan > 0)
            chan -= scroll;
    }

    ui_chan_btn(space, chan, !rtl);

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

void ui_text_in(space& space, std::string& s, uint32 min_len) {
    const auto bounds = space.measure(s);

    const auto nvg = space.nvg();
    if (space.focus == &s) {
        nvgBeginPath(nvg);
        nvgRoundedRect(nvg, bounds.pos.x, bounds.pos.y, bounds.size.x, bounds.size.y, 2.f);
        nvgFillColor(nvg, nvgRGB(30, 30, 30));
        nvgFill(nvg);
    }

    const auto hover = space.write_hover(s, space.config().colors.hover, space.config().colors.fg);

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
        s.clear();
    }
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

    freq *= 2.f * std::numbers::pi;

    nvgSave(nvg);

    draw_grid(space, space.config().colors.faint, outer_rect,
              std::min(outer_rect.size.x, outer_rect.size.y) / 2.f);

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, outer_rect.pos.x, outer_rect.center().y);
    nvgLineTo(nvg, outer_rect.max().x, outer_rect.center().y);
    nvgStrokeColor(nvg, space.config().colors.fg);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, outer_rect.pos.x, outer_rect.pos.y, outer_rect.size.x, outer_rect.size.y, 2.f);
    nvgStrokeColor(nvg, space.config().colors.frame);
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);

    const auto rect = outer_rect.inflate(-2.f);

    nvgSave(nvg);
    nvgIntersectScissor(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgBeginPath(nvg);
    for (uint32 i = 0; i < samples; ++i) {
        const auto x = static_cast<float32>(i) / static_cast<float32>(samples);
        const auto y = ampl * std::sin(freq * x);

        (i == 0 ? nvgMoveTo : nvgLineTo)(nvg, rect.pos.x + x * rect.size.x,
                                         rect.pos.y + rect.size.y * (1.f - y) / 2.f);
    }
    nvgStrokeColor(nvg, stroke);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);
    nvgRestore(nvg);

    nvgRestore(nvg);
}

void ui_viz_wf(space& space, NVGcolor stroke, uint32 lines, float32 ampl, std::span<const sample> samples) {
    const auto nvg = space.nvg();
    const auto outer_rect = space.draw_block(lines);

    nvgSave(nvg);

    draw_grid(space, space.config().colors.faint, outer_rect,
              std::min(outer_rect.size.x, outer_rect.size.y) / 2.f);

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, outer_rect.pos.x, outer_rect.center().y);
    nvgLineTo(nvg, outer_rect.max().x, outer_rect.center().y);
    nvgStrokeColor(nvg, space.config().colors.fg);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);

    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, outer_rect.pos.x, outer_rect.pos.y, outer_rect.size.x, outer_rect.size.y, 2.f);
    nvgStrokeColor(nvg, space.config().colors.frame);
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);

    const auto rect = outer_rect.inflate(-2.f);

    nvgSave(nvg);
    nvgIntersectScissor(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgBeginPath(nvg);
    for (uint32 i = 0; i < samples.size(); ++i) {
        const auto x = static_cast<float32>(i) / static_cast<float32>(samples.size());
        const auto y = ampl * samples[i];

        (i == 0 ? nvgMoveTo : nvgLineTo)(nvg, rect.pos.x + x * rect.size.x,
                                         rect.pos.y + rect.size.y * (1.f - y) / 2.f);
    }
    nvgStrokeColor(nvg, stroke);
    nvgStrokeWidth(nvg, 1.f);
    nvgStroke(nvg);
    nvgRestore(nvg);

    nvgRestore(nvg);
}
