#include "Board.h"
#include "Widget.h"

#include <spdlog/fmt/fmt.h>

int main() {
    struct filter_data0 {
        uint8 chan1;
    };

    struct filter_data1 {
        uint8 chan1, chan2;
        float32 freq;
    };

    board()
        .create()
        .filter(filter<filter_data0, stereo>{
            .data = {0},
            .size =
                [](auto&) {
                    return vector2<uint32>{30, 3};
                },
            .draw =
                [](auto& data, auto& s) {
                    s.set_bold(true);
                    s.write("000");
                    s.set_rtl(true);
                    s.write("\"WAV SOURCE\"");
                    s.set_bold(false);
                    s.set_rtl(false);

                    s.next_line();
                    s.write("File: ");
                    s.write("test.wav");

                    s.next_line();
                    s.set_rtl(true);
                    ui_chan_sel(s, data.chan1);
                    s.write("Channel A OUT: ");
                },
            .filter = [](auto&, auto x) -> stereo { return {.l = x.l, .r = x.r}; },
        })
        .filter(filter<filter_data1, stereo>{
            .data = {0, 0, 1.f},
            .size =
                [](auto&) {
                    return vector2<uint32>{30, 7};
                },
            .draw =
                [](auto& data, auto& s) {
                    s.set_bold(true);
                    s.write("001");
                    s.set_rtl(true);
                    s.write("\"NOTCH FILTER\"");
                    s.set_bold(false);
                    s.set_rtl(false);

                    s.next_line();
                    s.write("Channel A IN: ");
                    ui_chan_sel(s, data.chan1);

                    s.next_line();
                    s.set_rtl(true);
                    ui_chan_sel(s, data.chan2);
                    s.write("Channel A OUT: ");

                    s.set_rtl(false);
                    s.next_line();
                    s.write("Frequency: ");
                    ui_float_ran(s, -2.f, 2.f, 0.1f, data.freq);

                    ui_viz_sine(s, nvgRGB(255, 0, 0), 3, 100, 1.f, data.freq);
                },
            .filter = [](auto&, auto x) -> stereo { return {.l = x.l, .r = x.r}; },
        })
        .run_loop()
        .destroy();
}