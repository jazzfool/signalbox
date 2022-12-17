#pragma once

#include "util.h"

#include <nanovg.h>
#include <array>

struct BoardConfig final {
    NVGcolor bg;
    NVGcolor fg;
    NVGcolor frameColor;

    float32 fontSize;
    float32 lineHeight;
    float32 charWidth;
    float32 ascender;

    float32 outerPadding;
    float32 innerPadding;
};

struct InputState final {
    Vector2<float32> cursorPos = {0.f, 0.f};
    std::array<bool, 3> mouseJustPressed = {false, false, false};
};
