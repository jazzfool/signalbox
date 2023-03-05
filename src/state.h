#pragma once

#include "util.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <nanovg.h>
#include <array>
#include <optional>
#include <unordered_map>

struct UI_Options final {
    float32 font_size;
    float32 corner_radius;
    float32 border_width;
    Vector2_F32 text_pad;
    float32 panel_pad;
    float32 scroll_bar_width;

    static UI_Options default_options() {
        return UI_Options{
            .font_size = 10.f,
            .corner_radius = 2.f,
            .border_width = 1.f,
            .text_pad = {6.f, 3.f},
            .panel_pad = 5.f,
            .scroll_bar_width = 5.f,
        };
    }
};

struct UI_Colors final {
    NVGcolor window_bg;
    NVGcolor bg;
    NVGcolor fg;
    NVGcolor border;
    NVGcolor focus_border;

    NVGcolor highlight_bg;
    NVGcolor highlight_fg;
    NVGcolor lowlight_bg;
    NVGcolor lowlight_fg;

    NVGcolor button_from;
    NVGcolor button_to;
    NVGcolor button_border;

    NVGcolor input_bg;
    NVGcolor input_fg;
    NVGcolor input_focus_bg;
    NVGcolor input_focus_fg;

    NVGcolor dial_fill_from;
    NVGcolor dial_fill_to;
    NVGcolor dial_border_from;
    NVGcolor dial_border_to;
    NVGcolor dial_tick;

    NVGcolor scroll_bar_bg;
    NVGcolor scroll_bar_fg;
    NVGcolor scroll_bar_press_fg;

    static UI_Colors dark() {
        return UI_Colors{
            .window_bg = nvgRGB(40, 40, 40),
            .bg = nvgRGB(60, 60, 60),
            .fg = nvgRGB(212, 212, 212),
            .border = nvgRGB(0, 0, 0),
            .focus_border = nvgRGB(171, 14, 66),

            .highlight_bg = nvgRGB(171, 14, 66),
            .highlight_fg = nvgRGB(255, 255, 255),
            .lowlight_bg = nvgRGB(45, 45, 45),
            .lowlight_fg = nvgRGB(230, 230, 230),

            .button_from = nvgRGB(40, 40, 40),
            .button_to = nvgRGB(20, 20, 20),
            .button_border = nvgRGB(0, 0, 0),

            .input_bg = nvgRGB(30, 30, 30),
            .input_fg = nvgRGB(180, 180, 180),
            .input_focus_bg = nvgRGB(20, 20, 20),
            .input_focus_fg = nvgRGB(220, 220, 220),

            .dial_fill_from = nvgRGB(50, 50, 50),
            .dial_fill_to = nvgRGB(20, 20, 20),
            .dial_border_from = nvgRGB(120, 120, 120),
            .dial_border_to = nvgRGB(50, 50, 50),
            .dial_tick = nvgRGB(120, 120, 120),

            .scroll_bar_bg = nvgRGB(20, 20, 20),
            .scroll_bar_fg = nvgRGB(70, 70, 70),
            .scroll_bar_press_fg = nvgRGB(90, 90, 90),
        };
    }
};

struct Input_State final {
    std::array<bool, GLFW_KEY_LAST> key_is_pressed;
    std::array<bool, GLFW_KEY_LAST> keys_just_pressed;
    std::optional<char32> text;

    Vector2_F32 cursor_pos;
    Vector2_F32 cursor_delta;
    float32 scroll_wheel = 0.f;
    std::array<bool, GLFW_MOUSE_BUTTON_LAST> mouse_just_pressed;
    std::array<bool, GLFW_MOUSE_BUTTON_LAST> mouse_just_released;
    std::array<bool, GLFW_MOUSE_BUTTON_LAST> mouse_is_pressed;

    Input_State() {
        key_is_pressed.fill(false);
        mouse_is_pressed.fill(false);

        end_frame();
    }

    void begin_frame() {
    }

    void end_frame() {
        keys_just_pressed.fill(false);
        text.reset();

        cursor_delta = {};
        scroll_wheel = 0.f;
        mouse_just_pressed.fill(false);
        mouse_just_released.fill(false);
    }

    float32 take_scroll() {
        const auto f = scroll_wheel;
        scroll_wheel = 0.f;
        return f;
    }

    bool hover(const Rect2_F32& rect) const {
        return rect.contains(cursor_pos);
    }
};
