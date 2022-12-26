#include "sbfft.h"

#include <fft.h>
#include <bit>

std::vector<std::complex<float32>> r2c_fft(std::span<const sample> samples) {
    auto padded = std::vector<sample>{};
    padded.assign(samples.begin(), samples.end());
    padded.insert(padded.end(), std::bit_ceil(samples.size()) - samples.size(), 0.f);

    auto plan = mufft_create_plan_1d_r2c(padded.size(), MUFFT_FORWARD);
    auto out =
        reinterpret_cast<std::complex<float32>*>(mufft_alloc((padded.size() * 2) * sizeof(std::complex<float32>)));

    mufft_execute_plan_1d(plan, out, padded.data());

    auto outv = std::vector<std::complex<float32>>(out, out + (padded.size() * 2));

    mufft_free(out);
    mufft_free_plan_1d(plan);

    return outv;
}
