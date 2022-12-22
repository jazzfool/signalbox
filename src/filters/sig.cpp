#include "sig.h"

#include "space.h"
#include "widget.h"

std::unique_ptr<filter_base> fltr_sig_delay() {
    struct data {
        uint8 chan_in = 0;
        uint8 chan_out = 1;
        int32 delay = 100;
    };

    struct locals {
        std::vector<sample> line;
        uint32 cursor = 0;
    };

    auto f = filter<data, none>{
        .name = "DELAY",
        .data = {},
        .size =
            [](const data&) {
                return vector2<uint32>{25, 4};
            },
        .draw =
            [](data& data, space& s) {
                ui_chan_sel(s, data.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.set_rtl(true);
                ui_chan_sel(s, data.chan_out);
                s.write("Channel OUT ");

                s.next_line();
                s.set_rtl(false);
                s.write("Delay: ");
                ui_int_ran(s, 1, INT32_MAX, 10, 5, data.delay);
            },
        .update =
            [](data& data, const none& out) {

            },
        .apply = [l = locals{}](const data& data, channels& chans) mutable -> none {
            if (data.chan_in == data.chan_out)
                return {};
            
            if (l.line.size() != data.delay) {
                l.line = std::vector<sample>(data.delay, 0.f);
                l.cursor = 0;
            }

            for (size_t i = 0;
                 i < std::min(
                         chans.chans[data.chan_in].samples.size(), chans.chans[data.chan_out].samples.size());
                 ++i) {
                chans.chans[data.chan_out].samples[i] = l.line[l.cursor];
                l.line[l.cursor++] = chans.chans[data.chan_in].samples[i];
                if (l.cursor >= l.line.size())
                    l.cursor = 0;
            }

            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
