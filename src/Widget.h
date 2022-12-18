#pragma once

#include "util.h"

#include <vector>

struct space;
struct NVGcolor;

void ui_chan_sel(space& space, uint8& chan);
void ui_float_ran(space& space, float32 min, float32 max, float32 step, float32& x);
void ui_viz_sine(space& space, NVGcolor stroke, uint32 lines, uint32 samples, float32 ampl, float32 freq);
