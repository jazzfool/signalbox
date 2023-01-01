#pragma once

#include "util.h"
#include "filter.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <nanovg.h>
#include <array>
#include <optional>
#include <unordered_map>

struct board_colors final {
    NVGcolor bg;
    NVGcolor fg;
    NVGcolor frame;
    NVGcolor semifaint;
    NVGcolor faint;
    NVGcolor hover;
    NVGcolor focus;
    NVGcolor red;
    NVGcolor green;
    NVGcolor blue;
    NVGcolor yellow;
    NVGcolor meter_lo;
    NVGcolor meter_md;
    NVGcolor meter_hi;
    NVGcolor media_control;
    std::unordered_map<filter_kind, NVGcolor> filters;

    static board_colors dark() {
        return board_colors{
            .bg = nvgRGB(0, 0, 0),
            .fg = nvgRGB(255, 255, 255),
            .frame = nvgRGB(100, 100, 100),
            .semifaint = nvgRGB(120, 120, 120),
            .faint = nvgRGB(50, 50, 50),
            .hover = nvgRGB(100, 100, 100),
            .focus = nvgRGB(30, 30, 30),
            .red = nvgRGB(247, 47, 80),
            .green = nvgRGB(107, 255, 127),
            .blue = nvgRGB(76, 89, 252),
            .yellow = nvgRGB(252, 232, 3),
            .meter_lo = nvgRGB(3, 252, 7),
            .meter_md = nvgRGB(246, 255, 0),
            .meter_hi = nvgRGB(255, 51, 0),
            .media_control = nvgRGB(139, 232, 223),
            .filters =
                {
                    {filter_kind::chn, nvgRGB(73, 148, 166)},
                    {filter_kind::gen, nvgRGB(106, 171, 89)},
                    {filter_kind::viz, nvgRGB(74, 181, 124)},
                    {filter_kind::fir, nvgRGB(135, 76, 194)},
                    {filter_kind::iir, nvgRGB(194, 76, 151)},
                    {filter_kind::trk, nvgRGB(157, 255, 0)},
                    {filter_kind::misc, nvgRGB(156, 135, 141)},
                },
        };
    }
};

struct board_config final {
    board_colors colors = board_colors::dark();

    float32 font_size;
    float32 line_height;
    float32 char_width;
    float32 ascender;

    float32 outer_padding;
    float32 inner_padding;
};

struct input_state final {
    std::array<bool, GLFW_KEY_LAST> keys_just_pressed;
    vector2<float32> cursor_pos = {0.f, 0.f};
    std::array<bool, 3> mouse_just_pressed = {false, false, false};
    float32 scroll_wheel = 0.f;
    std::optional<char32> text = {};

    input_state() {
        keys_just_pressed.fill(false);
    }
};
