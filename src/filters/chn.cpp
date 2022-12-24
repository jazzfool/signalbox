#include "chn.h"

#include "space.h"
#include "widget.h"

#include <spdlog/fmt/fmt.h>

std::unique_ptr<filter_base> fltr_chn_split() {
    struct data : fd_chan_in<1>, fd_chan_vec_out {
        int32 num_out_chans = 2;
    };

    auto f = filter<data, none>{
        .name = "SPLIT",
        .kind = filter_kind::chn,
        .data = {},
        .size =
            [](const auto& data) {
                return vector2<uint32>{25, 3 + data.num_out_chans};
            },
        .draw =
            [](auto& data, auto& s) {
                ui_chan_sel(s, data.chan_in);
                s.write(" Channel IN");

                s.next_line();
                s.write("Channels ");
                ui_int_ran(s, 0, 8, 1, 2, data.num_out_chans);
                s.write(" OUT");

                if (data.chan_out.size() != data.num_out_chans) {
                    data.chan_out.resize(data.num_out_chans, 0);
                }

                s.set_rtl(true);
                auto idx = 0;
                for (auto& chan : data.chan_out) {
                    s.next_line();
                    ui_chan_sel(s, chan);
                    s.write(fmt::format("Channel {:02} OUT ", idx));
                    ++idx;
                }
            },
        .update = [](data& data, const none& in) {},
        .apply = [](const data& data, channels& chans) -> none {
            const auto& in = chans.chans[data.chan_in];
            for (auto chan : data.chan_out) {
                if (chan == data.chan_in)
                    continue;
                chans.chans[chan] = in;
            }
            return {};
        },
    };

    return std::make_unique<filter_fwd<data, none>>(std::move(f));
}
