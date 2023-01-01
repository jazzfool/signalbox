#pragma once

#include "util.h"
#include "filters/sample.h"

#include <functional>
#include <memory>
#include <array>
#include <string>
#include <span>
#include <vector>
#include <rigtorp/SPSCQueue.h>

struct space;

struct fd_chans_in {
    virtual std::span<uint8> chans_in() = 0;
    virtual bool chans_in_is_dynamic() = 0;
};

struct fd_chans_out {
    virtual std::span<uint8> chans_out() = 0;
    virtual bool chans_out_is_dynamic() = 0;
};

template <uint32 N, typename = std::enable_if_t<(N > 0)>>
struct fd_chan_in : fd_chans_in {
    std::array<uint8, N> chan_in = fill_array<uint8, N>(0);

    std::span<uint8> chans_in() override {
        return chan_in;
    }

    bool chans_in_is_dynamic() override {
        return false;
    }
};

template <>
struct fd_chan_in<1> : fd_chans_in {
    uint8 chan_in = 0;

    std::span<uint8> chans_in() override {
        return {&chan_in, 1};
    }

    bool chans_in_is_dynamic() override {
        return false;
    }
};

struct fd_chan_vec_in : fd_chans_in {
    std::vector<uint8> chan_in;

    std::span<uint8> chans_in() override {
        return chan_in;
    }

    bool chans_in_is_dynamic() override {
        return true;
    }
};

template <uint32 N>
struct fd_chan_out : fd_chans_out {
    std::array<uint8, N> chan_out = fill_array<uint8, N>(0);

    std::span<uint8> chans_out() override {
        return chan_out;
    }

    bool chans_out_is_dynamic() override {
        return false;
    }
};

template <>
struct fd_chan_out<1> : fd_chans_out {
    uint8 chan_out = 0;

    std::span<uint8> chans_out() override {
        return {&chan_out, 1};
    }

    bool chans_out_is_dynamic() override {
        return false;
    }
};

struct fd_chan_vec_out : fd_chans_out {
    std::vector<uint8> chan_out;

    std::span<uint8> chans_out() override {
        return chan_out;
    }

    bool chans_out_is_dynamic() override {
        return true;
    }
};

enum class filter_kind { chn, gen, viz, fir, iir, trk, misc, _max };

static constexpr const char* filter_kind_names[(size_t)filter_kind::_max] = {
    "Channel", "Generation", "Visualization", "FIR", "IIR", "Tracker", "Miscellaneous"};

struct filter_base {
    virtual ~filter_base() {
    }

    virtual const std::string& name() const = 0;
    virtual filter_kind kind() const = 0;

    virtual vector2<uint32> size() = 0;
    virtual void draw(space& space) = 0;
    virtual void update() = 0;
    virtual void apply(channels& x) = 0;

    virtual fd_chans_in* data_chans_in() = 0;
    virtual fd_chans_out* data_chans_out() = 0;
};

template <typename TDataIn, typename TDataOut>
struct filter final {
    std::string name;
    filter_kind kind = filter_kind::misc;
    TDataIn data;
    std::function<vector2<uint32>(const TDataIn&)> size;
    std::function<void(TDataIn&, space&)> draw;
    std::function<void(TDataIn&, const TDataOut&)> update;
    std::function<TDataOut(const TDataIn&, channels&)> apply;
};

template <typename TDataIn, typename TDataOut>
struct filter_fwd final : filter_base {
    filter<TDataIn, TDataOut> f;

    rigtorp::SPSCQueue<TDataIn> data_in;
    rigtorp::SPSCQueue<TDataOut> data_out;
    TDataIn data_hold;

    filter_fwd(filter<TDataIn, TDataOut>&& f) : f{std::move(f)}, data_in{32}, data_out{32} {
        data_hold = this->f.data;
    }

    filter_fwd(const filter_fwd&) = delete;
    filter_fwd& operator=(const filter_fwd&) = delete;

    filter_fwd(filter_fwd&& other)
        : f{std::move(other.f)}, data_in{std::move(other.data_in)}, data_out{std::move(other.data_out)},
          data_hold{std::move(other.data_hold)} {
    }

    filter_fwd& operator=(filter_fwd&& rhs) {
        if (this != &rhs) {
            f = std::move(rhs.f);
            data_in = std::move(rhs.data_in);
            data_out = std::move(rhs.data_out);
        }
        return *this;
    }

    const std::string& name() const override {
        return f.name;
    }

    filter_kind kind() const override {
        return f.kind;
    }

    vector2<uint32> size() override {
        return f.size(f.data);
    }

    void draw(space& space) override {
        f.draw(f.data, space);
        data_in.try_push(f.data);
    }

    void update() override {
        while (data_out.front()) {
            f.update(f.data, *data_out.front());
            data_out.pop();
        }
        data_in.try_push(f.data);
    }

    void apply(channels& chans) override {
        while (data_in.front()) {
            data_hold = *data_in.front();
            data_in.pop();
        }
        const auto out = f.apply(data_hold, chans);
        data_out.try_push(std::move(out));
    }

    fd_chans_in* data_chans_in() override {
        if constexpr (std::is_base_of_v<fd_chans_in, TDataIn>) {
            return &f.data;
        } else {
            return nullptr;
        }
    }

    fd_chans_out* data_chans_out() override {
        if constexpr (std::is_base_of_v<fd_chans_out, TDataIn>) {
            return &f.data;
        } else {
            return nullptr;
        }
    }
};

struct virtual_filter final {
    std::unique_ptr<filter_base> filter;
};

using filter_fn = std::unique_ptr<filter_base> (*)();

template <typename TDataOut, typename TDataIn>
std::unique_ptr<filter_base> make_filter(filter<TDataOut, TDataIn>&& f) {
    return std::make_unique<filter_fwd<TDataOut, TDataIn>>(std::move(f));
}
