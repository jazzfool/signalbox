#pragma once

#include "util.h"
#include "state.h"

#include <string>
#include <functional>

struct GLFWwindow;
struct NVGcontext;

class space final {
  public:
    space(
        GLFWwindow* window, NVGcontext* nvg, void*& focus, bool& did_focus, input_state& input,
        board_config config, rect2<float32> rect,
        rect2<float32> scissor = {{-FLT_MAX, -FLT_MAX}, {FLT_MAX, FLT_MAX}}, bool subspace = false);

    void begin();
    void end();

    space subspace(const rect2<float32>& rect, const rect2<float32>& scissor);

    rect2<float32> measure(const std::string& s);
    void write(const std::string& s);
    bool write_button(const std::string& s);
    bool write_hover(const std::string& s, NVGcolor bg, NVGcolor fg);
    void next_line();
    rect2<float32> draw_block(uint32 lines);

    float32 remaining_height() const;

    GLFWwindow* window() const;
    NVGcontext* nvg() const;
    input_state& input();
    const board_config& config() const;

    rect2<float32> rect() const;
    rect2<float32> scissor() const;
    vector2<float32> layout_cursor() const;
    void set_rtl(bool rtl);
    bool rtl() const;

    void set_color(NVGcolor color);
    NVGcolor color() const;
    void set_bold(bool bold);
    void set_sans(bool sans);

    void*& focus;
    bool& did_focus;

    std::function<void(std::string_view, std::function<void(filter_base*)>&&)> cb_add_filter;

  private:
    float32 advance_layout_x(float32 x);
    const char* font_face() const;

    GLFWwindow* m_window;
    NVGcontext* m_nvg;
    input_state& m_input;
    board_config m_config;

    vector2<float32> m_layout_cursor;
    float32 m_available_width;
    bool m_rtl;
    rect2<float32> m_outer_rect;
    rect2<float32> m_rect;

    bool m_subspace;
    rect2<float32> m_scissor;

    NVGcolor m_color;
    bool m_bold;
    bool m_sans;
};
