#include "draw.h"

#include <nanovg.h>
#include <stack>
#include <spdlog/spdlog.h>

const char* get_font_name(Draw_Font font) {
    static const char* font_names[(size_t)Draw_Font::_Max] = {"mono", "monoB", "sans", "sansB"};
    return font_names[(size_t)font];
}

int32 get_text_align(Text_Align align) {
    switch (align) {
    case Text_Align::Center_Middle:
        return NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE;
    case Text_Align::Left_Middle:
        return NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE;
    default:
        return 0;
    }
}

NVGcolor blend_color(NVGcolor a, NVGcolor b, float32 t) {
    return nvgRGBAf(lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t));
}

Draw_List::Draw_List(NVGcontext* nvg) : m_nvg{nvg} {
}

void Draw_List::reset(uint32 width, uint32 height) {
    m_layer = 0;
    m_width = width;
    m_height = height;
    for (auto& layer : m_list) {
        layer.cmds.clear();
        layer.clip_stack.clear();
        layer.clip_stack.push_back(Rect2_F32{{0.f, 0.f}, {(float32)width, (float32)height}});
    }
}

void Draw_List::execute() {
    auto draw_visitor = Lambda_Overloads{
        [&](const Cmd_Rect& cmd) {
            nvgBeginPath(m_nvg);
            nvgRect(m_nvg, NVG_RECT_ARGS(cmd.rect));
            cmd.paint.apply(m_nvg, cmd.rect);
        },
        [&](const Cmd_RRect& cmd) {
            nvgBeginPath(m_nvg);
            nvgRoundedRect(m_nvg, NVG_RECT_ARGS(cmd.rect), cmd.radius);
            cmd.paint.apply(m_nvg, cmd.rect);
        },
        [&](const Cmd_Circle& cmd) {
            nvgBeginPath(m_nvg);
            nvgCircle(m_nvg, cmd.center.x, cmd.center.y, cmd.radius);
            cmd.paint.apply(
                m_nvg, {cmd.center - Vector2_F32{cmd.radius, cmd.radius},
                        2.f * Vector2_F32{cmd.radius, cmd.radius}});
        },
        [&](const Cmd_Line& cmd) {
            nvgBeginPath(m_nvg);
            nvgMoveTo(m_nvg, cmd.p0.x, cmd.p0.y);
            nvgLineTo(m_nvg, cmd.p1.x, cmd.p1.y);
            cmd.paint.apply(m_nvg, Rect2_F32::from_point_fit(cmd.p0, cmd.p1));
        },
        [&](const Cmd_Text& cmd) {
            nvgFontFace(m_nvg, get_font_name(cmd.font));
            nvgFontSize(m_nvg, cmd.size);
            nvgFillColor(m_nvg, cmd.color);
            nvgTextAlign(m_nvg, get_text_align(cmd.align));
            float32 ascender = 0.f;
            nvgTextMetrics(m_nvg, &ascender, nullptr, nullptr);
            nvgText(m_nvg, cmd.pos.x, cmd.pos.y + ascender / 2.f, cmd.text.c_str(), nullptr);
        },
        [&](const Cmd_Clip& cmd) { nvgScissor(m_nvg, NVG_RECT_ARGS(cmd.rect)); },
    };

    for (const auto& layer : m_list) {
        nvgSave(m_nvg);
        for (const auto& cmd : layer.cmds) {
            std::visit(draw_visitor, cmd);
        }
        nvgRestore(m_nvg);
    }
}

void Draw_List::push_layer() {
    if (m_layer < MAX_LAYERS - 1)
        ++m_layer;
}

void Draw_List::pop_layer() {
    if (m_layer > 0)
        --m_layer;
}

void Draw_List::push_clip_rect(const Rect2_F32& rect) {
    auto& layer = m_list[m_layer];

    layer.clip_stack.push_back(layer.clip_stack.back().rect_intersect(rect));

    Cmd_Clip clip;
    clip.rect = layer.clip_stack.back();

    layer.cmds.emplace_back(clip);
}

void Draw_List::pop_clip_rect() {
    auto& layer = m_list[m_layer];

    if (layer.clip_stack.size() < 2) {
        spdlog::error("clip pop with no push");
        return;
    }

    layer.clip_stack.pop_back();

    Cmd_Clip clip;
    clip.rect = layer.clip_stack.back();

    layer.cmds.emplace_back(clip);
}

Rect2_F32 Draw_List::clip_rect() const {
    return m_list[m_layer].clip_stack.back();
}

void Draw_List::fill_rect(const Rect2_F32& rect, NVGcolor color) {
    Cmd_Rect cmdrect;
    cmdrect.rect = rect;
    cmdrect.paint.color = color;

    m_list[m_layer].cmds.emplace_back(cmdrect);
}

void Draw_List::stroke_rect(const Rect2_F32& rect, NVGcolor color, float32 stroke_width) {
    Cmd_Rect cmdrect;
    cmdrect.rect = rect;
    cmdrect.paint.color = color;

    m_list[m_layer].cmds.emplace_back(cmdrect);
}

void Draw_List::tb_grad_fill_rect(const Rect2_F32& rect, NVGcolor from, NVGcolor to) {
    Cmd_Rect cmdrect;
    cmdrect.rect = rect;
    cmdrect.paint.gradient = true;
    cmdrect.paint.gradient_from = from;
    cmdrect.paint.gradient_to = to;

    m_list[m_layer].cmds.emplace_back(cmdrect);
}

void Draw_List::fill_rrect(const Rect2_F32& rect, float32 radius, NVGcolor color) {
    Cmd_RRect rrect;
    rrect.rect = rect;
    rrect.radius = radius;
    rrect.paint.color = color;

    m_list[m_layer].cmds.emplace_back(rrect);
}

void Draw_List::stroke_rrect(const Rect2_F32& rect, float32 radius, NVGcolor color, float32 stroke_width) {
    Cmd_RRect rrect;
    rrect.rect = rect;
    rrect.radius = radius;
    rrect.paint.stroke = true;
    rrect.paint.width = stroke_width;
    rrect.paint.color = color;

    m_list[m_layer].cmds.emplace_back(rrect);
}

void Draw_List::tb_grad_fill_rrect(const Rect2_F32& rect, float32 radius, NVGcolor from, NVGcolor to) {
    Cmd_RRect cmdrrect;
    cmdrrect.rect = rect;
    cmdrrect.radius = radius;
    cmdrrect.paint.gradient = true;
    cmdrrect.paint.gradient_from = from;
    cmdrrect.paint.gradient_to = to;

    m_list[m_layer].cmds.emplace_back(cmdrrect);
}

void Draw_List::fill_circle(Vector2_F32 center, float32 radius, NVGcolor color) {
    Cmd_Circle cmdcircle;
    cmdcircle.center = center;
    cmdcircle.radius = radius;
    cmdcircle.paint.color = color;

    m_list[m_layer].cmds.emplace_back(cmdcircle);
}

void Draw_List::tb_grad_fill_circle(Vector2_F32 center, float32 radius, NVGcolor from, NVGcolor to) {
    Cmd_Circle cmdcircle;
    cmdcircle.center = center;
    cmdcircle.radius = radius;
    cmdcircle.paint.gradient = true;
    cmdcircle.paint.gradient_from = from;
    cmdcircle.paint.gradient_to = to;

    m_list[m_layer].cmds.emplace_back(cmdcircle);
}

void Draw_List::stroke_line(Vector2_F32 p0, Vector2_F32 p1, NVGcolor color, float32 stroke_width) {
    Cmd_Line cmdline;
    cmdline.p0 = p0;
    cmdline.p1 = p1;
    cmdline.paint.stroke = true;
    cmdline.paint.width = stroke_width;
    cmdline.paint.color = color;

    m_list[m_layer].cmds.emplace_back(cmdline);
}

Vector2_F32
Draw_List::measure_text(std::string_view text, Draw_Font font, float32 size, float32* advance) const {
    nvgFontFace(m_nvg, get_font_name(font));
    nvgFontSize(m_nvg, size);

    float32 bounds[4];
    const auto adv = nvgTextBounds(m_nvg, 0.f, 0.f, text.data(), text.data() + text.size(), bounds);

    if (advance) {
        *advance = adv;
    }

    return {bounds[2] - bounds[0], bounds[3] - bounds[1]};
}

float32 Draw_List::line_height(Draw_Font font, float32 size) const {
    nvgFontFace(m_nvg, get_font_name(font));
    nvgFontSize(m_nvg, size);

    float32 lh = 0.f;
    nvgTextMetrics(m_nvg, nullptr, nullptr, &lh);

    return lh;
}

void Draw_List::text(
    Vector2_F32 pos, Text_Align align, std::string_view text, NVGcolor color, Draw_Font font, float32 size) {
    Cmd_Text cmdtext;
    cmdtext.pos = pos;
    cmdtext.align = align;
    cmdtext.color = color;
    cmdtext.text = std::string{text};
    cmdtext.font = font;
    cmdtext.size = size;

    m_list[m_layer].cmds.emplace_back(cmdtext);
}

void Draw_List::Cmd_Paint::apply(NVGcontext* nvg, const Rect2_F32& rect) const {
    if (gradient) {
        const auto paint = nvgLinearGradient(
            nvg, rect.pos.x, rect.pos.y, rect.pos.x, rect.max().y, gradient_from, gradient_to);
        if (stroke) {
            nvgStrokePaint(nvg, paint);
        } else {
            nvgFillPaint(nvg, paint);
        }
    } else {
        if (stroke) {
            nvgStrokeColor(nvg, color);
        } else {
            nvgFillColor(nvg, color);
        }
    }

    if (stroke) {
        nvgStrokeWidth(nvg, width);
        nvgStroke(nvg);
    } else {
        nvgFill(nvg);
    }
}
