#include "dsp/fft_plan.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned int reverse_bits_for_plan(
    unsigned int value,
    unsigned int bits) {
    unsigned int reversed = 0U;
    for (unsigned int index = 0U; index < bits; ++index) {
        reversed =
            (reversed << 1U) | (value & 1U);
        value >>= 1U;
    }
    return reversed;
}

bool spectra_fft_plan_init(
    SpectraFftPlan *plan,
    unsigned int size) {
    if (plan == NULL || size < 2U ||
        (size & (size - 1U)) != 0U) {
        return false;
    }

    *plan = (SpectraFftPlan){0};
    plan->reversed_indices =
        (unsigned int *)calloc(
            size, sizeof(unsigned int));
    plan->twiddle_real =
        (float *)calloc(size - 1U, sizeof(float));
    plan->twiddle_imaginary =
        (float *)calloc(size - 1U, sizeof(float));
    if (plan->reversed_indices == NULL ||
        plan->twiddle_real == NULL ||
        plan->twiddle_imaginary == NULL) {
        spectra_fft_plan_free(plan);
        return false;
    }

    unsigned int bits = 0U;
    for (unsigned int value = size;
         value > 1U;
         value >>= 1U) {
        ++bits;
    }
    for (unsigned int index = 0U; index < size; ++index) {
        plan->reversed_indices[index] =
            reverse_bits_for_plan(index, bits);
    }

    for (unsigned int block_size = 2U;
         block_size <= size;
         block_size <<= 1U) {
        const unsigned int half_block =
            block_size / 2U;
        const unsigned int offset = half_block - 1U;
        const float angle_step =
            -2.0f * (float)M_PI /
            (float)block_size;
        const float step_real = cosf(angle_step);
        const float step_imaginary = sinf(angle_step);
        float twiddle_real = 1.0f;
        float twiddle_imaginary = 0.0f;
        for (unsigned int index = 0U;
             index < half_block;
             ++index) {
            plan->twiddle_real[offset + index] =
                twiddle_real;
            plan->twiddle_imaginary[offset + index] =
                twiddle_imaginary;
            const float next_real =
                twiddle_real * step_real -
                twiddle_imaginary * step_imaginary;
            twiddle_imaginary =
                twiddle_real * step_imaginary +
                twiddle_imaginary * step_real;
            twiddle_real = next_real;
        }
    }
    plan->size = size;
    return true;
}

void spectra_fft_plan_free(SpectraFftPlan *plan) {
    if (plan == NULL) {
        return;
    }
    free(plan->reversed_indices);
    free(plan->twiddle_real);
    free(plan->twiddle_imaginary);
    *plan = (SpectraFftPlan){0};
}

void spectra_fft_forward(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary) {
    if (plan == NULL || real == NULL ||
        imaginary == NULL || plan->size < 2U) {
        return;
    }

    const unsigned int size = plan->size;
    for (unsigned int index = 0U; index < size; ++index) {
        const unsigned int reversed =
            plan->reversed_indices[index];
        if (reversed > index) {
            const float real_value = real[index];
            const float imaginary_value = imaginary[index];
            real[index] = real[reversed];
            imaginary[index] = imaginary[reversed];
            real[reversed] = real_value;
            imaginary[reversed] = imaginary_value;
        }
    }

    for (unsigned int block_size = 2U;
         block_size <= size;
         block_size <<= 1U) {
        const unsigned int half_block =
            block_size / 2U;
        const unsigned int offset = half_block - 1U;
        for (unsigned int block_start = 0U;
             block_start < size;
             block_start += block_size) {
            for (unsigned int index = 0U;
                 index < half_block;
                 ++index) {
                const unsigned int even =
                    block_start + index;
                const unsigned int odd =
                    even + half_block;
                const float twiddle_real =
                    plan->twiddle_real[offset + index];
                const float twiddle_imaginary =
                    plan->twiddle_imaginary[offset + index];
                const float transformed_real =
                    twiddle_real * real[odd] -
                    twiddle_imaginary * imaginary[odd];
                const float transformed_imaginary =
                    twiddle_real * imaginary[odd] +
                    twiddle_imaginary * real[odd];
                const float even_real = real[even];
                const float even_imaginary = imaginary[even];
                real[odd] = even_real - transformed_real;
                imaginary[odd] =
                    even_imaginary - transformed_imaginary;
                real[even] = even_real + transformed_real;
                imaginary[even] =
                    even_imaginary + transformed_imaginary;
            }
        }
    }
}
