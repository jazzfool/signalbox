#include "chn.h"

#include "space.h"
#include "widget.h"

#include <spdlog/fmt/fmt.h>

std::unique_ptr<filter_base> fltr_chn_split() {
    struct data {
        uint8 in_chan;
        int32 num_out_chans;
        std::vector<uint8> out_chans;
    };

    auto f = filter<data, none> {
        .name = "SPLIT", .data = {0, 2, {0, 1}},
        .size =
            [](const auto& data) {
                return vector2<uint32>{25, 3 + data.num_out_chans};
            },
        .draw =
            [](auto& data, auto& s) {
                ui_chan_sel(s, data.in_chan);
                s.write(" Channel IN");

                s.next_line();
                s.write("Channels ");
                ui_int_ran(s, 0, 8, 1, 2, data.num_out_chans);
                s.write(" OUT");

                if (data.out_chans.size() != data.num_out_chans) {
                    data.out_chans.resize(data.num_out_chans, 0);
                }

                s.set_rtl(true);
                auto idx = 0;
                for (auto& chan : data.out_chans) {
                    s.next_line();
                    ui_chan_sel(s, chan);
                    s.write(fmt::format("Channel {:02} OUT ", idx));
                    ++idx;
                }
            },
        .update = [](auto& data, const auto& in) {}, .apply = [](const auto& data, auto& chans) -> none {
            const auto in = chans.chans[data.in_chan];
            for (auto chan : data.out_chans) {
                if (chan == data.in_chan)
                    continue;
                chans.chans[chan] = in;
            }
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
