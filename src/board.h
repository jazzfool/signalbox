#pragma once

#include "context.h"
#include "util.h"
#include "state.h"
#include "space.h"
#include "filter.h"
#include "filters/executor.h"

#include <vector>
#include <array>
#include <memory>
#include <map>
#include <string_view>

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

    board& register_filter(filter_fn fn);

  private:
    struct filter_info final {
        filter_fn fn;
        std::string name;
        std::string in;
        std::string out;
    };

    void add_filter(std::unique_ptr<filter_base>&& filter, bool decoding = false);
    const filter_info* find_filter(std::string_view name);

    space new_spacef(const rect2<float32>& rect);
    void draw_frame();
    void reset_layout();

    context m_cx;

    board_config m_config;
    bool m_frame0;

    float32 m_dpi_scale;
    rect2<float32> m_layout_rect;
    vector2<float32> m_layout_cursor;
    float32 m_max_layout_height;
    uint32 m_panel_width;
    std::optional<std::unique_ptr<filter_base>> m_dragging_filter;

    input_state m_input;
    void* m_focus;
    bool m_did_focus;

    std::string m_filter_search;
    std::map<filter_kind, std::vector<filter_info>> m_all_filters;
    size_t m_filter_count;
    filter_executor m_filter_executor;
    filter_list m_filters;
    std::string m_executor_status;
};
