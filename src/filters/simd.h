#pragma once

#include "sample.h"

#include <pmmintrin.h>

#include <vector>
#include <variant>
#include <cstdlib>
#include <numeric>
#include <span>
#include <type_traits>

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

    simd_v(__m128 v) : v{v} {
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
};

template <typename Out, typename Expr, std::size_t W = 4>
void simd_process(Out&& out, Expr&& expr, std::size_t begin, std::size_t end) {
    auto i = std::size_t{begin};

    for (; i < end / W * W; i += W) {
        simd_store(simd_width<W>{}, out, simd_load(simd_width<4>{}, expr, i), i);
    }

    for (; i < end; ++i) {
        simd_store(simd_width<1>{}, out, simd_load(simd_width<1>{}, expr, i), i);
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
