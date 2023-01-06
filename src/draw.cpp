#include "draw.h"

#include <nanovg.h>

NVGcolor blend_color(NVGcolor a, NVGcolor b, float32 t) {
    return nvgRGBAf(lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t));
}

void draw_fill_rrect(NVGcontext* nvg, const rect2<float32>& rect, float32 radius, NVGcolor color) {
    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, NVG_RECT_ARGS(rect), radius);
    nvgFillColor(nvg, color);
    nvgFill(nvg);
}

void draw_stroke_rrect(
    NVGcontext* nvg, const rect2<float32>& rect, float32 radius, NVGcolor color, float32 stroke_width) {
    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, NVG_RECT_ARGS(rect), radius);
    nvgStrokeColor(nvg, color);
    nvgStrokeWidth(nvg, stroke_width);
    nvgStroke(nvg);
}
