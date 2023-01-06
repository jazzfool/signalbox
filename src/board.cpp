#include "board.h"

#include "util.h"
#include "widget.h"
#include "enc.h"

#include <spdlog/spdlog.h>
#include <map>
#include <nfd.hpp>
#include <fstream>

#ifdef __APPLE__
#define SB_DPI_SCALE(X) 1.f
#else
#define SB_DPI_SCALE(X) X
#endif

board::board() : m_filter_executor{m_filters} {
    m_focus = nullptr;
    m_did_focus = false;
    m_frame0 = true;
    m_max_layout_height = 0.f;
    m_executor_status = "INACTIVE";
    m_panel_width = 30;
    m_filter_count = 0;
}

board& board::create() {
    m_cx = create_context();

    glfwSetWindowUserPointer(m_cx.window, this);

    glfwGetWindowContentScale(m_cx.window, &m_dpi_scale, nullptr);
    glfwSetWindowContentScaleCallback(m_cx.window, [](GLFWwindow* window, float32 xScale, float32 yScale) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_dpi_scale = xScale;
    });

    glfwSetCursorPosCallback(m_cx.window, [](GLFWwindow* window, float64 x, float64 y) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_input.cursor_pos = vector2<float32>{
            static_cast<float32>(x) / SB_DPI_SCALE(b.m_dpi_scale),
            static_cast<float32>(y) / SB_DPI_SCALE(b.m_dpi_scale)};
    });

    glfwSetMouseButtonCallback(m_cx.window, [](GLFWwindow* window, int32 button, int32 action, int32 mods) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS)
            b.m_input.mouse_just_pressed[button] = true;
    });

    glfwSetScrollCallback(m_cx.window, [](GLFWwindow* window, float64 xScroll, float64 yScroll) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_input.scroll_wheel = static_cast<float32>(yScroll);
    });

    glfwSetCharCallback(m_cx.window, [](GLFWwindow* window, uint32 text) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_input.text = static_cast<char32>(text);
    });

    glfwSetKeyCallback(
        m_cx.window, [](GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods) {
            auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                b.m_input.keys_just_pressed[key] = true;
            }
        });

    glfwSetFramebufferSizeCallback(m_cx.window, [](GLFWwindow* window, int32, int32) {
        auto p = glfwGetWindowUserPointer(window);
        if (!p)
            return;
        auto& b = *reinterpret_cast<board*>(p);
        context_on_resize(&b.m_cx);
        b.draw_frame();
    });

    NFD::Init();

    m_config.font_size = 12.f;

    nvgCreateFont(m_cx.nvg, "mono", "data/iosevka-fixed-slab-extended.ttf");
    nvgCreateFont(m_cx.nvg, "monoB", "data/iosevka-fixed-slab-extendedbold.ttf");

    nvgFontFace(m_cx.nvg, "mono");
    nvgFontSize(m_cx.nvg, m_config.font_size);
    nvgTextAlign(m_cx.nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgTextMetrics(m_cx.nvg, &m_config.ascender, nullptr, &m_config.line_height);
    m_config.line_height *= 1.2f;

    m_config.outer_padding = 10.f;
    m_config.inner_padding = 5.f;

    m_filter_executor.create();

    return *this;
}

void board::destroy() {
    m_filter_executor.destroy();

    destroy_context(&m_cx);
}

board& board::run_loop() {
    while (!glfwWindowShouldClose(m_cx.window)) {
        draw_frame();
        glfwPollEvents();
    }

    return *this;
}

space board::make_space(const vector2<uint32>& text_size) {
    const auto size = vector2<float32>{
        static_cast<float32>(text_size.x) * m_config.char_width + 2.f * m_config.inner_padding,
        static_cast<float32>(text_size.y) * m_config.line_height + 2.f * m_config.inner_padding,
    };
    return make_spacef(size);
}

space board::make_spacef(const vector2<float32>& size) {
    if (size.x > m_layout_rect.max().x - m_layout_cursor.x) {
        m_layout_cursor.x = m_layout_rect.pos.x;
        m_layout_cursor.y += m_max_layout_height + m_config.outer_padding;
        m_max_layout_height = 0.f;
    }

    m_max_layout_height = std::max(m_max_layout_height, size.y);
    const auto space_rect = rect2<float32>{m_layout_cursor, size};
    m_layout_cursor.x += size.x + m_config.outer_padding;

    return new_spacef(space_rect);
}

const board_config& board::config() const {
    return m_config;
}

board& board::register_filter(filter_fn fn) {
    auto dummy = fn();

    auto info = filter_info{};
    info.fn = fn;
    info.name = dummy->name();

    for (const auto& [kind, filters] : m_all_filters) {
        sb_ASSERT_EQ(
            std::find_if(filters.begin(), filters.end(), [&](const auto& f) { return f.name == info.name; }),
            filters.end());
    }

    const auto fd_in = dummy->data_chans_in();
    const auto fd_out = dummy->data_chans_out();

    info.in =
        fd_in ? fd_in->chans_in_is_dynamic() ? "**" : fmt::format("{:02}", fd_in->chans_in().size()) : "..";
    info.out = fd_out
                   ? fd_out->chans_out_is_dynamic() ? "**" : fmt::format("{:02}", fd_out->chans_out().size())
                   : "..";

    m_all_filters[dummy->kind()].push_back(std::move(info));

    return *this;
}

void board::add_filter(std::unique_ptr<filter_base>&& filter, bool decoding) {
    if (!decoding)
        auto lock = std::lock_guard{m_filters.mut};

    if (!decoding) {
        const auto chan_in = filter->data_chans_in();
        const auto chan_out = filter->data_chans_out();

        if (chan_in || chan_out) {
            auto chan = uint8{0};
            auto max = uint8{0};
            auto b = false;
            for (auto& f : m_filters.filters) {
                const auto f_chan_in = f->data_chans_in();
                const auto f_chan_out = f->data_chans_out();
                if (f_chan_in) {
                    for (auto c : f_chan_in->chans_in()) {
                        max = std::max(c, max);
                    }
                }
                if (f_chan_out) {
                    b = true;
                    for (auto c : f_chan_out->chans_out()) {
                        max = std::max(c, max);
                        chan = std::max(c, chan);
                    }
                }
            }

            if (chan_in) {
                for (auto& c : chan_in->chans_in()) {
                    c = chan;
                    chan -= (uint8)(chan > 0);
                }
            }

            if (chan_out) {
                for (auto& c : chan_out->chans_out()) {
                    c = max += (uint8)(b);
                    b = true;
                    max = std::min(max, uint8{99});
                }
            }
        }
    }

    m_filters.filters.push_back(std::move(filter));
    ++m_filter_count;
}

const board::filter_info* board::find_filter(std::string_view name) {
    for (const auto& [kind, filters] : m_all_filters) {
        for (const auto& f : filters) {
            if (f.name == name)
                return &f;
        }
    }
    return nullptr;
}

space board::new_spacef(const rect2<float32>& rect) {
    return space{m_cx.window, m_cx.nvg, m_focus, m_did_focus, m_input, m_config, rect};
}

void board::draw_frame() {
    int32 width, height;
    glfwGetWindowSize(m_cx.window, &width, &height);

    int32 fb_width, fb_height;
    glfwGetFramebufferSize(m_cx.window, &fb_width, &fb_height);

    const auto clear_color = m_config.colors.bg;
    context_begin_frame(&m_cx, clear_color.r, clear_color.g, clear_color.b);

    nvgBeginFrame(m_cx.nvg, width, height, m_dpi_scale);
    nvgScale(m_cx.nvg, SB_DPI_SCALE(m_dpi_scale), SB_DPI_SCALE(m_dpi_scale));

    if (m_frame0) {
        m_frame0 = false;
        nvgFontFace(m_cx.nvg, "mono");
        nvgFontSize(m_cx.nvg, m_config.font_size);
        nvgTextAlign(m_cx.nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        m_config.char_width = nvgTextBounds(m_cx.nvg, 0.f, 0.f, " ", nullptr, nullptr);
    }

    reset_layout();

    {
        auto s = new_spacef(rect2<float32>{
            {0.f, 0.f},
            {static_cast<float32>(m_panel_width) * m_config.char_width + m_config.outer_padding,
             m_layout_rect.size.y + 2.f * m_config.outer_padding}}
                                .inflate(-m_config.outer_padding));

        s.begin();
        s.set_bold(true);
        s.set_color(m_config.colors.fg);
        s.write("FILTERS");
        s.next_line();
        s.set_bold(false);
        ui_text_in(s, m_config.colors.fg, m_filter_search, "Search...", m_panel_width - 3);
        s.next_line();

        using filter_iterator = std::vector<filter_info>::iterator;
        auto pushed = std::optional<filter_iterator>{};

        const auto draw_one_filter = [&](filter_kind kind, filter_iterator it) {
            s.set_color(m_config.colors.filters[kind]);
            s.set_rtl(false);
            if (s.write_button(it->name)) {
                pushed = it;
            }
            s.set_rtl(true);
            s.set_color(m_config.colors.semifaint);
            s.write(fmt::format("{}->{}", it->in, it->out));
            s.set_color(m_config.colors.fg);
            s.next_line();
        };

        if (m_filter_search.empty()) {
            for (auto& [kind, filters] : m_all_filters) {
                s.set_rtl(false);
                s.set_color(m_config.colors.fg);
                s.write(filter_kind_names[(size_t)kind]);
                s.next_line();

                for (auto it = filters.begin(); it != filters.end(); ++it) {
                    draw_one_filter(kind, it);
                }
            }
        } else {
            auto dists = std::vector<std::tuple<uint32, filter_kind, filter_iterator>>{};
            dists.reserve(m_filter_count);
            for (auto& [kind, filters] : m_all_filters) {
                for (auto it = filters.begin(); it != filters.end(); ++it) {
                    const auto dist = str_distance(
                        it->name.substr(0, std::min(it->name.size(), m_filter_search.size())),
                        m_filter_search);
                    dists.push_back(std::make_tuple(dist, kind, it));
                }
            }
            std::sort(dists.begin(), dists.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a) < std::get<0>(b);
            });
            for (auto& [dist, kind, it] : dists) {
                draw_one_filter(kind, it);
            }
        }

        s.end();

        if (pushed) {
            add_filter((*pushed)->fn());
        }
    }

    {
        m_filter_executor.out_status.remove(&m_executor_status);

        auto playback_l_chan = m_filter_executor.playback_l_chan.load();
        auto playback_r_chan = m_filter_executor.playback_r_chan.load();
        const auto playback_muted = m_filter_executor.playback_mute.load();

        auto capture_l_chan = m_filter_executor.capture_l_chan.load();
        auto capture_r_chan = m_filter_executor.capture_r_chan.load();
        const auto capture_muted = m_filter_executor.capture_mute.load();

        auto s =
            make_spacef({m_layout_rect.size.x, m_config.line_height * 4.f + 2.f * m_config.inner_padding});
        s.begin();
        s.set_bold(true);
        s.write(m_executor_status);
        s.set_bold(false);
        s.set_rtl(true);
        if (s.write_button("Reload")) {
            m_filter_executor.reload_device();
        }

        s.next_line();
        s.set_rtl(false);
        s.set_bold(true);
        s.write("PLAYBACK ");
        s.set_bold(false);
        ui_enum_sel(s, m_filter_executor.playback_devs(), false, m_filter_executor.playback_dev_idx);
        s.set_rtl(true);
        if (s.write_button(playback_muted ? "Unmute" : "  Mute")) {
            m_filter_executor.playback_mute.store(
                !playback_muted); // not atomic negation but other threads will only read
        }
        s.write(" ");
        ui_chan_sel(s, playback_r_chan);
        s.write(" Right  IN ");
        ui_chan_sel(s, playback_l_chan);
        s.write("Left  IN ");

        s.next_line();
        s.set_rtl(false);
        s.set_bold(true);
        s.write("CAPTURE  ");
        s.set_bold(false);
        ui_enum_sel(s, m_filter_executor.capture_devs(), false, m_filter_executor.capture_dev_idx);
        s.set_rtl(true);
        if (s.write_button(capture_muted ? "Unmute" : "  Mute")) {
            m_filter_executor.capture_mute.store(!capture_muted);
        }
        s.write(" ");
        ui_chan_sel(s, capture_r_chan);
        s.write(" Right OUT ");
        ui_chan_sel(s, capture_l_chan);
        s.write("Left OUT ");

        s.next_line();
        s.set_rtl(false);
        s.set_color(m_config.colors.green);
        if (s.write_button("Save")) {
            NFD::UniquePath save_path;
            const nfdfilteritem_t filters[1] = {{"Signalbox Sequence", "sbxsq"}};

            const auto result = NFD::SaveDialog(save_path, filters, 1);
            if (result == NFD_OKAY) {
                std::lock_guard lock{m_filters.mut};
                std::fstream file{save_path.get(), std::ios::out | std::ios::binary};
                enc_encode_one<uint32>(file, m_filters.filters.size());
                for (const auto& f : m_filters.filters) {
                    enc_encode_string(file, f->name());
                    f->encode(file);
                }
                file.close();
            }
        }
        s.write(" ");
        if (s.write_button("Load")) {
            NFD::UniquePath load_path;
            const nfdfilteritem_t filters[1] = {{"Signalbox Sequence", "sbxsq"}};

            const auto result = NFD::OpenDialog(load_path, filters, 1);
            if (result == NFD_OKAY) {
                std::lock_guard lock{m_filters.mut};
                std::fstream file{load_path.get(), std::ios::in | std::ios::binary};

                const auto num_filters = enc_decode_one<uint32>(file);
                m_filters.filters.clear();
                m_filters.filters.reserve(num_filters);
                m_filter_count = 0;
                for (size_t i = 0; i < num_filters; ++i) {
                    const auto name = enc_decode_string(file);
                    const auto filter = find_filter(name);
                    if (!filter)
                        break;
                    auto f = filter->fn();
                    f->decode(file);
                    add_filter(std::move(f), true);
                }

                file.close();
            }
        }

        s.end();

        m_filter_executor.playback_l_chan.store(playback_l_chan);
        m_filter_executor.playback_r_chan.store(playback_r_chan);
        m_filter_executor.capture_l_chan.store(capture_l_chan);
        m_filter_executor.capture_r_chan.store(capture_r_chan);
    }

    auto swapped = std::optional<std::array<decltype(m_filters.filters)::iterator, 2>>{};
    auto deleted = std::optional<decltype(m_filters.filters)::iterator>{};
    std::vector<std::pair<std::string, std::function<void(filter_base*)>>> added;

    auto cb_add_filter = [&](std::string_view name, std::function<void(filter_base*)>&& cb) {
        added.push_back(std::make_pair(std::string{name}, std::move(cb)));
    };

    for (auto it = m_filters.filters.begin(); it != m_filters.filters.end(); ++it) {
        auto& f = **it;
        const auto idx = std::distance(m_filters.filters.begin(), it);

        auto space = make_space(f.size());
        space.cb_add_filter = cb_add_filter;
        space.begin();

        space.set_bold(true);
        space.set_color(m_config.colors.red);
        if (space.write_button("*")) {
            deleted = {it};
        }
        space.write(" ");
        space.set_color(m_config.colors.yellow);
        if (space.write_button("<") && it != m_filters.filters.begin()) {
            swapped = {it, it - 1};
        }
        space.set_color(m_config.colors.fg);
        space.write(fmt::format("{:03}", idx));
        space.set_color(m_config.colors.yellow);
        if (space.write_button(">") && it != m_filters.filters.end() - 1) {
            swapped = {it, it + 1};
        }
        space.set_color(m_config.colors.fg);
        space.set_rtl(true);
        space.set_color(m_config.colors.filters[f.kind()]);
        space.write(f.name());
        space.set_color(m_config.colors.fg);
        space.set_bold(false);
        space.set_rtl(false);
        space.next_line();

        f.update();
        f.draw(space);
        space.end();
    }

    // practically mutually exclusive anyway but eh
    if (deleted && !swapped) {
        auto lock = std::lock_guard{m_filters.mut};
        m_filters.filters.erase(*deleted);
    }

    if (swapped) {
        auto lock = std::lock_guard{m_filters.mut};
        std::iter_swap((*swapped)[0], (*swapped)[1]);
    }

    for (const auto& [name, cb] : added) {
        const auto found = find_filter(name);
        sb_ASSERT(found);
        auto f = found->fn();
        cb(f.get());
        add_filter(std::move(f));
    }

    if (!m_did_focus && m_input.mouse_just_pressed[0]) {
        m_focus = nullptr;
    }

    nvgEndFrame(m_cx.nvg);

    m_input.keys_just_pressed.fill(false);
    m_input.mouse_just_pressed.fill(false);
    m_input.scroll_wheel = 0.f;
    m_input.text.reset();
    m_did_focus = false;

    context_end_frame(&m_cx);
}

void board::reset_layout() {
    const auto panel_width = static_cast<float32>(m_panel_width) * m_config.char_width;

    int32 width, height;
    glfwGetWindowSize(m_cx.window, &width, &height);
    m_layout_rect =
        rect2<float32>{
            {panel_width, 0.f},
            vector2<float32>{
                static_cast<float32>(width) / SB_DPI_SCALE(m_dpi_scale) - panel_width,
                static_cast<float32>(height) / SB_DPI_SCALE(m_dpi_scale)},
        }
            .inflate(-m_config.outer_padding);

    m_layout_cursor = m_layout_rect.pos;
    m_max_layout_height = 0.f;
}
