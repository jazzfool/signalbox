#pragma once

#include "util.h"
#include "filters/sample.h"
#include "filters/simd.h"

#include <vector>
#include <string>

struct space;
struct NVGcolor;

void ui_chan_sel(space& space, uint8& chan);
void ui_enum_sel(space& space, std::span<const std::string> options, uint32& i);
void ui_float_ran(space& space, float32 min, float32 max, float32 step, float32& x);
void ui_float(space& space, float32 step, float32& x); 
void ui_int_ran(space& space, int32 min, int32 max, int32 step, uint8 pad, int32& x);
void ui_int(space& space, int32 step, uint8 pad, int32& x);
void ui_text_in(space& space, NVGcolor color, std::string& s, uint32 len);
void ui_viz_sine(space& space, NVGcolor stroke, uint32 lines, uint32 samples, float32 ampl, float32 freq);
void ui_viz_wf(space& space, NVGcolor stroke, uint32 lines, float32 scale, float32 offset, simd_slice<const sample> samples);
void ui_viz_meter(space& space, float32 level);
