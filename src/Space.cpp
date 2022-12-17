#include "Space.h"

#include <nanovg.h>
#include <GLFW/glfw3.h>
#include <glm/mat3x3.hpp>
#include <glm/gtc/type_ptr.hpp>

void Space::begin() {
    layoutCursor = rect.pos + Vector2<float32>{config.innerPadding};

    color = config.fg;

    nvgSave(nvg);
    nvgIntersectScissor(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y);
    nvgSave(nvg);
}

void Space::end() {
    nvgRestore(nvg);
    nvgRestore(nvg);

    nvgSave(nvg);
    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, rect.pos.x, rect.pos.y, rect.size.x, rect.size.y, 4.f);
    nvgStrokeColor(nvg, nvgRGBA(255, 255, 255, 100));
    nvgStrokeWidth(nvg, 1.5f);
    nvgStroke(nvg);
    nvgRestore(nvg);
}

void Space::write(const std::string& s) {
    if (s.empty())
        return;

    const float32 y = layoutCursor.y + config.lineHeight / 2.f;

    nvgFontFace(nvg, "mono");
    nvgFontSize(nvg, config.fontSize);
    nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgFillColor(nvg, color);
    nvgText(nvg, layoutCursor.x, y, s.c_str(), nullptr);

    const float32 advance =
        nvgTextBounds(nvg, layoutCursor.x, y, s.c_str(), nullptr, nullptr);

    layoutCursor.x += advance;
}

bool Space::writeButton(const std::string& s) {
    if (s.empty())
        return false;

    const float32 y = layoutCursor.y + config.lineHeight / 2.f;

    nvgFontFace(nvg, "mono");
    nvgFontSize(nvg, config.fontSize);
    nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    float32 bounds[4];
    const float32 advance =
        nvgTextBounds(nvg, layoutCursor.x, y, s.c_str(), nullptr, bounds);

    const auto boundsRect = Rect<float32>{
        {bounds[0], bounds[1]}, {bounds[2] - bounds[0], bounds[3] - bounds[1]}};

    const auto mouseOver = boundsRect.contains(input.cursorPos);
    if (mouseOver) {
        nvgBeginPath(nvg);
        nvgRect(
            nvg, boundsRect.pos.x, layoutCursor.y, boundsRect.size.x, config.lineHeight);
        nvgFillColor(nvg, color);
        nvgFill(nvg);
    }

    nvgFillColor(nvg, mouseOver ? config.bg : color);
    nvgText(nvg, layoutCursor.x, y, s.c_str(), nullptr);

    layoutCursor.x += advance;

    return mouseOver && input.mouseJustPressed[0];
}

void Space::nextLine() {
    layoutCursor.x = rect.pos.x + config.innerPadding;
    layoutCursor.y += config.lineHeight;
}

Rect<float32> Space::drawBlock(uint32 lines) {
    if (!floatEq(layoutCursor.x, 0.f))
        nextLine();

    const auto height =
        std::min(remainingHeight(), config.lineHeight * static_cast<float32>(lines));
    const auto outRect = Rect<float32>{layoutCursor, {rect.size.x, height}};

    layoutCursor.x = rect.pos.x;
    layoutCursor.y += height;

    return outRect;
}

float32 Space::remainingHeight() const {
    return rect.size.y - layoutCursor.y;
}
