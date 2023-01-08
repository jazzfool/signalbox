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
struct tracker_line;

struct ui_scroll_state final {
    bool scrollable = true;
    uint32 scroll = 0;
    uint32 lines = 0;
    uint32 inner_lines = 0;
};

void ui_chan_sel(space& space, uint8& chan);
void ui_enum_sel(space& space, std::span<const std::string> options, bool wrap, uint32& i);
void ui_float_ran(space& space, float32 min, float32 max, float32 step, float32& x);
void ui_float(space& space, float32 step, float32& x);
void ui_int_ran(space& space, int32 min, int32 max, int32 step, uint8 pad, int32& x);
void ui_int(space& space, int32 step, uint8 pad, int32& x);
void ui_tracker_line(space& space, tracker_line& line, uint8 num_notes, uint8 num_fx);
void ui_text_in(space& space, NVGcolor color, std::string& s, const std::string& placeholder, uint32 len);
bool ui_filepath_in(space& space, NVGcolor color, std::string& s, const std::string& placeholder, uint32 len, bool dir_mode = false);
void ui_viz_sine(space& space, NVGcolor stroke, uint32 lines, uint32 samples, float32 ampl, float32 freq);
void ui_viz_wf(
    space& space, NVGcolor stroke, uint32 lines, float32 scale, float32 offset,
    simd_slice<const sample> samples);
void ui_viz_sample(
    space& space, NVGcolor stroke, uint32 lines, uint64 length, uint64 scale, uint64 first, uint64 last,
    simd_slice<const sample> samples, size_t active_splice, std::span<int32> splices);
void ui_viz_meter(space& space, float32 level);

space ui_begin_vscroll(space& space, ui_scroll_state& state);
void ui_end_vscroll(space& space, ui_scroll_state& state);