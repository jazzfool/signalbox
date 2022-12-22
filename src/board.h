#pragma once

#include "util.h"
#include "state.h"
#include "space.h"
#include "filter.h"
#include "filters/executor.h"

#include <vector>
#include <array>
#include <memory>

struct GLFWwindow;

class board final {
  public:
    board();
    board& create();
    void destroy();

    board& run_loop();

    space make_space(const vector2<uint32>& text_size);
    space make_spacef(const vector2<float32>& size);

    const board_config& config() const;

    board& add_filter(std::unique_ptr<filter_base>&& filter);

    template <typename TDataIn, typename TDataOut>
    board& add_filter(filter<TDataIn, TDataOut>&& filter) {
        auto lock = std::lock_guard{m_filters.mut};
        return add_filter(std::make_unique<filter_fwd<TDataIn, TDataOut>>(std::move(filter)));
    }

  private:
    void draw_frame();
    void reset_layout();

    board_config m_config;
    GLFWwindow* m_window;
    NVGcontext* m_nvg;
    bool m_frame0;

    float32 m_dpi_scale;
    rect2<float32> m_layout_rect;
    vector2<float32> m_layout_cursor;
    float32 m_max_layout_height;

    input_state m_input;
    void* m_focus;

    filter_executor m_filter_executor;
    filter_list m_filters;
    std::string m_executor_status;
};
