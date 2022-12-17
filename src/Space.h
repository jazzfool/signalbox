#pragma once

#include "util.h"
#include "State.h"

#include <string>

struct GLFWwindow;
struct NVGcontext;

struct Space final {
    GLFWwindow* window;
    NVGcontext* nvg;
    InputState input;

    Vector2<float32> layoutCursor;
    Rect<float32> rect;
    NVGcolor color;
    BoardConfig config;

    void begin();
    void end();

    void write(const std::string& s);
    bool writeButton(const std::string& s);
    void nextLine();
    Rect<float32> drawBlock(uint32 lines);

    float32 remainingHeight() const;
};
