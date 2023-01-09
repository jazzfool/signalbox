#pragma once

#include "util.h"

#include <string_view>
#include <nanovg.h>
#include <variant>

enum class draw_font { mono, mono_bold, sans, sans_bold, _max };

struct draw_list final {
  public:
    static constexpr size_t MAX_LAYERS = 8;

    draw_list(NVGcontext* nvg);

    void execute();

    void push_layer();
    void pop_layer();

    void fill_rrect(const rect2f32& rect, float32 radius, NVGcolor color);
    void stroke_rrect(const rect2f32& rect, float32 radius, NVGcolor color, float32 stroke_width);

    void tb_grad_fill_rrect(const rect2f32& rect, float32 radius, NVGcolor from, NVGcolor to);

    vector2f32 measure_text(std::string_view text, draw_font font, float32 size) const;
    void centered_text(vector2f32 pos, std::string_view text, NVGcolor color, draw_font font, float32 size);

  private:
    struct cmd_paint final {
        NVGcolor color = nvgRGB(0, 0, 0);

        bool stroke = false;
        float32 width = 0.f;

        bool gradient = false;
        NVGcolor gradient_from = nvgRGB(0, 0, 0);
        NVGcolor gradient_to = nvgRGB(0, 0, 0);

        void apply(NVGcontext* nvg, const rect2f32& rect) const;
    };

    struct cmd_rrect final {
        rect2f32 rect;
        float32 radius = 0.f;
        cmd_paint paint;
    };

    struct cmd_text final {
        vector2f32 pos;
        int32 align = NVG_ALIGN_BASELINE | NVG_ALIGN_LEFT;
        NVGcolor color = nvgRGB(0, 0, 0);
        std::string text;
        draw_font font = draw_font::sans;
        float32 size = 0.f;
    };

    struct command final {
        enum { rrect, text } type;
        std::variant<cmd_rrect, cmd_text> cmd;
    };

    NVGcontext* m_nvg;
    uint8 m_layer = 0;
    std::array<std::vector<command>, MAX_LAYERS> m_list;
};

NVGcolor blend_color(NVGcolor a, NVGcolor b, float32 t);
