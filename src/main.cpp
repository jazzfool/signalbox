#include "board.h"
#include "widget.h"

#include "filters/gen.h"
#include "filters/viz.h"
#include "filters/chn.h"
#include "filters/sig.h"

#include <spdlog/fmt/fmt.h>

int main() {
    board{}
        .create()
        .add_filter(fltr_gen_sine())
        .add_filter(fltr_sig_delay())
        .add_filter(fltr_sig_gain())
        .add_filter(fltr_viz_waveform())
        .add_filter(fltr_viz_vu())
        .add_filter(fltr_chn_split())
        .run_loop()
        .destroy();
}