#pragma once

#include "util.h"

struct NVGcontext;
struct NVGcolor;

NVGcolor blend_color(NVGcolor a, NVGcolor b, float32 t);

void draw_fill_rrect(NVGcontext* nvg, const rect2<float32>& rect, float32 radius, NVGcolor color);
void draw_stroke_rrect(NVGcontext* nvg, const rect2<float32>& rect, float32 radius, NVGcolor color, float32 stroke_width);
