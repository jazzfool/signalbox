#include "sbfft.h"

#include <fft.h>
#include <bit>

simd_vec<std::complex<float32>> r2c_fft(simd_slice<const sample> samples, std::size_t pad) {
    const auto n_in = std::bit_ceil(pad == 0 ? samples.size() * 2 : pad);
    const auto n_out = n_in / 2 + 1;

    auto padded = (sample*)mufft_calloc(n_in * sizeof(sample));
    ::memcpy(padded, samples.data(), samples.size() * sizeof(sample));

    auto plan = mufft_create_plan_1d_r2c(n_in, 0);
    auto out = (std::complex<float32>*)mufft_calloc(n_out * sizeof(std::complex<float32>));

    mufft_execute_plan_1d(plan, out, padded);

    auto outv = simd_vec<std::complex<float32>>(out, out + n_out);

    mufft_free(padded);
    mufft_free(out);
    mufft_free_plan_1d(plan);

    return outv;
}

simd_vec<float32> c2r_ifft(simd_slice<const std::complex<float32>> c) {
    const auto n_out = std::bit_ceil((c.size() - 1) * 2);
    const auto n_in = n_out / 2 + 1;

    auto padded = (float32*)mufft_calloc(n_in * sizeof(std::complex<float32>));
    ::memcpy(padded, c.data(), c.size() * sizeof(std::complex<float32>));

    auto plan = mufft_create_plan_1d_c2r(n_out, 0);
    auto out = (float32*)mufft_calloc(n_out * sizeof(float32));

    mufft_execute_plan_1d(plan, out, padded);

    auto outv = simd_vec<float32>(out, out + n_out / 2);

    mufft_free(padded);
    mufft_free(out);
    mufft_free_plan_1d(plan);

    return outv;
}
