#pragma once

#include <assert.h>
#include <inttypes.h>
#include <type_traits>
#include <limits>
#include <cmath>
#include <glm/vec2.hpp>
#include <array>
#include <string_view>
#include <vector>
#include <cctype>
#include <type_traits>
#include <utility>

#define sb_ASSERT(x) assert((x))
#define sb_ASSERT_EQ(x, y) assert(((x) == (y)))

using uint8 = uint8_t;
using uint16 = uint16_t;
using uint32 = uint32_t;
using uint64 = uint64_t;

using int8 = int8_t;
using int16 = int16_t;
using int32 = int32_t;
using int64 = int64_t;

using char8 = char8_t;
using char16 = char16_t;
using char32 = char32_t;

using float32 = float;
using float64 = double;

template <typename T>
using vector2 = glm::tvec2<T>;

#define NVG_RECT_ARGS(arg_rect) arg_rect.pos.x, arg_rect.pos.y, arg_rect.size.x, arg_rect.size.y

template <typename T>
struct rect2 {
    vector2<T> pos, size;

    vector2<T> max() const {
        return {pos.x + size.x, pos.y + size.y};
    }

    vector2<T> center() const {
        return {
            pos.x + static_cast<T>(static_cast<float64>(size.x) / 2.f),
            pos.y + static_cast<T>(static_cast<float64>(size.y) / 2.f),
        };
    }

    bool contains(const vector2<T>& point) const {
        return point.x > pos.x && point.y > pos.y && point.x < max().x && point.y < max().y;
    }

    rect2 inflate(T d) const {
        return {{pos.x - d, pos.y - d}, {size.x + d * static_cast<T>(2), size.y + d * static_cast<T>(2)}};
    }

    rect2 translate(const vector2<T>& xy) const {
        return {{pos + xy}, size};
    }

    template <typename U = T, typename = std::enable_if_t<std::is_floating_point_v<U>>>
    rect2 half_round() const {
        return {
            {std::round(pos.x) + static_cast<T>(0.5), std::round(pos.y) + static_cast<T>(0.5)},
            {std::round(size.x), std::round(size.y)}};
    }
};

template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
inline bool float_eq(T a, T b) {
    return std::abs(a - b) < std::numeric_limits<T>::epsilon();
}

template <typename T>
inline T clamp(T min, T max, T x) {
    return std::min(max, std::max(min, x));
}

template <typename T>
inline T remap(T in_lo, T in_hi, T out_lo, T out_hi, T x) {
    return out_lo + (x - in_lo) * (out_hi - out_lo) / (in_hi - in_lo);
}

template <typename T, std::size_t N>
constexpr std::array<T, N> fill_array(const T& value) {
    std::array<T, N> ret;
    ret.fill(value);
    return ret;
}

struct none final {};

inline uint32 str_distance(std::string_view a, std::string_view b) {
    // levenshtein
    const auto sz_a = a.size() + 1;
    const auto sz_b = b.size() + 1;
    const auto sz = sz_a * sz_b;
    auto dists = std::vector<uint32>{};
    dists.resize(sz, 0);
    for (auto i = size_t{0}; i < sz; ++i) {
        const auto i_a = i % sz_a;
        const auto i_b = i / sz_a;
        if (i_a == 0 || i_b == 0) {
            dists[i] = i_a + i_b;
            continue;
        }
        const auto cost = static_cast<uint32>(std::tolower(a[i_a - 1]) != std::tolower(b[i_b - 1]));
        dists[i] = std::min(std::min(dists[i - sz_a] + 1, dists[i - 1] + 1), dists[i - sz_a - 1] + cost);
    }
    return dists.back();
}

inline uint32 rdbytesu32le(const uint8* p) {
    return (uint32)p[3] << 24 | (uint32)p[2] << 16 | (uint32)p[1] << 8 | (uint32)p[0];
}

inline uint64 rdbytesu64le(const uint8* p) {
    return (uint64)p[7] << 56 | (uint64)p[6] << 48 | (uint64)p[5] << 40 | (uint64)p[4] << 32 |
           (uint64)p[3] << 24 | (uint64)p[2] << 16 | (uint64)p[1] << 8 | (uint64)p[0];
}
