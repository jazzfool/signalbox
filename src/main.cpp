#include "board.h"
#include "widget.h"

#include "filters/gen.h"
#include "filters/viz.h"
#include "filters/chn.h"
#include "filters/fir.h"

#include <spdlog/fmt/fmt.h>

int main() {
    board{}
        .create()
        .register_filter(fltr_gen_sine)
        .register_filter(fltr_gen_file)
        .register_filter(fltr_fir_delay)
        .register_filter(fltr_fir_gain)
        .register_filter(fltr_viz_waveform)
        .register_filter(fltr_viz_vu)
        .register_filter(fltr_chn_split)
        .run_loop()
        .destroy();
}