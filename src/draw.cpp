#include "draw.h"

#include <nanovg.h>

const char* get_font_name(draw_font font) {
    static const char* font_names[(size_t)draw_font::_max] = {"mono", "monoB", "sans", "sansB"};
    return font_names[(size_t)font];
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
            case command::text: {
                const auto& cmdtext = std::get<cmd_text>(cmd.cmd);

                nvgFontFace(m_nvg, get_font_name(cmdtext.font));
                nvgFontSize(m_nvg, cmdtext.size);
                nvgFillColor(m_nvg, cmdtext.color);
                nvgTextAlign(m_nvg, cmdtext.align);
                nvgText(m_nvg, cmdtext.pos.x, cmdtext.pos.y, cmdtext.text.c_str(), nullptr);

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

vector2f32 draw_list::measure_text(std::string_view text, draw_font font, float32 size) const {
    nvgFontFace(m_nvg, get_font_name(font));
    nvgFontSize(m_nvg, size);

    float32 bounds[4];
    nvgTextBounds(m_nvg, 0.f, 0.f, text.data(), nullptr, bounds);

    return {bounds[2] - bounds[0], bounds[3] - bounds[1]};
}

void draw_list::centered_text(
    vector2f32 pos, std::string_view text, NVGcolor color, draw_font font, float32 size) {
    cmd_text cmdtext;
    cmdtext.pos = pos;
    cmdtext.align = NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE;
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
