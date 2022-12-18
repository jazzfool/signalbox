#pragma once

#include "util.h"

#include <nanovg.h>
#include <array>

struct board_colors final {
    NVGcolor bg;
    NVGcolor fg;
    NVGcolor frame;
    NVGcolor faint;
    NVGcolor hover;
    NVGcolor red;
    NVGcolor green;
    NVGcolor blue;

    static board_colors dark() {
        return board_colors{
            .bg = nvgRGB(0, 0, 0),
            .fg = nvgRGB(255, 255, 255),
            .frame = nvgRGB(100, 100, 100),
            .faint = nvgRGB(50, 50, 50),
            .hover = nvgRGB(100, 100, 100),
            .red = nvgRGB(247, 47, 80),
            .green = nvgRGB(107, 255, 127),
            .blue = nvgRGB(76, 89, 252),
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
    vector2<float32> cursor_pos = {0.f, 0.f};
    std::array<bool, 3> mouse_just_pressed = {false, false, false};
    float32 scroll_wheel = 0.f;
};
