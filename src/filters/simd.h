#pragma once

#include "sample.h"
#include "sse.h"

#include <vector>
#include <variant>
#include <cstdlib>
#include <numeric>
#include <span>
#include <type_traits>
#include <complex>

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

template<typename T, typename U>
bool operator==(const simd_allocator<T>&, const simd_allocator<U>&) {
    return true;
}

template<typename T, typename U>
bool operator!=(const simd_allocator<T>&, const simd_allocator<U>&) {
    return false;
}

template <typename T>
struct simd_real_to_complex final {
    using type = std::complex<T>;
};

template <typename T>
struct simd_real_to_complex<const T> final {
    using type = const std::complex<T>;
};

template <typename T>
using simd_real_to_complex_t = typename simd_real_to_complex<T>::type;

template <typename T>
struct simd_complex_to_real final {};

template <typename T>
struct simd_complex_to_real<std::complex<T>> final {
    using type = T;
};

template <typename T>
struct simd_complex_to_real<const std::complex<T>> final {
    using type = const T;
};

template <typename T>
using simd_complex_to_real_t = typename simd_complex_to_real<T>::type;

static constexpr std::size_t SIMD_VEC_DYNAMIC = std::numeric_limits<std::size_t>::max();

template <typename T, std::size_t N = SIMD_VEC_DYNAMIC>
struct simd_vec;

template <typename T>
using simd_slice = simd_vec<T, 0>;

template <typename T, std::size_t N>
struct simd_vec final : std::array<T, N> {
    using std::array<T, N>::array;

    simd_slice<T> slice(std::size_t offset, std::size_t count) {
        sb_ASSERT(count <= this->size() - offset);
        return simd_slice<T>{this->data() + offset, count};
    }

    simd_slice<const T> slice(std::size_t offset, std::size_t count) const {
        sb_ASSERT(count <= this->size() - offset);
        return simd_slice<T>{this->data() + offset, count};
    }

    simd_slice<T> whole() {
        return slice(0, this->size());
    }

    simd_slice<const T> whole() const {
        return slice(0, this->size());
    }
};

template <typename T>
struct simd_vec<T, SIMD_VEC_DYNAMIC> final : std::vector<T, simd_allocator<T>> {
    using std::vector<T, simd_allocator<T>>::vector;

    simd_slice<T> slice(std::size_t offset, std::size_t count) {
        sb_ASSERT(count <= this->size() - offset);
        return simd_slice<T>{this->data() + offset, count};
    }

    simd_slice<const T> slice(std::size_t offset, std::size_t count) const {
        sb_ASSERT(count <= this->size() - offset);
        return simd_slice<const T>{this->data() + offset, count};
    }

    simd_slice<T> whole() {
        return slice(0, this->size());
    }

    simd_slice<const T> whole() const {
        return slice(0, this->size());
    }
};

template <typename T>
struct simd_vec<T, 0> final : std::span<T> {
    using std::span<T>::span;

    template<typename U = T>
    simd_slice<simd_complex_to_real_t<U>> as_real() const {
        return simd_slice<simd_complex_to_real_t<T>>{
            reinterpret_cast<simd_complex_to_real_t<T>*>(this->data()), this->size() * 2};
    }

    simd_slice<simd_real_to_complex_t<T>> as_complex() const {
        sb_ASSERT(this->size() % 2 == 0);
        return simd_slice<simd_real_to_complex_t<T>>{
            reinterpret_cast<simd_real_to_complex_t<T>*>(this->data()), this->size() / 2};
    }
};

inline float32 simd_hmax_ps(__m128 x) {
    const auto m1 = _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 0, 3, 2));
    const auto m2 = _mm_max_ps(x, m1);
    const auto m3 = _mm_shuffle_ps(m2, m2, _MM_SHUFFLE(0, 0, 0, 1));
    const auto m4 = _mm_max_ps(m2, m3);
    return _mm_cvtss_f32(m4);
}

inline float32 simd_max(simd_slice<const float32> v) {
    const auto sz = v.size();
    auto i = size_t{0};
    auto _max = _mm_setzero_ps();
    for (; i < sz / 4 * 4; i += 4) {
        _max = _mm_max_ps(_mm_load_ps(&v[i]), _max);
    }
    auto max = simd_hmax_ps(_max);
    for (; i < sz; ++i) {
        max = std::max(v[i], max);
    }
    return max;
}

inline void simd_mul1(simd_slice<const float32> in, simd_slice<float32> out, float32 x) {
    sb_ASSERT_EQ(in.size(), out.size());
    const auto sz = in.size();
    auto i = size_t{0};
    const auto _x = _mm_set1_ps(x);
    for (; i < sz / 4 * 4; i += 4) {
        _mm_store_ps(&out[i], _mm_mul_ps(_mm_load_ps(&in[i]), _x));
    }
    for (; i < sz; ++i) {
        out[i] = in[i] * x;
    }
}
