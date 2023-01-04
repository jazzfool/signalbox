#pragma once
#pragma once

#include "util.h"
#include "filters/sample.h"
#include "filters/simd.h"

#include <vector>
#include <string>
#include <spdlog/fmt/fmt.h>
#include <typeindex>
#include <any>
#include <type_traits>
#include <utility>

class space;
struct NVGcolor;
struct tracker_note;

void ui_chan_sel(space& space, uint8& chan);
void ui_enum_sel(space& space, std::span<const std::string> options, bool wrap, uint32& i);
void ui_float_ran(space& space, float32 min, float32 max, float32 step, float32& x);
void ui_float(space& space, float32 step, float32& x);
void ui_int_ran(space& space, int32 min, int32 max, int32 step, uint8 pad, int32& x);
void ui_int(space& space, int32 step, uint8 pad, int32& x);
void ui_tracker_note(space& space, tracker_note& note);
void ui_text_in(space& space, NVGcolor color, std::string& s, uint32 len);
void ui_viz_sine(space& space, NVGcolor stroke, uint32 lines, uint32 samples, float32 ampl, float32 freq);
void ui_viz_wf(
    space& space, NVGcolor stroke, uint32 lines, float32 scale, float32 offset,
    simd_slice<const sample> samples);
void ui_viz_sample(
    space& space, NVGcolor stroke, uint32 lines, uint64 length, uint64 scale, uint64 first, uint64 last,
    simd_slice<const sample> samples, size_t active_splice, std::span<int32> splices);
void ui_viz_meter(space& space, float32 level);
