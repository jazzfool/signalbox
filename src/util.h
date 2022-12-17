#pragma once

#include <assert.h>
#include <inttypes.h>
#include <type_traits>
#include <limits>
#include <cmath>
#include <glm/vec2.hpp>

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

using float32 = float;
using float64 = double;

template <typename T>
using Vector2 = glm::tvec2<T>;

template <typename T>
struct Rect {
    Vector2<T> pos, size;

    Vector2<T> max() const {
        return {pos.x + size.x, pos.y + size.y};
    }

    bool contains(const Vector2<T>& point) const {
        return point.x > pos.x && point.y > pos.y && point.x < max().x &&
               point.y < max().y;
    }

    Rect inflate(T d) const {
        return {{pos.x - d, pos.y - d},
            {size.x + d * static_cast<T>(2), size.y + d * static_cast<T>(2)}};
    }
};

template <typename T, typename = std::enable_if_t<std::is_floating_point_v<T>>>
inline bool floatEq(T a, T b) {
    return std::abs(a - b) < std::numeric_limits<T>::epsilon();
}
