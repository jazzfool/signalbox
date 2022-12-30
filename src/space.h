#pragma once

#include "util.h"
#include "state.h"

#include <string>

struct GLFWwindow;
struct NVGcontext;

class space final {
  public:
    space(
        GLFWwindow* window, NVGcontext* nvg, void*& focus, bool& did_focus, input_state input,
        board_config config, rect2<float32> rect);

    void begin();
    void end();

    rect2<float32> measure(const std::string& s);
    void write(const std::string& s);
    bool write_button(const std::string& s);
    bool write_hover(const std::string& s, NVGcolor bg, NVGcolor fg);
    void next_line();
    rect2<float32> draw_block(uint32 lines);

    float32 remaining_height() const;

    GLFWwindow* window() const;
    NVGcontext* nvg() const;
    const input_state& input() const;
    const board_config& config() const;

    rect2<float32> rect() const;
    void set_rtl(bool rtl);
    bool rtl() const;

    void set_color(NVGcolor color);
    NVGcolor color() const;
    void set_bold(bool bold);

    void*& focus;
    bool& did_focus;

  private:
    float32 advance_layout_x(float32 x);
    const char* font_face() const;

    GLFWwindow* m_window;
    NVGcontext* m_nvg;
    input_state m_input;
    board_config m_config;

    vector2<float32> m_layout_cursor;
    float32 m_available_width;
    bool m_rtl;
    rect2<float32> m_outer_rect;
    rect2<float32> m_rect;

    NVGcolor m_color;
    bool m_bold;
};
