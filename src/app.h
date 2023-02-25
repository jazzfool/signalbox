#pragma once

#include "context.h"
#include "util.h"
#include "tracker/tracker.h"
#include "draw.h"

#include <optional>

struct GLFWwindow;

class App final {
  public:
    App();
    App& create();
    void destroy();

    App& run_loop();

  private:
    void ui();
    void draw_frame();

    Context m_cx;
    float32 m_dpi_scale;
    std::optional<Draw_List> m_draw;

    Tracker m_tracker;
};
