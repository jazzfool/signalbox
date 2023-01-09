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

    static ui_options default_options() {
        return ui_options{
            .font_size = 12.f,
        };
    }
};

struct ui_colors final {
    NVGcolor bg;
    NVGcolor fg;

    NVGcolor button_from;
    NVGcolor button_to;
    NVGcolor button_border;

    static ui_colors dark() {
        return ui_colors{
            .bg = nvgRGB(30, 30, 30),
            .fg = nvgRGB(212, 212, 212),

            .button_from = nvgRGB(110, 110, 110),
            .button_to = nvgRGB(60, 60, 60),
            .button_border = nvgRGB(0, 0, 0),
        };
    }
};

struct input_state final {
    std::array<bool, GLFW_KEY_LAST> keys_just_pressed;
    std::optional<char32> text;

    vector2<float32> cursor_pos;
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
