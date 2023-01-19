#include "draw.h"

#include <nanovg.h>

const char* get_font_name(draw_font font) {
    static const char* font_names[(size_t)draw_font::_max] = {"mono", "monoB", "sans", "sansB"};
    return font_names[(size_t)font];
}

int32 get_text_align(text_align align) {
    switch (align) {
    case text_align::center_middle:
        return NVG_ALIGN_CENTER | NVG_ALIGN_BASELINE;
    case text_align::left_middle:
        return NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE;
    default:
        return 0;
    }
}

NVGcolor blend_color(NVGcolor a, NVGcolor b, float32 t) {
    return nvgRGBAf(lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t));
}

draw_list::draw_list(NVGcontext* nvg) : m_nvg{nvg} {
}

void draw_list::execute() {
    for (const auto& layer : m_list) {
        for (const auto& cmd : layer) {
            switch (cmd.type) {
            case command::rrect: {
                const auto& cmdrrect = std::get<cmd_rrect>(cmd.cmd);

                nvgBeginPath(m_nvg);
                nvgRoundedRect(m_nvg, NVG_RECT_ARGS(cmdrrect.rect), cmdrrect.radius);
                cmdrrect.paint.apply(m_nvg, cmdrrect.rect);

                break;
            }
            case command::circle: {
                const auto& cmdcircle = std::get<cmd_circle>(cmd.cmd);

                nvgBeginPath(m_nvg);
                nvgCircle(m_nvg, cmdcircle.center.x, cmdcircle.center.y, cmdcircle.radius);
                cmdcircle.paint.apply(
                    m_nvg, {cmdcircle.center - vector2f32{cmdcircle.radius, cmdcircle.radius},
                            2.f * vector2f32{cmdcircle.radius, cmdcircle.radius}});

                break;
            }
            case command::line: {
                const auto& cmdline = std::get<cmd_line>(cmd.cmd);

                nvgBeginPath(m_nvg);
                nvgMoveTo(m_nvg, cmdline.p0.x, cmdline.p0.y);
                nvgLineTo(m_nvg, cmdline.p1.x, cmdline.p1.y);
                cmdline.paint.apply(m_nvg, rect2f32::from_point_fit(cmdline.p0, cmdline.p1));

                break;
            }
            case command::text: {
                const auto& cmdtext = std::get<cmd_text>(cmd.cmd);

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

void draw_list::push_layer() {
    if (m_layer < MAX_LAYERS - 1)
        ++m_layer;
}

void draw_list::pop_layer() {
    if (m_layer > 0)
        --m_layer;
}

void draw_list::fill_rrect(const rect2f32& rect, float32 radius, NVGcolor color) {
    cmd_rrect rrect;
    rrect.rect = rect;
    rrect.radius = radius;
    rrect.paint.color = color;

    command cmd;
    cmd.type = command::rrect;
    cmd.cmd = rrect;

    m_list[m_layer].push_back(cmd);
}

void draw_list::stroke_rrect(const rect2f32& rect, float32 radius, NVGcolor color, float32 stroke_width) {
    cmd_rrect rrect;
    rrect.rect = rect;
    rrect.radius = radius;
    rrect.paint.stroke = true;
    rrect.paint.width = stroke_width;
    rrect.paint.color = color;

    command cmd;
    cmd.type = command::rrect;
    cmd.cmd = rrect;

    m_list[m_layer].push_back(cmd);
}

void draw_list::tb_grad_fill_rrect(const rect2f32& rect, float32 radius, NVGcolor from, NVGcolor to) {
    cmd_rrect cmdrrect;
    cmdrrect.rect = rect;
    cmdrrect.radius = radius;
    cmdrrect.paint.gradient = true;
    cmdrrect.paint.gradient_from = from;
    cmdrrect.paint.gradient_to = to;

    command cmd;
    cmd.type = command::rrect;
    cmd.cmd = cmdrrect;

    m_list[m_layer].push_back(cmd);
}

void draw_list::fill_circle(vector2f32 center, float32 radius, NVGcolor color) {
    cmd_circle cmdcircle;
    cmdcircle.center = center;
    cmdcircle.radius = radius;
    cmdcircle.paint.color = color;

    command cmd;
    cmd.type = command::circle;
    cmd.cmd = cmdcircle;

    m_list[m_layer].push_back(cmd);
}

void draw_list::tb_grad_fill_circle(vector2f32 center, float32 radius, NVGcolor from, NVGcolor to) {
    cmd_circle cmdcircle;
    cmdcircle.center = center;
    cmdcircle.radius = radius;
    cmdcircle.paint.gradient = true;
    cmdcircle.paint.gradient_from = from;
    cmdcircle.paint.gradient_to = to;

    command cmd;
    cmd.type = command::circle;
    cmd.cmd = cmdcircle;

    m_list[m_layer].push_back(cmd);
}

void draw_list::stroke_line(vector2f32 p0, vector2f32 p1, NVGcolor color, float32 stroke_width) {
    cmd_line cmdline;
    cmdline.p0 = p0;
    cmdline.p1 = p1;
    cmdline.paint.stroke = true;
    cmdline.paint.width = stroke_width;
    cmdline.paint.color = color;

    command cmd;
    cmd.type = command::line;
    cmd.cmd = cmdline;

    m_list[m_layer].push_back(cmd);
}

vector2f32 draw_list::measure_text(std::string_view text, draw_font font, float32 size) const {
    nvgFontFace(m_nvg, get_font_name(font));
    nvgFontSize(m_nvg, size);

    float32 bounds[4];
    nvgTextBounds(m_nvg, 0.f, 0.f, text.data(), nullptr, bounds);

    return {bounds[2] - bounds[0], bounds[3] - bounds[1]};
}

float32 draw_list::line_height(draw_font font, float32 size) const {
    nvgFontFace(m_nvg, get_font_name(font));
    nvgFontSize(m_nvg, size);

    float32 lh = 0.f;
    nvgTextMetrics(m_nvg, nullptr, nullptr, &lh);

    return lh;
}

void draw_list::text(
    vector2f32 pos, text_align align, std::string_view text, NVGcolor color, draw_font font, float32 size) {
    cmd_text cmdtext;
    cmdtext.pos = pos;
    cmdtext.align = align;
    cmdtext.color = color;
    cmdtext.text = text;
    cmdtext.font = font;
    cmdtext.size = size;

    command cmd;
    cmd.type = command::text;
    cmd.cmd = cmdtext;

    m_list[m_layer].push_back(cmd);
}

void draw_list::cmd_paint::apply(NVGcontext* nvg, const rect2f32& rect) const {
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
