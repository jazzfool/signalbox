#include "app.h"

#include "ui.h"

#include <nfd.hpp>
#include <GLFW/glfw3.h>

#ifdef __APPLE__
#define SB_DPI_SCALE(X) 1.f
#else
#define SB_DPI_SCALE(X) X
#endif

App::App()  {
    m_dpi_scale = 1.f;
}

App& App::create() {
    m_cx = create_context();

    glfwSetWindowUserPointer(m_cx.window, this);

    glfwGetWindowContentScale(m_cx.window, &m_dpi_scale, nullptr);
    glfwSetWindowContentScaleCallback(m_cx.window, [](GLFWwindow* window, float32 xScale, float32 yScale) {
        auto& b = *reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
        b.m_dpi_scale = xScale;
    });

    glfwSetCursorPosCallback(m_cx.window, [](GLFWwindow* window, float64 x, float64 y) {
        auto& state = UI_State::get();
        auto& b = *reinterpret_cast<App*>(glfwGetWindowUserPointer(window));
        const auto pos = Vector2<float32>{
            static_cast<float32>(x) / SB_DPI_SCALE(b.m_dpi_scale),
            static_cast<float32>(y) / SB_DPI_SCALE(b.m_dpi_scale)};
        state.input.cursor_delta = pos - state.input.cursor_pos;
        state.input.cursor_pos = pos;
    });

    glfwSetMouseButtonCallback(m_cx.window, [](GLFWwindow* window, int32 button, int32 action, int32 mods) {
        auto& state = UI_State::get();
        if (action == GLFW_PRESS) {
            state.input.mouse_just_pressed[button] = true;
            state.input.mouse_is_pressed[button] = true;
        } else if (action == GLFW_RELEASE) {
            state.input.mouse_just_released[button] = true;
            state.input.mouse_is_pressed[button] = false;
        }
    });

    glfwSetScrollCallback(m_cx.window, [](GLFWwindow* window, float64 xScroll, float64 yScroll) {
        auto& state = UI_State::get();
        state.input.scroll_wheel = static_cast<float32>(yScroll);
    });

    glfwSetCharCallback(m_cx.window, [](GLFWwindow* window, uint32 text) {
        auto& state = UI_State::get();
        state.input.text = static_cast<char32>(text);
    });

    glfwSetKeyCallback(
        m_cx.window, [](GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods) {
            auto& state = UI_State::get();
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                state.input.keys_just_pressed[key] = true;
                state.input.key_is_pressed[key] = true;
            } else if (action == GLFW_RELEASE) {
                state.input.key_is_pressed[key] = false;
            }
        });

    glfwSetFramebufferSizeCallback(m_cx.window, [](GLFWwindow* window, int32, int32) {
        auto p = glfwGetWindowUserPointer(window);
        if (!p)
            return;
        auto& b = *reinterpret_cast<App*>(p);
        context_on_resize(&b.m_cx);
        b.draw_frame();
    });

    NFD::Init();

    nvgCreateFont(m_cx.nvg, "mono", "data/iosevka-fixed-slab-extended.ttf");
    nvgCreateFont(m_cx.nvg, "monoB", "data/iosevka-fixed-slab-extendedbold.ttf");

    nvgCreateFont(m_cx.nvg, "sans", "data/NotoSans-Regular.ttf");
    nvgCreateFont(m_cx.nvg, "sansB", "data/NotoSans-Bold.ttf");
    nvgCreateFont(m_cx.nvg, "sansI", "data/NotoSans-Italic.ttf");
    nvgCreateFont(m_cx.nvg, "sansBI", "data/NotoSans-BoldItalic.ttf");

    nvgCreateFont(m_cx.nvg, "symbols", "data/NotoSansSymbols-Regular.ttf");
    nvgCreateFont(m_cx.nvg, "symbols2", "data/NotoSansSymbols2-Regular.ttf");
    nvgCreateFont(m_cx.nvg, "emoji", "data/NotoEmoji-Regular.ttf");

    nvgAddFallbackFont(m_cx.nvg, "sans", "symbols");
    nvgAddFallbackFont(m_cx.nvg, "sans", "symbols2");
    nvgAddFallbackFont(m_cx.nvg, "sans", "emoji");

    m_draw.emplace(m_cx.nvg);

    m_tracker.create();

    return *this;
}

void App::destroy() {
    m_tracker.destroy();
    destroy_context(&m_cx);
}

App& App::run_loop() {
    while (!glfwWindowShouldClose(m_cx.window)) {
        draw_frame();
        glfwWaitEvents();
    }

    return *this;
}

void App::ui() {
    m_tracker.ui();
}

void App::draw_frame() {
    auto& state = UI_State::get();

    int32 width, height;
    glfwGetWindowSize(m_cx.window, &width, &height);

    int32 fb_width, fb_height;
    glfwGetFramebufferSize(m_cx.window, &fb_width, &fb_height);

    const auto clear_color = state.colors.window_bg;
    context_begin_frame(&m_cx, clear_color.r, clear_color.g, clear_color.b);

    nvgBeginFrame(m_cx.nvg, width, height, m_dpi_scale);
    nvgScale(m_cx.nvg, SB_DPI_SCALE(m_dpi_scale), SB_DPI_SCALE(m_dpi_scale));

    m_draw->reset(width, height);

    state.begin_frame(*m_draw);

    ui();

    state.end_frame();
    m_draw->execute();

    nvgEndFrame(m_cx.nvg);
    context_end_frame(&m_cx);
}
