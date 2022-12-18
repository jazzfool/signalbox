#include "Space.h"

#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <glm/mat3x3.hpp>
#include <glm/gtc/type_ptr.hpp>

space::space(GLFWwindow* window, NVGcontext* nvg, input_state input, board_config config, rect2<float32> rect)
    : m_window{window}, m_nvg{nvg}, m_input{input}, m_config{config}, m_outer_rect{rect} {
    m_rect = m_outer_rect.inflate(-m_config.inner_padding);
    m_layout_cursor = m_rect.pos;
    m_available_width = m_rect.size.x;
    m_rtl = false;

    m_color = m_config.colors.fg;
}

void space::begin() {
    nvgSave(m_nvg);
    nvgIntersectScissor(m_nvg, m_outer_rect.pos.x, m_outer_rect.pos.y, m_outer_rect.size.x,
                        m_outer_rect.size.y);
    nvgSave(m_nvg);
}

void space::end() {
    nvgRestore(m_nvg);
    nvgRestore(m_nvg);

    nvgSave(m_nvg);
    nvgBeginPath(m_nvg);
    nvgRoundedRect(m_nvg, m_outer_rect.pos.x, m_outer_rect.pos.y, m_outer_rect.size.x, m_outer_rect.size.y,
                   4.f);
    nvgStrokeColor(m_nvg, m_config.colors.frame);
    nvgStrokeWidth(m_nvg, 1.5f);
    nvgStroke(m_nvg);
    nvgRestore(m_nvg);
}

rect2<float32> space::measure(const std::string& s) {
    if (s.empty())
        return {};

    nvgFontFace(m_nvg, "mono");
    nvgFontSize(m_nvg, m_config.font_size);
    nvgTextAlign(m_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    float32 bounds[4];
    const float32 advance = nvgTextBounds(m_nvg, 0.f, 0.f, s.c_str(), nullptr, bounds);

    const auto x = m_layout_cursor.x - (float32)m_rtl * advance;
    const auto y = m_layout_cursor.y + m_config.line_height / 2.f;

    auto bounds_rect = rect2<float32>{{bounds[0], bounds[1]}, {bounds[2] - bounds[0], bounds[3] - bounds[1]}};
    bounds_rect = bounds_rect.translate({x, y});

    return bounds_rect;
}

void space::write(const std::string& s) {
    if (s.empty())
        return;

    float32 bounds[4];
    const float32 advance = nvgTextBounds(m_nvg, 0.f, 0.f, s.c_str(), nullptr, bounds);

    const float32 x = advance_layout_x(advance);
    const float32 y = m_layout_cursor.y + m_config.line_height / 2.f;

    nvgFontFace(m_nvg, font_face());
    nvgFontSize(m_nvg, m_config.font_size);
    nvgTextAlign(m_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgFillColor(m_nvg, m_color);
    nvgText(m_nvg, x, y, s.c_str(), nullptr);
}

bool space::write_button(const std::string& s) {
    return write_hover(s, m_color, m_config.colors.bg) && m_input.mouse_just_pressed[0];
}

bool space::write_hover(const std::string& s, NVGcolor bg, NVGcolor fg) {
    if (s.empty())
        return false;

    float32 bounds[4];
    const float32 advance = nvgTextBounds(m_nvg, 0.f, 0.f, s.c_str(), nullptr, bounds);

    const float32 x = advance_layout_x(advance);
    const float32 y = m_layout_cursor.y + m_config.line_height / 2.f;

    auto bounds_rect = rect2<float32>{{bounds[0], bounds[1]}, {bounds[2] - bounds[0], bounds[3] - bounds[1]}};
    bounds_rect = bounds_rect.translate({x, y});

    const auto mouse_over = bounds_rect.contains(m_input.cursor_pos);
    const auto font = font_face();

    nvgFontFace(m_nvg, font);
    nvgFontSize(m_nvg, m_config.font_size);
    nvgTextAlign(m_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    if (mouse_over) {
        nvgBeginPath(m_nvg);
        nvgRoundedRect(m_nvg, bounds_rect.pos.x, m_layout_cursor.y, bounds_rect.size.x, m_config.line_height,
                       2.f);
        nvgFillColor(m_nvg, bg);
        nvgFill(m_nvg);
    }

    nvgFillColor(m_nvg, mouse_over ? fg : m_color);
    nvgText(m_nvg, x, y, s.c_str(), nullptr);

    return mouse_over;
}

void space::next_line() {
    m_layout_cursor.x = m_rect.pos.x + (float32)m_rtl * m_rect.size.x;
    m_layout_cursor.y += m_config.line_height;
    m_available_width = m_rect.size.x;
}

rect2<float32> space::draw_block(uint32 lines) {
    const auto rtl = m_rtl;
    set_rtl(false);

    if (!float_eq(m_layout_cursor.x, 0.f))
        next_line();

    const auto height = std::min(remaining_height(), m_config.line_height * static_cast<float32>(lines));
    const auto out_rect = rect2<float32>{m_layout_cursor, {m_rect.size.x, height}};

    m_layout_cursor.x = m_rect.pos.x;
    m_layout_cursor.y += height;

    set_rtl(rtl);

    return out_rect;
}

float32 space::remaining_height() const {
    return m_rect.max().y - m_layout_cursor.y;
}

GLFWwindow* space::window() const {
    return m_window;
}

NVGcontext* space::nvg() const {
    return m_nvg;
}

const input_state& space::input() const {
    return m_input;
}

const board_config& space::config() const {
    return m_config;
}

rect2<float32> space::rect() const {
    return m_rect;
}

void space::set_rtl(bool rtl) {
    if (m_rtl == rtl)
        return;
    m_rtl = rtl;
    m_layout_cursor.x += (float32)((m_rtl << 1) - 1) * m_available_width;
}

bool space::rtl() const {
    return m_rtl;
}

void space::set_color(NVGcolor color) {
    m_color = color;
}

NVGcolor space::color() const {
    return m_color;
}

void space::set_bold(bool bold) {
    m_bold = bold;
}

float32 space::advance_layout_x(float32 x) {
    const auto out_x = m_layout_cursor.x;
    m_available_width -= x;
    m_layout_cursor.x += (float32)(((int8)(!m_rtl) << 1) - 1) * x;
    return out_x - (float32)m_rtl * x;
}

const char* space::font_face() const {
    return m_bold ? "monoB" : "mono";
}
