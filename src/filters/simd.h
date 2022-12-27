#pragma once

#include "sample.h"
#include "sse.h"

#include <pmmintrin.h>

#include <vector>
#include <variant>
#include <cstdlib>
#include <numeric>
#include <span>
#include <type_traits>

#define _USE_MATH_DEFINES
#include <math.h>

static constexpr std::size_t SIMD_ALIGN = 16;

template <typename T>
class simd_allocator {
  public:
    using value_type = T;
    static constexpr std::align_val_t ALIGN{SIMD_ALIGN};

    template <typename U>
    struct rebind final {
        using other = simd_allocator<U>;
    };

  public:
    constexpr simd_allocator() noexcept = default;
    constexpr simd_allocator(const simd_allocator&) noexcept = default;
    template <typename U>
    constexpr simd_allocator(simd_allocator<U> const&) noexcept {
    }

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            throw std::bad_array_new_length();
        }
        auto const n_bytes = n * sizeof(T);
        return reinterpret_cast<T*>(::operator new[](n_bytes, ALIGN));
    }

    void deallocate(T* ptr, [[maybe_unused]] std::size_t n_bytes) {
        ::operator delete[](ptr, ALIGN);
    }
};

struct simd_expr {};

template <typename... Ts>
using simd_enable_expr_t = std::enable_if_t<(std::is_base_of_v<simd_expr, std::remove_cvref_t<Ts>> && ...)>;

template <typename T, std::size_t W>
struct simd_v final {};

template <>
struct simd_v<float32, 1> final : simd_expr {
    using element_type = float32;

    float32 v;

    simd_v(float32 v) : v{v} {
    }

    static simd_v splat(float32 x) {
        return simd_v{x};
    }

    static simd_v set(float32 x) {
        return simd_v{x};
    }

    simd_v operator-() const {
        return -v;
    }

    simd_v operator+(const simd_v& rhs) const {
        return v + rhs.v;
    }

    simd_v operator-(const simd_v& rhs) const {
        return v - rhs.v;
    }

    simd_v operator*(const simd_v& rhs) const {
        return v * rhs.v;
    }

    simd_v operator/(const simd_v& rhs) const {
        return v / rhs.v;
    }
};

template <>
struct simd_v<float32, 4> final : simd_expr {
    using element_type = float32;

    __m128 v;

    simd_v(__m128 v = {}) : v{v} {
    }

    // !! x -> aligned !!
    static simd_v load(const float32* x) {
        return simd_v{_mm_load_ps(x)};
    }

    static simd_v set(float32 x0, float32 x1, float32 x2, float32 x3) {
        return simd_v{_mm_setr_ps(x0, x1, x2, x3)};
    }

    static simd_v splat(float32 x) {
        return simd_v{_mm_set1_ps(x)};
    }

    simd_v operator-() const {
        return _mm_sub_ps(splat(0.f).v, v);
    }

    simd_v operator+(const simd_v& rhs) const {
        return _mm_add_ps(v, rhs.v);
    }

    simd_v operator-(const simd_v& rhs) const {
        return _mm_sub_ps(v, rhs.v);
    }

    simd_v operator*(const simd_v& rhs) const {
        return _mm_mul_ps(v, rhs.v);
    }

    simd_v operator/(const simd_v& rhs) const {
        return _mm_div_ps(v, rhs.v);
    }

    float32 sum() const {
        // https://stackoverflow.com/a/35270026
        auto shuf = _mm_movehdup_ps(v);
        auto sums = _mm_add_ps(v, shuf);
        shuf = _mm_movehl_ps(shuf, sums);
        sums = _mm_add_ss(sums, shuf);
        return _mm_cvtss_f32(sums);
    }

    simd_v sin() const {
        return simd_v{simd_sin(v)};
    }
};

template <typename Out, typename Expr, std::size_t W = 4>
void simd_process(Out&& out, Expr&& expr, std::size_t begin, std::size_t end) {
    sb_ASSERT(simd_size(std::forward<Out>(out)) <= simd_size(std::forward<Expr>(expr)));

    auto i = std::size_t{begin};

    for (; i < end / W * W; i += W) {
        simd_store(
            simd_width<W>{}, std::forward<Out>(out), simd_load(simd_width<4>{}, std::forward<Expr>(expr), i),
            i);
    }

    for (; i < end; ++i) {
        simd_store(
            simd_width<1>{}, std::forward<Out>(out), simd_load(simd_width<1>{}, std::forward<Expr>(expr), i),
            i);
    }
}

static constexpr std::size_t SIMD_VEC_DYNAMIC = std::numeric_limits<std::size_t>::max();

template <typename T, std::size_t N = SIMD_VEC_DYNAMIC>
struct simd_vec;

template <typename T, std::size_t N>
struct simd_vec final : simd_expr, std::array<T, N> {
    using std::array<T, N>::array;
    using element_type = T;

    template <typename Expr, typename = simd_enable_expr_t<Expr>>
    simd_vec& operator=(Expr&& expr) {
        simd_process(*this, std::forward<Expr>(expr), 0, size());
        return *this;
    }

    simd_vec<T, 0> slice(std::size_t offset, std::size_t count) {
        sb_ASSERT(count <= size() - offset);
        return simd_vec<T, 0>{data() + offset, count};
    }

    simd_vec<const T, 0> slice(std::size_t offset, std::size_t count) const {
        sb_ASSERT(count <= size() - offset);
        return simd_vec<T, 0>{data() + offset, count};
    }
};

template <typename T>
struct simd_vec<T, SIMD_VEC_DYNAMIC> final : simd_expr, std::vector<T, simd_allocator<T>> {
    using std::vector<T, simd_allocator<T>>::vector;
    using element_type = T;

    template <typename Expr, typename = simd_enable_expr_t<Expr>>
    simd_vec& operator=(Expr&& expr) {
        simd_process(*this, std::forward<Expr>(expr), 0, this->size());
        return *this;
    }

    simd_vec<T, 0> slice(std::size_t offset, std::size_t count) {
        sb_ASSERT(count <= this->size() - offset);
        return simd_vec<T, 0>{this->data() + offset, count};
    }

    simd_vec<const T, 0> slice(std::size_t offset, std::size_t count) const {
        sb_ASSERT(count <= this->size() - offset);
        return simd_vec<T, 0>{this->data() + offset, count};
    }
};

template <typename T>
struct simd_vec<T, 0> final : simd_expr, std::span<T> {
    using std::span<T>::span;
    using element_type = T;

    template <typename Expr, typename = simd_enable_expr_t<Expr>>
    simd_vec& operator=(Expr&& expr) {
        simd_process(*this, std::forward<Expr>(expr), 0, this->size());
        return *this;
    }
};

template <std::size_t W>
struct simd_width final {};

template <std::size_t N>
simd_v<float32, 4> simd_load(simd_width<4>, const simd_vec<float32, N>& v, std::size_t i) {
    return {_mm_load_ps(v.data() + i)};
}

template <std::size_t N>
simd_v<float32, 1> simd_load(simd_width<1>, const simd_vec<float32, N>& v, std::size_t i) {
    return {*(v.data() + i)};
}

template <std::size_t N>
void simd_store(simd_width<4>, simd_vec<float32, N>& v, simd_v<float32, 4> in, std::size_t i) {
    _mm_store_ps(v.data() + i, in.v);
}

template <std::size_t N>
void simd_store(simd_width<1>, simd_vec<float32, N>& v, simd_v<float32, 1> in, std::size_t i) {
    *(v.data() + i) = in.v;
}

template <typename T, std::size_t N>
std::size_t simd_size(const simd_vec<T, N>& v) {
    return v.size();
}

template <typename T, std::size_t W>
struct simd_const final : simd_expr {
    static_assert(W > 0);

    using element_type = T;
    alignas(16) const std::array<T, W> values;
    simd_v<float32, 4> v4;

    simd_const(std::array<T, W>&& vs) : values{std::move(vs)} {
        alignas(16) const float32 v4s[4] = {values[0 % W], values[1 % W], values[2 % W], values[3 % W]};
        v4 = simd_v<float32, 4>::load(v4s);
    }

    simd_const(const T v) requires(W == 1) : values{fill_array<T, W>(v)} {
        v4 = simd_v<float32, 4>::splat(v);
    }
};

template <typename T, std::size_t W1, std::size_t W2>
simd_v<float32, W1> simd_load(simd_width<W1>, const simd_const<T, W2>& c, std::size_t i) {
    return simd_v<float32, W1>::splat(c.values[i % W2]);
}

template <typename T, std::size_t W>
simd_v<float32, 4> simd_load(simd_width<4>, const simd_const<T, W>& c, std::size_t) {
    return c.v4;
}

template <typename T, std::size_t W>
std::size_t simd_size(const simd_const<T, W>&) {
    return std::numeric_limits<std::size_t>::max();
}

template <typename A, typename B>
struct simd_binary_expr : simd_expr {
    static_assert(std::is_same_v<typename A::element_type, typename B::element_type>);
    using element_type = typename A::element_type;
    const std::size_t size;
    const std::remove_cvref_t<A>* a;
    const std::remove_cvref_t<B>* b;
    simd_binary_expr(const A& a, const B& b)
        : size{std::min<std::size_t>(simd_size(a), simd_size(b))}, a{&a}, b{&b} {
    }
};

template <typename A, typename B>
std::size_t simd_size(const simd_binary_expr<A, B>& expr) {
    return expr.size;
}

#define SIMD_BINARY_EXPR(name, op)                                                                           \
    template <typename A, typename B>                                                                        \
    struct simd_expr_##name final : simd_binary_expr<A, B> {                                                 \
        using simd_binary_expr<A, B>::simd_binary_expr;                                                      \
    };                                                                                                       \
                                                                                                             \
    template <typename A, typename B, std::size_t W>                                                         \
    auto simd_load(simd_width<W> w, const simd_expr_##name<A, B>& expr, std::size_t i) {                     \
        return simd_load(w, *expr.a, i) op simd_load(w, *expr.b, i);                                         \
    }                                                                                                        \
                                                                                                             \
    template <typename A, typename B, typename = simd_enable_expr_t<A, B>>                                   \
    simd_expr_##name<A, B> operator op(const A& a, const B& b) {                                             \
        return {a, b};                                                                                       \
    }

SIMD_BINARY_EXPR(add, +)
SIMD_BINARY_EXPR(sub, -)
SIMD_BINARY_EXPR(mul, *)
SIMD_BINARY_EXPR(div, /)

template <typename Expr, std::size_t W = 4, typename = simd_enable_expr_t<Expr>>
auto simd_sum(Expr&& expr) {
    auto acc = simd_v<typename std::remove_cvref_t<Expr>::element_type, W>::splat(0.f);
    const auto sz = simd_size(expr);
    auto x = std::size_t{0};
    for (; x < sz / W * W; x += W) {
        const auto v = simd_load(simd_width<W>{}, expr, x);
        acc = acc + v;
    }
    auto f = acc.sum();
    for (; x < sz; ++x) {
        f += simd_load(simd_width<1>{}, expr, x).v;
    }
    return f;
}

template <typename T>
struct simd_range_expr final : simd_expr {
    using element_type = typename T;
    const std::size_t size;
    const T inv_size;
    const T start;
    const T stop;
    const T dist;
    simd_range_expr(T start, T stop, std::size_t size)
        : size{size}, inv_size{static_cast<T>(1) / static_cast<T>(size)}, start{start}, stop{stop},
          dist{stop - start} {
    }
};

template <typename T>
auto simd_range(T start, T stop, std::size_t size) {
    return simd_range_expr<T>{start, stop, size};
}

template <typename T, std::size_t W>
auto simd_load(simd_width<W>, const simd_range_expr<T>& expr, std::size_t i) {
    return simd_load<T, W>(simd_width<W>{}, expr, i, std::make_index_sequence<W>::value);
}

template <typename T, std::size_t W, std::size_t... I, typename = std::enable_if_t<(sizeof...(I) == W)>>
auto simd_load(simd_width<W>, const simd_range_expr<T>& expr, std::size_t i, std::index_sequence<I...>) {
    return simd_v<T, W>::set((expr.start + static_cast<T>(I + i) * expr.inv_size * expr.dist)...);
}

template <typename T>
std::size_t simd_size(const simd_range_expr<T>& expr) {
    return expr.size;
}

template <typename T>
struct simd_window_expr : simd_expr {
    const simd_range_expr<T> range;
    simd_window_expr(T start, T stop, std::size_t size) : range{start, stop, size} {
    }
};

template <typename T>
std::size_t simd_size(const simd_window_expr<T>& expr) {
    return simd_size(expr.range);
}

template <typename T>
struct simd_hann_window_expr final : simd_window_expr<T> {
    using simd_window_expr<T>::simd_window_expr;
};

template <typename T>
auto simd_hann_window(T start, T stop, std::size_t size) {
    return simd_hann_window_expr{start, stop, size};
}

template <typename T, std::size_t W>
auto simd_load(simd_width<W> w, const simd_hann_window_expr<T>& expr, std::size_t i) {
    static auto _0_5 = simd_const<T, W>::splat(static_cast<T>(0.5f));
    static auto _1_0 = simd_const<T, W>::splat(static_cast<T>(1.f));
    static auto _2_pi = simd_const<T, W>::splat(static_cast<T>(M_PI * 2.0));
    return _0_5 * (_1_0 - cos_ps(_2_pi * simd_load(w, expr.range, i)));
}
