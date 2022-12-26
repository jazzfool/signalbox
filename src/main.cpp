#include "board.h"
#include "widget.h"

#include "filters/gen.h"
#include "filters/viz.h"
#include "filters/chn.h"
#include "filters/fir.h"
#include "filters/tfm.h"

#include "filters/simd.h"

#include <spdlog/fmt/fmt.h>

int main() {
    simd_vec<float32> v1{1.f, 2.f, 3.f, 4.f, 2.f};
    simd_vec<float32> v2{2.f, 3.f, 4.f, 5.f, 2.f};
    simd_vec<float32> v3{1.f, 0.f, 0.f, 1.f, 1.f};

    auto f = simd_sum(v1);

    return 0;

    board{}
        .create()
        .register_filter(fltr_gen_sine)
        .register_filter(fltr_gen_file)
        .register_filter(fltr_fir_delay)
        .register_filter(fltr_fir_gain)
        .register_filter(fltr_tfm_fft)
        .register_filter(fltr_viz_waveform)
        .register_filter(fltr_viz_vu)
        .register_filter(fltr_chn_split)
        .run_loop()
        .destroy();
}