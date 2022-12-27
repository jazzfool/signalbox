#include "sbfft.h"

#include <fft.h>
#include <bit>

simd_vec<std::complex<float32>> r2c_fft(simd_slice<const sample> samples, std::size_t pad) {
    const auto padded_sz = std::bit_ceil(pad == 0 ? samples.size() : pad);
    auto padded = (sample*)mufft_calloc(padded_sz * sizeof(sample));
    ::memcpy(padded, samples.data(), sizeof(sample) * samples.size());

    auto plan = mufft_create_plan_1d_r2c(padded_sz, MUFFT_FORWARD);
    auto out = reinterpret_cast<std::complex<float32>*>(
        mufft_alloc((padded_sz * 2) * sizeof(std::complex<float32>)));

    mufft_execute_plan_1d(plan, out, padded);

    auto outv = simd_vec<std::complex<float32>>(out, out + (padded_sz * 2));

    mufft_free(padded);
    mufft_free(out);
    mufft_free_plan_1d(plan);

    return outv;
}

simd_vec<float32> c2r_ifft(simd_slice<const std::complex<float32>> c) {
    const auto padded_sz = std::bit_ceil(c.size() * 2);
    auto padded = (float32*)mufft_calloc(padded_sz * sizeof(sample));
    ::memcpy(padded, c.data(), sizeof(sample) * c.size() * 2);

    auto plan = mufft_create_plan_1d_c2r(c.size(), MUFFT_INVERSE);
    auto out = (float32*)mufft_alloc(sizeof(float32) * padded_sz * 2);

    mufft_execute_plan_1d(plan, out, padded);

    auto outv = simd_vec<float32>(out, out + padded_sz * 2);

    mufft_free(out);
    mufft_free_plan_1d(plan);

    return outv;
}
