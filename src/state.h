#pragma once

#include "util.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <nanovg.h>
#include <array>
#include <optional>
#include <unordered_map>

struct ui_options final {
    float32 font_size;
    float32 corner_radius;
    float32 border_width;
    vector2f32 text_pad;
    float32 panel_pad;

    static ui_options default_options() {
        return ui_options{
            .font_size = 10.f,
            .corner_radius = 2.f,
            .border_width = 1.f,
            .text_pad = {6.f, 3.f},
            .panel_pad = 5.f,
        };
    }
};

struct ui_colors final {
    NVGcolor window_bg;
    NVGcolor bg;
    NVGcolor fg;
    NVGcolor border;

    NVGcolor highlight_bg;
    NVGcolor highlight_fg;
    NVGcolor lowlight_bg;
    NVGcolor lowlight_fg;

    NVGcolor button_from;
    NVGcolor button_to;
    NVGcolor button_border;

    NVGcolor dial_fill_from;
    NVGcolor dial_fill_to;
    NVGcolor dial_border_from;
    NVGcolor dial_border_to;
    NVGcolor dial_tick;

    static ui_colors dark() {
        return ui_colors{
            .window_bg = nvgRGB(40, 40, 40),
            .bg = nvgRGB(60, 60, 60),
            .fg = nvgRGB(212, 212, 212),
            .border = nvgRGB(0, 0, 0),

            .highlight_bg = nvgRGB(171, 14, 66),
            .highlight_fg = nvgRGB(255, 255, 255),
            .lowlight_bg = nvgRGB(45, 45, 45),
            .lowlight_fg = nvgRGB(230, 230, 230),

            .button_from = nvgRGB(40, 40, 40),
            .button_to = nvgRGB(20, 20, 20),
            .button_border = nvgRGB(0, 0, 0),

            .dial_fill_from = nvgRGB(50, 50, 50),
            .dial_fill_to = nvgRGB(20, 20, 20),
            .dial_border_from = nvgRGB(120, 120, 120),
            .dial_border_to = nvgRGB(50, 50, 50),
            .dial_tick = nvgRGB(120, 120, 120),
        };
    }
};

struct input_state final {
    std::array<bool, GLFW_KEY_LAST> key_is_pressed;
    std::array<bool, GLFW_KEY_LAST> keys_just_pressed;
    std::optional<char32> text;

    vector2f32 cursor_pos;
    vector2f32 cursor_delta;
    float32 scroll_wheel = 0.f;
    bool hover_taken = false;
    std::array<bool, GLFW_MOUSE_BUTTON_LAST> mouse_just_pressed;
    std::array<bool, GLFW_MOUSE_BUTTON_LAST> mouse_just_released;
    std::array<bool, GLFW_MOUSE_BUTTON_LAST> mouse_is_pressed;

    std::vector<rect2f32> prev_overlays;
    std::vector<rect2f32> next_overlays;
    bool hover_overlay = false;
    bool was_hover_overlay = false;

    input_state() {
        key_is_pressed.fill(false);
        mouse_is_pressed.fill(false);

        end_frame();
    }

    void begin_frame() {
        prev_overlays = next_overlays;
        next_overlays.clear();

        hover_overlay = false;
        for (const auto& overlay : prev_overlays) {
            if (overlay.contains(cursor_pos)) {
                hover_overlay = true;
                break;
            }
        }
    }

    void end_frame() {
        keys_just_pressed.fill(false);
        text.reset();

        cursor_delta = {};
        scroll_wheel = 0.f;
        mouse_just_pressed.fill(false);
        mouse_just_released.fill(false);

        hover_taken = false;
    }

    float32 take_scroll() {
        const auto f = scroll_wheel;
        scroll_wheel = 0.f;
        return f;
    }

    bool take_hover(const rect2f32& rect) {
        if (!hover_overlay && !hover_taken && rect.contains(cursor_pos)) {
            hover_taken = true;
            return true;
        }
        return false;
    }

    void push_overlay(const rect2f32& rect) {
        next_overlays.push_back(rect);
    }

    void begin_overlay_hover(const rect2f32& rect) {
        if (hover_overlay && rect.contains(cursor_pos)) {
            was_hover_overlay = true;
            hover_overlay = false;
        }
    }

    void end_overlay_hover() {
        hover_overlay = was_hover_overlay;
        was_hover_overlay = false;
    }
};
