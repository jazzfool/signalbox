#include "board.h"

#include "util.h"
#include "widget.h"

#include <gl/glew.h>
#include <GLFW/glfw3.h>

#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <spdlog/spdlog.h>

board::board() : m_filter_executor{m_filters} {
    m_focus = nullptr;
    m_frame0 = true;
    m_max_layout_height = 0.f;
    m_executor_status = "INACTIVE";
    m_panel_width = 20;
}

board& board::create() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);

    m_window = glfwCreateWindow(400, 300, "Signalbox", nullptr, nullptr);
    sb_ASSERT(m_window);

    glfwSetWindowUserPointer(m_window, this);

    glfwGetWindowContentScale(m_window, &m_dpi_scale, nullptr);
    glfwSetWindowContentScaleCallback(m_window, [](GLFWwindow* m_window, float32 xScale, float32 yScale) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(m_window));
        b.m_dpi_scale = xScale;
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, float64 x, float64 y) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_input.cursor_pos =
            vector2<float32>{static_cast<float32>(x), static_cast<float32>(y)} / b.m_dpi_scale;
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int32 button, int32 action, int32 mods) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS)
            b.m_input.mouse_just_pressed[button] = true;
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* window, float64 xScroll, float64 yScroll) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_input.scroll_wheel = static_cast<float32>(yScroll);
    });

    glfwSetCharCallback(m_window, [](GLFWwindow* window, uint32 text) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        b.m_input.text = static_cast<char32>(text);
    });

    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            b.m_input.keys_just_pressed[key] = true;
        }
    });

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int32, int32) {
        auto p = glfwGetWindowUserPointer(window);
        if (!p)
            return;
        auto& b = *reinterpret_cast<board*>(p);
        b.draw_frame();
    });

    glfwMakeContextCurrent(m_window);

    glewExperimental = GL_TRUE;
    glewInit();

    m_nvg = nvgCreateGL3(NVG_STENCIL_STROKES);

    m_config.font_size = 14.f;

    nvgCreateFont(m_nvg, "mono", "data/CourierPrime-Regular.ttf");
    nvgCreateFont(m_nvg, "monoB", "data/CourierPrime-Bold.ttf");

    nvgFontFace(m_nvg, "mono");
    nvgFontSize(m_nvg, m_config.font_size);
    nvgTextAlign(m_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgTextMetrics(m_nvg, &m_config.ascender, nullptr, &m_config.line_height);
    m_config.line_height *= 1.2f;

    m_config.outer_padding = 10.f;
    m_config.inner_padding = 5.f;

    m_filter_executor.create();

    return *this;
}

void board::destroy() {
    m_filter_executor.destroy();

    nvgDeleteGL3(m_nvg);
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

board& board::run_loop() {
    while (!glfwWindowShouldClose(m_window)) {
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
    info.kind = dummy->kind();

    const auto fd_in = dummy->data_chans_in();
    const auto fd_out = dummy->data_chans_out();

    info.in =
        fd_in ? fd_in->chans_in_is_dynamic() ? "**" : fmt::format("{:02}", fd_in->chans_in().size()) : "..";
    info.out = fd_out
                   ? fd_out->chans_out_is_dynamic() ? "**" : fmt::format("{:02}", fd_out->chans_out().size())
                   : "..";

    m_all_filters.push_back(std::move(info));

    return *this;
}

void board::add_filter(std::unique_ptr<filter_base>&& filter) {
    auto lock = std::lock_guard{m_filters.mut};

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

    m_filters.filters.push_back(std::move(filter));
}

space board::new_spacef(const rect2<float32>& rect) {
    return space{m_window, m_nvg, m_focus, m_input, m_config, rect};
}

void board::draw_frame() {
    float32 x_scale, y_scale;
    glfwGetWindowContentScale(m_window, &x_scale, &y_scale);

    int32 width, height;
    glfwGetWindowSize(m_window, &width, &height);

    int32 fb_width, fb_height;
    glfwGetFramebufferSize(m_window, &fb_width, &fb_height);

    glViewport(0, 0, fb_width, fb_height);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(m_nvg, width, height, m_dpi_scale);
    nvgScale(m_nvg, x_scale, y_scale);

    if (m_frame0) {
        m_frame0 = false;
        nvgFontFace(m_nvg, "mono");
        nvgFontSize(m_nvg, m_config.font_size);
        nvgTextAlign(m_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        m_config.char_width = nvgTextBounds(m_nvg, 0.f, 0.f, " ", nullptr, nullptr);
    }

    reset_layout();

    {
        auto pushed = std::optional<decltype(m_all_filters)::iterator>{};

        auto s = new_spacef(rect2<float32>{
            {0.f, 0.f},
            {static_cast<float32>(m_panel_width) * m_config.char_width + m_config.outer_padding,
             m_layout_rect.size.y + 2.f * m_config.outer_padding}}
                                .inflate(-m_config.outer_padding));

        s.begin();
        s.set_bold(true);
        s.write("FILTERS");
        s.next_line();
        s.set_bold(false);
        for (auto it = m_all_filters.begin(); it != m_all_filters.end(); ++it) {
            s.set_rtl(false);
            s.set_color(m_config.colors.filters[it->kind]);
            if (s.write_button(it->name)) {
                pushed = it;
            }
            s.set_rtl(true);
            s.set_color(m_config.colors.semifaint);
            s.write(fmt::format("{}->{}", it->in, it->out));
            s.set_color(m_config.colors.fg);
            s.next_line();
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
            make_spacef({m_layout_rect.size.x, m_config.line_height * 3.f + 2.f * m_config.inner_padding});
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
        ui_enum_sel(s, m_filter_executor.playback_devs(), m_filter_executor.playback_dev_idx);
        s.set_rtl(true);
        if (s.write_button(playback_muted ? "Unmute" : "Mute")) {
            m_filter_executor.playback_mute.store(
                !playback_muted); // not atomic negation but other threads will only read
        }
        s.write(" ");
        ui_chan_sel(s, playback_r_chan);
        s.write(" Right IN ");
        ui_chan_sel(s, playback_l_chan);
        s.write("Left IN ");

        s.next_line();
        s.set_rtl(false);
        s.set_bold(true);
        s.write("CAPTURE ");
        s.set_bold(false);
        ui_enum_sel(s, m_filter_executor.capture_devs(), m_filter_executor.capture_dev_idx);
        s.set_rtl(true);
        if (s.write_button(capture_muted ? "Unmute" : "Mute")) {
            m_filter_executor.capture_mute.store(!capture_muted);
        }
        s.write(" ");
        ui_chan_sel(s, capture_r_chan);
        s.write(" Right OUT ");
        ui_chan_sel(s, capture_l_chan);
        s.write("Left OUT ");

        s.end();

        m_filter_executor.playback_l_chan.store(playback_l_chan);
        m_filter_executor.playback_r_chan.store(playback_r_chan);
        m_filter_executor.capture_l_chan.store(capture_l_chan);
        m_filter_executor.capture_r_chan.store(capture_r_chan);
    }

    auto swapped = std::optional<std::array<decltype(m_filters.filters)::iterator, 2>>{};
    auto deleted = std::optional<decltype(m_filters.filters)::iterator>{};

    for (auto it = m_filters.filters.begin(); it != m_filters.filters.end(); ++it) {
        auto& f = **it;
        const auto idx = std::distance(m_filters.filters.begin(), it);

        auto space = make_space(f.size());
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
        m_filters.filters.erase(*deleted);
    }

    if (swapped) {
        auto lock = std::lock_guard{m_filters.mut};
        std::iter_swap((*swapped)[0], (*swapped)[1]);
    }

    nvgEndFrame(m_nvg);

    m_input.keys_just_pressed.fill(false);
    m_input.mouse_just_pressed = {false, false, false};
    m_input.scroll_wheel = 0.f;
    m_input.text.reset();

    glfwSwapBuffers(m_window);
}

void board::reset_layout() {
    const auto panel_width = static_cast<float32>(m_panel_width) * m_config.char_width;

    int32 width, height;
    glfwGetWindowSize(m_window, &width, &height);
    m_layout_rect =
        rect2<float32>{
            {panel_width, 0.f},
            vector2<float32>{
                static_cast<float32>(width) - panel_width * m_dpi_scale, static_cast<float32>(height)} /
                m_dpi_scale,
        }
            .inflate(-m_config.outer_padding);

    m_layout_cursor = m_layout_rect.pos;
    m_max_layout_height = 0.f;
}
