#pragma once

#include "context.h"
#include "util.h"
#include "state.h"
#include "ui.h"

struct GLFWwindow;

class tracker final : ui_view<> {
  public:
    tracker();
    tracker& create();
    void destroy();

    tracker& run_loop();

    void view(ui_state& state, ui_layout& layout) override;

  private:
    void draw_frame();

    context m_cx;
    float32 m_dpi_scale;
    input_state m_input;
    ui_memory m_memory;
    ui_key m_focus;
    bool m_focus_taken;
    ui_options m_ui_opts;
    ui_colors m_ui_colors;
};
