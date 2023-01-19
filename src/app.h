#pragma once

#include "context.h"
#include "util.h"
#include "state.h"
#include "ui.h"
#include "tracker/tracker.h"

struct GLFWwindow;

class app final {
  public:
    app();
    app& create();
    void destroy();

    app& run_loop();

  private:
    void ui();
    void draw_frame();

    context m_cx;
    float32 m_dpi_scale;
    input_state m_input;
    ui_memory m_memory;
    ui_key m_focus;
    ui_options m_ui_opts;
    ui_colors m_ui_colors;

    tracker m_tracker;
};
