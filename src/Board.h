#pragma once

#include "util.h"
#include "State.h"
#include "Space.h"

#include <vector>
#include <array>

struct GLFWwindow;

class Board final {
  public:
    Board();
    Board& create();
    void destroy();

    Board& runLoop();

    Space makeSpace(const Vector2<uint32>& textSize);

    const BoardConfig& getConfig() const;

  private:
    void resetLayout();

    BoardConfig config;
    GLFWwindow* window;
    NVGcontext* nvg;

    float32 dpiScale;
    Rect<float32> layoutRect;
    Vector2<float32> layoutCursor;
    float32 maxLayoutHeight;

    InputState input;
};
