#pragma once

#include "util.h"
#include "filters/sample.h"

#include <functional>
#include <memory>
#include <array>
#include <string>
#include <rigtorp/SPSCQueue.h>

struct space;

struct filter_base {
    virtual ~filter_base() {}
    virtual const std::string& name() const = 0;
    virtual vector2<uint32> size() = 0;
    virtual void draw(space& space) = 0;
    virtual void update() = 0;
    virtual void apply(channels& x) = 0;
};

template <typename TDataIn, typename TDataOut>
struct filter final {
    std::string name;
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
};

struct virtual_filter final {
    std::unique_ptr<filter_base> filter;
};
