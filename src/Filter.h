#pragma once

#include "util.h"

#include <functional>
#include <memory>

struct space;

template <typename T, uint8 C>
struct sample final {
    T x[C];
};

// mono
template<typename T>
struct sample<T, 1> final {
    T x;
};

// stereo
template<typename T>
struct sample<T, 2> final {
    float32 l, r;
};

using stereo = sample<float32, 2>;

template<typename Sam>
struct filter_base {
    virtual vector2<uint32> space_size() = 0;
    virtual void draw_space(space& space) = 0;
    virtual Sam sample_filter(Sam x) = 0;
};

template <typename T, typename Sam>
struct filter final {
    T data;
    std::function<vector2<uint32>(T&)> size;
    std::function<void(T&, space&)> draw;
    std::function<Sam(T&, Sam)> filter;
};

template <typename T, typename Sam>
struct filter_fwd final : filter_base<Sam> {
    filter<T, Sam> f;

    filter_fwd(filter<T, Sam> f) : f{f} {
    }

    vector2<uint32> space_size() override {
        return f.size(f.data);
    }

    void draw_space(space& space) override {
        f.draw(f.data, space);
    }

    Sam sample_filter(Sam x) override {
        return f.filter(f.data, x);
    }
};

template<typename Sam>
struct virtual_filter final {
    std::unique_ptr<filter_base<Sam>> filter;
};
