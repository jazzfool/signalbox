#include "draw.h"

#include <nanovg.h>

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

void Draw_List::reset() {
    m_layer = 0;
    for (auto& layer : m_list) {
        layer.clear();
    }
}

void Draw_List::execute() {
    for (const auto& layer : m_list) {
        for (const auto& cmd : layer) {
            switch (cmd.type) {
            case Command::rrect: {
                const auto& cmdrrect = std::get<Cmd_RRect>(cmd.cmd);

                nvgBeginPath(m_nvg);
                nvgRoundedRect(m_nvg, NVG_RECT_ARGS(cmdrrect.rect), cmdrrect.radius);
                cmdrrect.paint.apply(m_nvg, cmdrrect.rect);

                break;
            }
            case Command::circle: {
                const auto& cmdcircle = std::get<Cmd_Circle>(cmd.cmd);

                nvgBeginPath(m_nvg);
                nvgCircle(m_nvg, cmdcircle.center.x, cmdcircle.center.y, cmdcircle.radius);
                cmdcircle.paint.apply(
                    m_nvg, {cmdcircle.center - Vector2_F32{cmdcircle.radius, cmdcircle.radius},
                            2.f * Vector2_F32{cmdcircle.radius, cmdcircle.radius}});

                break;
            }
            case Command::line: {
                const auto& cmdline = std::get<Cmd_Line>(cmd.cmd);

                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, cmdline.p0.x, cmdline.p0.y);
                nvgLineTo(m_nvg, cmdline.p1.x, cmdline.p1.y);
                cmdline.paint.apply(m_nvg, Rect2_F32::from_point_fit(cmdline.p0, cmdline.p1));

                break;
            }
            case Command::text: {
                const auto& cmdtext = std::get<Cmd_Text>(cmd.cmd);

                nvgFontFace(m_nvg, get_font_name(cmdtext.font));
                nvgFontSize(m_nvg, cmdtext.size);
                nvgFillColor(m_nvg, cmdtext.color);
                nvgTextAlign(m_nvg, get_text_align(cmdtext.align));
                float32 ascender = 0.f;
                nvgTextMetrics(m_nvg, &ascender, nullptr, nullptr);
                nvgText(m_nvg, cmdtext.pos.x, cmdtext.pos.y + ascender / 2.f, cmdtext.text.c_str(), nullptr);

                break;
            }
            default:
                break;
            }
        }
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

void Draw_List::fill_rrect(const Rect2_F32& rect, float32 radius, NVGcolor color) {
    Cmd_RRect rrect;
    rrect.rect = rect;
    rrect.radius = radius;
    rrect.paint.color = color;

    Command cmd;
    cmd.type = Command::rrect;
    cmd.cmd = rrect;

    m_list[m_layer].push_back(cmd);
}

void Draw_List::stroke_rrect(const Rect2_F32& rect, float32 radius, NVGcolor color, float32 stroke_width) {
    Cmd_RRect rrect;
    rrect.rect = rect;
    rrect.radius = radius;
    rrect.paint.stroke = true;
    rrect.paint.width = stroke_width;
    rrect.paint.color = color;

    Command cmd;
    cmd.type = Command::rrect;
    cmd.cmd = rrect;

    m_list[m_layer].push_back(cmd);
}

void Draw_List::tb_grad_fill_rrect(const Rect2_F32& rect, float32 radius, NVGcolor from, NVGcolor to) {
    Cmd_RRect cmdrrect;
    cmdrrect.rect = rect;
    cmdrrect.radius = radius;
    cmdrrect.paint.gradient = true;
    cmdrrect.paint.gradient_from = from;
    cmdrrect.paint.gradient_to = to;

    Command cmd;
    cmd.type = Command::rrect;
    cmd.cmd = cmdrrect;

    m_list[m_layer].push_back(cmd);
}

void Draw_List::fill_circle(Vector2_F32 center, float32 radius, NVGcolor color) {
    Cmd_Circle cmdcircle;
    cmdcircle.center = center;
    cmdcircle.radius = radius;
    cmdcircle.paint.color = color;

    Command cmd;
    cmd.type = Command::circle;
    cmd.cmd = cmdcircle;

    m_list[m_layer].push_back(cmd);
}

void Draw_List::tb_grad_fill_circle(Vector2_F32 center, float32 radius, NVGcolor from, NVGcolor to) {
    Cmd_Circle cmdcircle;
    cmdcircle.center = center;
    cmdcircle.radius = radius;
    cmdcircle.paint.gradient = true;
    cmdcircle.paint.gradient_from = from;
    cmdcircle.paint.gradient_to = to;

    Command cmd;
    cmd.type = Command::circle;
    cmd.cmd = cmdcircle;

    m_list[m_layer].push_back(cmd);
}

void Draw_List::stroke_line(Vector2_F32 p0, Vector2_F32 p1, NVGcolor color, float32 stroke_width) {
    Cmd_Line cmdline;
    cmdline.p0 = p0;
    cmdline.p1 = p1;
    cmdline.paint.stroke = true;
    cmdline.paint.width = stroke_width;
    cmdline.paint.color = color;

    Command cmd;
    cmd.type = Command::line;
    cmd.cmd = cmdline;

    m_list[m_layer].push_back(cmd);
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
    cmdtext.text = text;
    cmdtext.font = font;
    cmdtext.size = size;

    Command cmd;
    cmd.type = Command::text;
    cmd.cmd = cmdtext;

    m_list[m_layer].push_back(cmd);
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
