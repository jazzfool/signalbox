#pragma once

#include "util.h"
#include "state.h"
#include "space.h"
#include "filter.h"

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

    space make_space(const vector2<uint32>& textSize);

    const board_config& config() const;

    template<typename T>
    board& filter(filter<T, stereo>&& filter) {
        auto fwd = filter_fwd<T, stereo>{std::move(filter)};
        m_filters.push_back(std::move(std::make_unique<decltype(fwd)>(fwd)));
        return *this;
    }

  private:
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

    std::vector<std::unique_ptr<filter_base<stereo>>> m_filters;
};
