#include "tracker.h"

#include <nfd.hpp>

#ifdef __APPLE__
#define SB_DPI_SCALE(X) 1.f
#else
#define SB_DPI_SCALE(X) X
#endif

tracker::tracker() : m_focus{ui_key::null()} {
    m_dpi_scale = 1.f;
    m_ui_opts = ui_options::default_options();
    m_ui_colors = ui_colors::dark();
}

tracker& tracker::create() {
    m_cx = create_context();

    glfwSetWindowUserPointer(m_cx.window, this);

    glfwGetWindowContentScale(m_cx.window, &m_dpi_scale, nullptr);
    glfwSetWindowContentScaleCallback(m_cx.window, [](GLFWwindow* window, float32 xScale, float32 yScale) {
        auto& b = *reinterpret_cast<tracker*>(glfwGetWindowUserPointer(window));
        b.m_dpi_scale = xScale;
    });

    glfwSetCursorPosCallback(m_cx.window, [](GLFWwindow* window, float64 x, float64 y) {
        auto& b = *reinterpret_cast<tracker*>(glfwGetWindowUserPointer(window));
        b.m_input.cursor_pos = vector2<float32>{
            static_cast<float32>(x) / SB_DPI_SCALE(b.m_dpi_scale),
            static_cast<float32>(y) / SB_DPI_SCALE(b.m_dpi_scale)};
    });

    glfwSetMouseButtonCallback(m_cx.window, [](GLFWwindow* window, int32 button, int32 action, int32 mods) {
        auto& b = *reinterpret_cast<tracker*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS) {
            b.m_input.mouse_just_pressed[button] = true;
            b.m_input.mouse_is_pressed[button] = true;
        } else if (action == GLFW_RELEASE) {
            b.m_input.mouse_just_released[button] = true;
            b.m_input.mouse_is_pressed[button] = false;
        }
    });

    glfwSetScrollCallback(m_cx.window, [](GLFWwindow* window, float64 xScroll, float64 yScroll) {
        auto& b = *reinterpret_cast<tracker*>(glfwGetWindowUserPointer(window));
        b.m_input.scroll_wheel = static_cast<float32>(yScroll);
    });

    glfwSetCharCallback(m_cx.window, [](GLFWwindow* window, uint32 text) {
        auto& b = *reinterpret_cast<tracker*>(glfwGetWindowUserPointer(window));
        b.m_input.text = static_cast<char32>(text);
    });

    glfwSetKeyCallback(
        m_cx.window, [](GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods) {
            auto& b = *reinterpret_cast<tracker*>(glfwGetWindowUserPointer(window));
            if (action == GLFW_PRESS || action == GLFW_REPEAT) {
                b.m_input.keys_just_pressed[key] = true;
            }
        });

    glfwSetFramebufferSizeCallback(m_cx.window, [](GLFWwindow* window, int32, int32) {
        auto p = glfwGetWindowUserPointer(window);
        if (!p)
            return;
        auto& b = *reinterpret_cast<tracker*>(p);
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

    return *this;
}

void tracker::destroy() {
    destroy_context(&m_cx);
}

tracker& tracker::run_loop() {
    while (!glfwWindowShouldClose(m_cx.window)) {
        draw_frame();
        glfwPollEvents();
    }

    return *this;
}

void tracker::ui() {
    ui_vstack vs;
    vs.spacing = 5.f;
    ui_with_layout(vs, [&] {
        ui_text_button({}, "Hello, world!");

        static uint32 opt = 0;

        static const std::vector<std::string> options = {"Really really Long option", "Short Option", "Bruh"};

        ui_dropdown_box_opts dropdown;
        dropdown.index = &opt;
        dropdown.options = options;
        ui_dropdown_box(dropdown);
    });
}

void tracker::draw_frame() {
    int32 width, height;
    glfwGetWindowSize(m_cx.window, &width, &height);

    int32 fb_width, fb_height;
    glfwGetFramebufferSize(m_cx.window, &fb_width, &fb_height);

    const auto clear_color = m_ui_colors.window_bg;
    context_begin_frame(&m_cx, clear_color.r, clear_color.g, clear_color.b);

    nvgBeginFrame(m_cx.nvg, width, height, m_dpi_scale);
    nvgScale(m_cx.nvg, SB_DPI_SCALE(m_dpi_scale), SB_DPI_SCALE(m_dpi_scale));

    m_input.begin_frame();
    m_memory.begin_frame();

    draw_list draw{m_cx.nvg};

    auto& state = ui_state::get();
    state.draw = &draw;
    state.input = &m_input;
    state.memory = &m_memory;
    state.focus = &m_focus;
    state.opts = &m_ui_opts;
    state.colors = &m_ui_colors;
    state.focus_taken = false;

    ui_padding padding;
    padding.rect = {{0.f, 0.f}, {(float32)width, (float32)height}};
    padding.left = 10.f;
    padding.right = 10.f;
    padding.top = 10.f;
    padding.bottom = 10.f;

    ui_with_no_alloc_layout(padding, [&] { ui(); });

    if (!ui_state::get().focus_taken && m_input.mouse_just_pressed[0]) {
        m_focus = ui_key::null();
    }

    draw.execute();

    m_input.end_frame();

    nvgEndFrame(m_cx.nvg);
    context_end_frame(&m_cx);
}
