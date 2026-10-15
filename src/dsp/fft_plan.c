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
    const unsigned int half_size = size / 2U;
    plan->reversed_indices =
        (unsigned int *)calloc(
            size, sizeof(unsigned int));
    plan->half_reversed_indices =
        (unsigned int *)calloc(
            half_size, sizeof(unsigned int));
    plan->twiddle_real =
        (float *)calloc(size - 1U, sizeof(float));
    plan->twiddle_imaginary =
        (float *)calloc(size - 1U, sizeof(float));
    if (plan->reversed_indices == NULL ||
        plan->half_reversed_indices == NULL ||
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
    for (unsigned int index = 0U; index < half_size; ++index) {
        plan->half_reversed_indices[index] =
            reverse_bits_for_plan(index, bits - 1U);
    }

    /* Direct double-precision evaluation avoids the error that a running
     * rotation recurrence accumulates across large stages. */
    for (unsigned int block_size = 2U;
         block_size <= size;
         block_size <<= 1U) {
        const unsigned int half_block =
            block_size / 2U;
        const unsigned int offset = half_block - 1U;
        const double angle_step =
            -2.0 * M_PI / (double)block_size;
        for (unsigned int index = 0U;
             index < half_block;
             ++index) {
            const double angle =
                angle_step * (double)index;
            plan->twiddle_real[offset + index] =
                (float)cos(angle);
            plan->twiddle_imaginary[offset + index] =
                (float)sin(angle);
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
    free(plan->half_reversed_indices);
    free(plan->twiddle_real);
    free(plan->twiddle_imaginary);
    *plan = (SpectraFftPlan){0};
}

static void transform(
    const SpectraFftPlan *plan,
    unsigned int size,
    const unsigned int *reversed_indices,
    float *real,
    float *imaginary) {
    for (unsigned int index = 0U; index < size; ++index) {
        const unsigned int reversed =
            reversed_indices[index];
        if (reversed > index) {
            const float real_value = real[index];
            const float imaginary_value = imaginary[index];
            real[index] = real[reversed];
            imaginary[index] = imaginary[reversed];
            real[reversed] = real_value;
            imaginary[reversed] = imaginary_value;
        }
    }

    /* The first stage always uses a unit twiddle. */
    for (unsigned int even = 0U; even < size; even += 2U) {
        const unsigned int odd = even + 1U;
        const float odd_real = real[odd];
        const float odd_imaginary = imaginary[odd];
        real[odd] = real[even] - odd_real;
        imaginary[odd] = imaginary[even] - odd_imaginary;
        real[even] += odd_real;
        imaginary[even] += odd_imaginary;
    }

    for (unsigned int block_size = 4U;
         block_size <= size;
         block_size <<= 1U) {
        const unsigned int half_block =
            block_size / 2U;
        const float *twiddle_real =
            plan->twiddle_real + (half_block - 1U);
        const float *twiddle_imaginary =
            plan->twiddle_imaginary + (half_block - 1U);
        for (unsigned int block_start = 0U;
             block_start < size;
             block_start += block_size) {
            float *even_real = real + block_start;
            float *even_imaginary = imaginary + block_start;
            float *odd_real = even_real + half_block;
            float *odd_imaginary = even_imaginary + half_block;
            for (unsigned int index = 0U;
                 index < half_block;
                 ++index) {
                const float transformed_real =
                    twiddle_real[index] * odd_real[index] -
                    twiddle_imaginary[index] *
                        odd_imaginary[index];
                const float transformed_imaginary =
                    twiddle_real[index] *
                        odd_imaginary[index] +
                    twiddle_imaginary[index] *
                        odd_real[index];
                const float first_real = even_real[index];
                const float first_imaginary =
                    even_imaginary[index];
                odd_real[index] =
                    first_real - transformed_real;
                odd_imaginary[index] =
                    first_imaginary - transformed_imaginary;
                even_real[index] =
                    first_real + transformed_real;
                even_imaginary[index] =
                    first_imaginary + transformed_imaginary;
            }
        }
    }
}

void spectra_fft_forward(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary) {
    if (plan == NULL || real == NULL ||
        imaginary == NULL || plan->size < 2U) {
        return;
    }
    transform(
        plan,
        plan->size,
        plan->reversed_indices,
        real,
        imaginary);
}

void spectra_fft_inverse(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary) {
    if (plan == NULL || real == NULL ||
        imaginary == NULL || plan->size < 2U) {
        return;
    }
    const unsigned int size = plan->size;
    for (unsigned int index = 0U; index < size; ++index) {
        imaginary[index] = -imaginary[index];
    }
    transform(
        plan, size, plan->reversed_indices, real, imaginary);
    const float scale = 1.0f / (float)size;
    for (unsigned int index = 0U; index < size; ++index) {
        real[index] *= scale;
        imaginary[index] = -imaginary[index] * scale;
    }
}

void spectra_fft_forward_real(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary) {
    if (plan == NULL || real == NULL ||
        imaginary == NULL || plan->size < 2U) {
        return;
    }
    const unsigned int size = plan->size;
    if (size < 4U) {
        for (unsigned int index = 0U; index < size; ++index) {
            imaginary[index] = 0.0f;
        }
        spectra_fft_forward(plan, real, imaginary);
        return;
    }

    /* Pack even samples as real parts and odd samples as imaginary parts of
     * a half-size complex sequence, using imaginary as storage. */
    const unsigned int half = size / 2U;
    float *packed_real = imaginary;
    float *packed_imaginary = imaginary + half;
    for (unsigned int index = 0U; index < half; ++index) {
        packed_real[index] = real[2U * index];
        packed_imaginary[index] = real[2U * index + 1U];
    }
    transform(
        plan,
        half,
        plan->half_reversed_indices,
        packed_real,
        packed_imaginary);

    /* Split the packed spectrum into the even and odd sample spectra and
     * combine them with the final-stage twiddles. real[0, size) is free
     * scratch here; bin k's imaginary part is parked at real[half + k]. */
    const float *final_twiddle_real =
        plan->twiddle_real + (half - 1U);
    const float *final_twiddle_imaginary =
        plan->twiddle_imaginary + (half - 1U);
    const float dc = packed_real[0] + packed_imaginary[0];
    const float nyquist = packed_real[0] - packed_imaginary[0];
    for (unsigned int bin = 1U; bin <= half / 2U; ++bin) {
        const unsigned int mirror = half - bin;
        const float a_real = packed_real[bin];
        const float a_imaginary = packed_imaginary[bin];
        const float b_real = packed_real[mirror];
        const float b_imaginary = packed_imaginary[mirror];
        const float even_real = 0.5f * (a_real + b_real);
        const float even_imaginary =
            0.5f * (a_imaginary - b_imaginary);
        const float odd_real =
            0.5f * (a_imaginary + b_imaginary);
        const float odd_imaginary = -0.5f * (a_real - b_real);
        const float twiddle_real = final_twiddle_real[bin];
        const float twiddle_imaginary =
            final_twiddle_imaginary[bin];
        const float rotated_real =
            twiddle_real * odd_real -
            twiddle_imaginary * odd_imaginary;
        const float rotated_imaginary =
            twiddle_real * odd_imaginary +
            twiddle_imaginary * odd_real;
        real[bin] = even_real + rotated_real;
        real[half + bin] = even_imaginary + rotated_imaginary;
        if (mirror != bin) {
            /* X[half - k] = conj(E) + (-wr + i wi) * conj(O). */
            const float mirror_rotated_real =
                -twiddle_real * odd_real +
                twiddle_imaginary * odd_imaginary;
            const float mirror_rotated_imaginary =
                twiddle_real * odd_imaginary +
                twiddle_imaginary * odd_real;
            real[mirror] = even_real + mirror_rotated_real;
            real[half + mirror] =
                -even_imaginary + mirror_rotated_imaginary;
        }
    }
    real[0] = dc;
    real[half] = nyquist;

    imaginary[0] = 0.0f;
    imaginary[half] = 0.0f;
    for (unsigned int bin = 1U; bin < half; ++bin) {
        imaginary[bin] = real[half + bin];
    }
    for (unsigned int bin = 1U; bin < half; ++bin) {
        real[size - bin] = real[bin];
        imaginary[size - bin] = -imaginary[bin];
    }
}

void spectra_fft_inverse_real(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary) {
    if (plan == NULL || real == NULL ||
        imaginary == NULL || plan->size < 2U) {
        return;
    }
    const unsigned int size = plan->size;
    if (size < 4U) {
        imaginary[0] = 0.0f;
        imaginary[size / 2U] = 0.0f;
        for (unsigned int bin = 1U; bin < size / 2U; ++bin) {
            real[size - bin] = real[bin];
            imaginary[size - bin] = -imaginary[bin];
        }
        spectra_fft_inverse(plan, real, imaginary);
        return;
    }

    /* Rebuild the half-size packed spectrum Z[k] = E[k] + i O[k]. */
    const unsigned int half = size / 2U;
    const float *final_twiddle_real =
        plan->twiddle_real + (half - 1U);
    const float *final_twiddle_imaginary =
        plan->twiddle_imaginary + (half - 1U);
    const float dc = real[0];
    const float nyquist = real[half];
    real[0] = 0.5f * (dc + nyquist);
    imaginary[0] = 0.5f * (dc - nyquist);
    for (unsigned int bin = 1U; bin <= half / 2U; ++bin) {
        const unsigned int mirror = half - bin;
        const float x_real = real[bin];
        const float x_imaginary = imaginary[bin];
        const float y_real = real[mirror];
        const float y_imaginary = imaginary[mirror];
        const float even_real = 0.5f * (x_real + y_real);
        const float even_imaginary =
            0.5f * (x_imaginary - y_imaginary);
        const float difference_real =
            0.5f * (x_real - y_real);
        const float difference_imaginary =
            0.5f * (x_imaginary + y_imaginary);
        const float twiddle_real = final_twiddle_real[bin];
        const float twiddle_imaginary =
            final_twiddle_imaginary[bin];
        /* O = D * conj(W). */
        const float odd_real =
            difference_real * twiddle_real +
            difference_imaginary * twiddle_imaginary;
        const float odd_imaginary =
            difference_imaginary * twiddle_real -
            difference_real * twiddle_imaginary;
        real[bin] = even_real - odd_imaginary;
        imaginary[bin] = even_imaginary + odd_real;
        if (mirror != bin) {
            real[mirror] = even_real + odd_imaginary;
            imaginary[mirror] = -even_imaginary + odd_real;
        }
    }

    /* Inverse half-size transform through conjugation. */
    for (unsigned int index = 0U; index < half; ++index) {
        imaginary[index] = -imaginary[index];
    }
    transform(
        plan,
        half,
        plan->half_reversed_indices,
        real,
        imaginary);

    /* Unpack: x[2n] = Re z[n], x[2n + 1] = Im z[n]. Park the real parts in
     * the unused upper half of imaginary before interleaving into real. */
    const float scale = 1.0f / (float)half;
    for (unsigned int index = 0U; index < half; ++index) {
        imaginary[half + index] = real[index];
    }
    for (unsigned int index = 0U; index < half; ++index) {
        real[2U * index] = imaginary[half + index] * scale;
        real[2U * index + 1U] = -imaginary[index] * scale;
    }
}
