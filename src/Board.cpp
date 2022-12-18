#include "Board.h"

#include "util.h"

#include <gl/glew.h>
#include <GLFW/glfw3.h>

#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <spdlog/spdlog.h>

board::board() {
    m_max_layout_height = 0.f;
}

board& board::create() {
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GL_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 0);
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

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* m_window, float64 x, float64 y) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(m_window));
        b.m_input.cursor_pos =
            vector2<float32>{static_cast<float32>(x), static_cast<float32>(y)} / b.m_dpi_scale;
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* m_window, int32 button, int32 action, int32 mods) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(m_window));
        if (action == GLFW_PRESS)
            b.m_input.mouse_just_pressed[button] = true;
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* m_window, float64 xScroll, float64 yScroll) {
        auto& b = *reinterpret_cast<board*>(glfwGetWindowUserPointer(m_window));
        b.m_input.scroll_wheel = static_cast<float32>(yScroll);
    });

    glfwMakeContextCurrent(m_window);

    glewExperimental = GL_TRUE;
    glewInit();

    m_nvg = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);

    m_config.font_size = 14.f;
    nvgFontSize(m_nvg, m_config.font_size);

    nvgCreateFont(m_nvg, "mono", "data/CourierPrime-Regular.ttf");
    nvgCreateFont(m_nvg, "monoB", "data/CourierPrime-Bold.ttf");

    nvgFontFace(m_nvg, "mono");

    nvgTextMetrics(m_nvg, &m_config.ascender, nullptr, &m_config.line_height);
    m_config.line_height *= 1.2f;

    float32 bounds[4];
    m_config.char_width = nvgTextBounds(m_nvg, 0.f, 0.f, "A", nullptr, bounds);

    m_config.outer_padding = 10.f;
    m_config.inner_padding = 5.f;

    return *this;
}

void board::destroy() {
    nvgDeleteGL3(m_nvg);
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

board& board::run_loop() {
    while (!glfwWindowShouldClose(m_window)) {
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

        reset_layout();

        for (auto& f : m_filters) {
            auto space = make_space(f->space_size());
            space.begin();
            f->draw_space(space);
            space.end();
        }

        nvgEndFrame(m_nvg);

        m_input.mouse_just_pressed = {false, false, false};
        m_input.scroll_wheel = 0.f;

        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }

    return *this;
}

const board_config& board::config() const {
    return m_config;
}

space board::make_space(const vector2<uint32>& textSize) {
    const auto size = vector2<float32>{
        static_cast<float32>(textSize.x) * m_config.char_width + 2.f * m_config.inner_padding,
        static_cast<float32>(textSize.y) * m_config.line_height + 2.f * m_config.inner_padding,
    };

    if (size.x > m_layout_rect.size.x - m_layout_cursor.x) {
        m_layout_cursor.x = m_layout_rect.pos.x;
        m_layout_cursor.y += m_max_layout_height + m_config.outer_padding;
        m_max_layout_height = 0.f;
    }

    m_max_layout_height = std::max(m_max_layout_height, size.y);
    const auto space_rect = rect2<float32>{m_layout_cursor, size};
    m_layout_cursor.x += size.x + m_config.outer_padding;

    return space{m_window, m_nvg, m_input, m_config, space_rect};
}

void board::reset_layout() {
    int32 width, height;
    glfwGetWindowSize(m_window, &width, &height);
    m_layout_rect =
        rect2<float32>{
            {0.f, 0.f},
            vector2<float32>{static_cast<float32>(width), static_cast<float32>(height)} / m_dpi_scale,
        }
            .inflate(-m_config.outer_padding);

    m_layout_cursor = m_layout_rect.pos;
    m_max_layout_height = 0.f;
}
