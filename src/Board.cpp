#include "Board.h"

#include "util.h"

#include <gl/glew.h>
#include <GLFW/glfw3.h>

#include <nanovg.h>
#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

#include <spdlog/spdlog.h>

Board::Board() {
    maxLayoutHeight = 0.f;
}

Board& Board::create() {
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

    window = glfwCreateWindow(400, 300, "Signalbox", nullptr, nullptr);
    sb_ASSERT(window);

    glfwSetWindowUserPointer(window, this);

    glfwGetWindowContentScale(window, &dpiScale, nullptr);
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* window, float32 xScale, float32 yScale) {
        auto& board = *reinterpret_cast<Board*>(glfwGetWindowUserPointer(window));
        board.dpiScale = xScale;
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* window, float64 x, float64 y) {
        auto& board = *reinterpret_cast<Board*>(glfwGetWindowUserPointer(window));
        board.input.cursorPos =
            Vector2<float32>{static_cast<float32>(x), static_cast<float32>(y)} / board.dpiScale;
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int32 button, int32 action, int32 mods) {
        auto& board = *reinterpret_cast<Board*>(glfwGetWindowUserPointer(window));
        if (action == GLFW_PRESS)
            board.input.mouseJustPressed[button] = true;
    });

    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    glewInit();

    nvg = nvgCreateGL3(NVG_STENCIL_STROKES | NVG_ANTIALIAS);

    config.bg = nvgRGB(0, 0, 0);
    config.fg = nvgRGB(255, 255, 255);
    config.frameColor = nvgRGB(150, 150, 150);

    config.fontSize = 14.f;

    nvgFontSize(nvg, config.fontSize);
    nvgCreateFont(nvg, "mono", "data/RobotoMono-Regular.ttf");
    nvgFontFace(nvg, "mono");

    nvgTextMetrics(nvg, &config.ascender, nullptr, &config.lineHeight);
    config.lineHeight *= 1.2f;

    float32 bounds[4];
    config.charWidth = nvgTextBounds(nvg, 0.f, 0.f, "A", nullptr, bounds);

    config.outerPadding = 10.f;
    config.innerPadding = 5.f;

    return *this;
}

void Board::destroy() {
    nvgDeleteGL3(nvg);
    glfwDestroyWindow(window);
    glfwTerminate();
}

Board& Board::runLoop() {
    while (!glfwWindowShouldClose(window)) {
        float32 xScale, yScale;
        glfwGetWindowContentScale(window, &xScale, &yScale);

        int32 width, height;
        glfwGetWindowSize(window, &width, &height);

        int32 fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

        glViewport(0, 0, fbWidth, fbHeight);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(nvg, width, height, dpiScale);
        nvgScale(nvg, xScale, yScale);

        resetLayout();

        auto space = makeSpace({50, 5});
        space.begin();
        space.write("Click this button: ");
        if (space.writeButton("This one"))
            spdlog::info("click!");
        space.nextLine();
        space.writeButton("...or this one");
        space.end();

        auto s2 = makeSpace({50, 5});
        s2.begin();
        s2.end();

        auto s3 = makeSpace({50, 5});
        s3.begin();
        s3.end();

        nvgEndFrame(nvg);

        input.mouseJustPressed = {false, false, false};

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    return *this;
}

const BoardConfig& Board::getConfig() const {
    return config;
}

Space Board::makeSpace(const Vector2<uint32>& textSize) {
    const auto size = Vector2<float32>{
        static_cast<float32>(textSize.x) * config.charWidth,
        static_cast<float32>(textSize.y) * config.lineHeight,
    };

    if (size.x > layoutRect.size.x - layoutCursor.x) {
        layoutCursor.x = layoutRect.pos.x;
        layoutCursor.y += maxLayoutHeight + config.outerPadding;
        maxLayoutHeight = 0.f;
    }

    maxLayoutHeight = std::max(maxLayoutHeight, size.y);
    const auto spaceRect = Rect<float32>{layoutCursor, size};
    layoutCursor.x += size.x + config.outerPadding;

    Space space;

    space.window = window;
    space.nvg = nvg;
    space.input = input;

    space.config = getConfig();
    space.rect = spaceRect;

    return space;
}

void Board::resetLayout() {
    int32 width, height;
    glfwGetWindowSize(window, &width, &height);
    layoutRect =
        Rect<float32>{
            {0.f, 0.f},
            Vector2<float32>{static_cast<float32>(width), static_cast<float32>(height)} / dpiScale,
        }
            .inflate(-config.outerPadding);

    layoutCursor = layoutRect.pos;
    maxLayoutHeight = 0.f;
}
