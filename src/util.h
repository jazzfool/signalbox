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
#include <glm/common.hpp>
#include <robin_hood.h>
#include <span>

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
using Vector2 = glm::tvec2<T>;

#define NVG_RECT_ARGS(arg_rect) arg_rect.pos.x, arg_rect.pos.y, arg_rect.size.x, arg_rect.size.y

template <typename T>
struct Rect2 {
    Vector2<T> pos, size;

    static Rect2 from_min_max(Vector2<T> min, Vector2<T> max) {
        return {min, max - min};
    }

    template <typename... Args>
    static Rect2 from_point_fit(const Args&... arg) {
        Vector2<T> min, max;
        ((min = glm::min(min, arg), max = glm::max(max, arg)), ...);
        return Rect2::from_min_max(min, max);
    }

    static Rect2 from_point_list_fit(std::span<Vector2<T>> points) {
        Vector2<T> min, max;
        for (const auto& p : points) {
            min = glm::min(min, p);
            max = glm::max(max, p);
        }
        return Rect2::from_min_max(min, max);
    }

    Vector2<T> min() const {
        return pos;
    }

    Vector2<T> max() const {
        return {pos.x + size.x, pos.y + size.y};
    }

    Vector2<T> center() const {
        return {
            pos.x + static_cast<T>(static_cast<float64>(size.x) / 2.f),
            pos.y + static_cast<T>(static_cast<float64>(size.y) / 2.f),
        };
    }

    bool contains(const Vector2<T>& point) const {
        return point.x > pos.x && point.y > pos.y && point.x < max().x && point.y < max().y;
    }

    Rect2 inflate(T d) const {
        return {{pos.x - d, pos.y - d}, {size.x + d + d, size.y + d + d}};
    }

    Rect2 inflate(const Vector2<T>& d) const {
        return {{pos.x - d.x, pos.y - d.y}, {size.x + d.x + d.x, size.y + d.y + d.y}};
    }

    Rect2 translate(const Vector2<T>& xy) const {
        return {{pos + xy}, size};
    }

    template <typename U = T, typename = std::enable_if_t<std::is_floating_point_v<U>>>
    Rect2 half_round() const {
        return {
            {std::round(pos.x) + static_cast<T>(0.5), std::round(pos.y) + static_cast<T>(0.5)},
            {std::round(size.x), std::round(size.y)}};
    }

    Rect2 rect_union(const Rect2& r) {
        return {glm::min(min(), r.min()), std::max(max(), r.max())};
    }
};

using Rect2_F32 = Rect2<float32>;
using Vector2_F32 = Vector2<float32>;
using Vector2_Bool = Vector2<bool>;

template <typename T>
struct Math_Consts final {
    static constexpr T pi_mul(T x) {
        return Math_Consts::pi * x;
    }

    static constexpr T pi_div(T x) {
        return Math_Consts::pi / x;
    }

    static constexpr T pi = static_cast<T>(3.1415926535897932384626433832795);
    static constexpr T e = static_cast<T>(2.718281828459045235360287471352662);
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

inline float32 lerp(float32 a, float32 b, float32 t) {
    return a * (1.f - t) + b * t;
}

struct None final {};

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

inline void hash_combine(std::size_t& seed) {
}

template <typename T>
struct Hash_Decay final {
    using type = T;
};

template <>
struct Hash_Decay<const char*> final {
    using type = std::string_view;
};

template <>
struct Hash_Decay<char*> final {
    using type = std::string_view;
};

struct Pre_Hash final {
    uint64 hash;
};

inline constexpr uint32 fnv1a_32(const char* s, uint64 count) {
    return count ? (fnv1a_32(s, count - 1) ^ s[count - 1]) * 16777619u : 2166136261u;
}

consteval Pre_Hash consthash(const std::string_view& str) {
    return {static_cast<uint64>(fnv1a_32(str.data(), str.size()))};
}

namespace std {
template <>
struct hash<Pre_Hash> {
    size_t operator()(const Pre_Hash& x) const {
        return x.hash;
    }
};
} // namespace std

namespace robin_hood {
template <>
struct hash<Pre_Hash> {
    size_t operator()(const Pre_Hash& x) const {
        return x.hash;
    }
};
} // namespace robin_hood

template <typename T, typename... Rest>
void hash_combine(std::size_t& seed, const T& v, const Rest&... rest) {
    using hd_t = typename Hash_Decay<std::decay_t<T>>::type;
    seed ^= robin_hood::hash<hd_t>{}(hd_t{v}) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    (hash_combine(seed, rest), ...);
}

template <typename T>
inline std::string to_string2(T&& x) {
    return std::to_string(x);
}

inline std::string to_string2(const char* x) {
    return std::string{x};
}

inline std::string to_string_cat(auto... arg) {
    std::string s;
    s = (to_string2(std::forward<decltype(arg)>(arg)) + ...);
    return s;
}
