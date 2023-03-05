#pragma once

#include "util.h"

#include <string_view>
#include <nanovg.h>
#include <variant>
#include <deque>

enum class Draw_Font { Mono, Mono_Bold, Sans, Sans_Bold, _Max };

enum class Text_Align { Center_Middle, Left_Middle };

struct Draw_List final {
  public:
    static constexpr size_t MAX_LAYERS = 8;

    Draw_List(NVGcontext* nvg);

    void reset(uint32 width, uint32 height);

    void execute();

    void push_layer();
    void pop_layer();

    void push_clip_rect(const Rect2_F32& rect);
    void pop_clip_rect();
    Rect2_F32 clip_rect() const;

    void fill_rect(const Rect2_F32& rect, NVGcolor color);
    void stroke_rect(const Rect2_F32& rect, NVGcolor color, float32 stroke_width);
    void tb_grad_fill_rect(const Rect2_F32& rect, NVGcolor from, NVGcolor to);

    void fill_rrect(const Rect2_F32& rect, float32 radius, NVGcolor color);
    void stroke_rrect(const Rect2_F32& rect, float32 radius, NVGcolor color, float32 stroke_width);
    void tb_grad_fill_rrect(const Rect2_F32& rect, float32 radius, NVGcolor from, NVGcolor to);

    void fill_circle(Vector2_F32 center, float32 radius, NVGcolor color);
    void tb_grad_fill_circle(Vector2_F32 center, float32 radius, NVGcolor from, NVGcolor to);

    void stroke_line(Vector2_F32 p0, Vector2_F32 p1, NVGcolor color, float32 stroke_width);

    Vector2_F32
    measure_text(std::string_view text, Draw_Font font, float32 size, float32* advance = nullptr) const;
    float32 line_height(Draw_Font font, float32 size) const;
    void text(
        Vector2_F32 pos, Text_Align align, std::string_view text, NVGcolor color, Draw_Font font,
        float32 size);

  private:
    struct Cmd_Paint final {
        NVGcolor color = nvgRGB(0, 0, 0);

        bool stroke = false;
        float32 width = 0.f;

        bool gradient = false;
        NVGcolor gradient_from = nvgRGB(0, 0, 0);
        NVGcolor gradient_to = nvgRGB(0, 0, 0);

        void apply(NVGcontext* nvg, const Rect2_F32& rect) const;
    };

    struct Cmd_Rect final {
        Rect2_F32 rect;
        Cmd_Paint paint;
    };

    struct Cmd_RRect final {
        Rect2_F32 rect;
        float32 radius = 0.f;
        Cmd_Paint paint;
    };

    struct Cmd_Circle final {
        Vector2_F32 center;
        float32 radius = 0.f;
        Cmd_Paint paint;
    };

    struct Cmd_Line final {
        Vector2_F32 p0;
        Vector2_F32 p1;
        Cmd_Paint paint;
    };

    struct Cmd_Text final {
        Vector2_F32 pos;
        Text_Align align = Text_Align::Center_Middle;
        NVGcolor color = nvgRGB(0, 0, 0);
        std::string text;
        Draw_Font font = Draw_Font::Sans;
        float32 size = 0.f;
    };

    struct Cmd_Clip final {
        enum { Push, Pop } mode = Push;
        Rect2_F32 rect;
    };

    using Command = std::variant<Cmd_Rect, Cmd_RRect, Cmd_Circle, Cmd_Line, Cmd_Text, Cmd_Clip>;

    struct Layer {
        std::vector<Command> cmds;
        std::deque<Rect2_F32> clip_stack;
    };

    NVGcontext* m_nvg;
    uint8 m_layer = 0;
    uint32 m_width = 0;
    uint32 m_height = 0;
    std::array<Layer, MAX_LAYERS> m_list;
};

NVGcolor blend_color(NVGcolor a, NVGcolor b, float32 t);
