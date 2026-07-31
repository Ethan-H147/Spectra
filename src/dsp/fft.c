#include "dsp/fft.h"

#include "dsp/fft_plan.h"
#include "dsp/signal_utils.h"
#include "dsp/windowing.h"
#include "platform/parallel_for.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAX_FFT_SIZE 16384U

typedef struct {
    const float *samples;
    const float *window;
    const SpectraFftPlan *plan;
    float *worker_real;
    float *worker_imaginary;
    double *worker_power;
    size_t regular_frame_count;
    size_t last_frame_start;
    size_t hop_size;
    size_t bin_count;
    double power_scale;
} AveragedSpectrumContext;

static void accumulate_spectrum_frames(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context_pointer) {
    AveragedSpectrumContext *context =
        (AveragedSpectrumContext *)context_pointer;
    const unsigned int fft_size = context->plan->size;
    float *real = context->worker_real +
        (size_t)worker_index * fft_size;
    float *imaginary = context->worker_imaginary +
        (size_t)worker_index * fft_size;
    double *power_sum = context->worker_power +
        (size_t)worker_index * context->bin_count;

    for (size_t frame = begin; frame < end; ++frame) {
        const size_t frame_start =
            frame < context->regular_frame_count
                ? frame * context->hop_size
                : context->last_frame_start;
        for (unsigned int index = 0U;
             index < fft_size;
             ++index) {
            real[index] =
                context->samples[frame_start + (size_t)index] *
                context->window[index];
            imaginary[index] = 0.0f;
        }

        spectra_fft_forward(context->plan, real, imaginary);
        for (size_t bin = 0U;
             bin < context->bin_count;
             ++bin) {
            const double real_part = (double)real[bin];
            const double imaginary_part =
                (double)imaginary[bin];
            power_sum[bin] +=
                (real_part * real_part +
                 imaginary_part * imaginary_part) *
                context->power_scale;
        }
    }
}

static unsigned int reverse_bits(unsigned int value, unsigned int bits) {
    unsigned int reversed = 0;
    for (unsigned int i = 0; i < bits; i++) {
        reversed = (reversed << 1) | (value & 1U);
        value >>= 1;
    }
    return reversed;
}

void fft_radix2(float *real, float *imaginary, unsigned int size) {
    if (real == NULL || imaginary == NULL || size == 0 || (size & (size - 1U)) != 0) {
        return;
    }

    unsigned int bits = 0;
    for (unsigned int n = size; n > 1; n >>= 1) {
        bits++;
    }

    for (unsigned int i = 0; i < size; i++) {
        unsigned int j = reverse_bits(i, bits);
        if (j > i) {
            float real_temp = real[i];
            float imag_temp = imaginary[i];
            real[i] = real[j];
            imaginary[i] = imaginary[j];
            real[j] = real_temp;
            imaginary[j] = imag_temp;
        }
    }

    for (unsigned int block_size = 2; block_size <= size; block_size <<= 1) {
        unsigned int half_block = block_size / 2;
        float angle_step = -2.0f * (float)M_PI / (float)block_size;
        float step_real = cosf(angle_step);
        float step_imaginary = sinf(angle_step);

        for (unsigned int block_start = 0; block_start < size; block_start += block_size) {
            float wr = 1.0f;
            float wi = 0.0f;
            for (unsigned int k = 0; k < half_block; k++) {
                unsigned int even_index = block_start + k;
                unsigned int odd_index = even_index + half_block;

                float tr = wr * real[odd_index] - wi * imaginary[odd_index];
                float ti = wr * imaginary[odd_index] + wi * real[odd_index];

                real[odd_index] = real[even_index] - tr;
                imaginary[odd_index] = imaginary[even_index] - ti;
                real[even_index] += tr;
                imaginary[even_index] += ti;

                float next_wr = wr * step_real - wi * step_imaginary;
                wi = wr * step_imaginary + wi * step_real;
                wr = next_wr;
            }
        }
    }
}

Spectrum compute_magnitude_spectrum(const float *samples, size_t sample_count, unsigned int sample_rate) {
    Spectrum spectrum = {0};
    if (samples == NULL || sample_count == 0 || sample_rate == 0) {
        return spectrum;
    }

    unsigned int fft_size = next_power_of_two((unsigned int)sample_count);
    if (fft_size > MAX_FFT_SIZE) {
        fft_size = MAX_FFT_SIZE;
    }
    if (fft_size < 2) {
        fft_size = 2;
    }

    float *window = (float *)calloc(fft_size, sizeof(float));
    float *real = (float *)calloc(fft_size, sizeof(float));
    float *imaginary = (float *)calloc(fft_size, sizeof(float));
    if (window == NULL || real == NULL || imaginary == NULL) {
        free(window);
        free(real);
        free(imaginary);
        return spectrum;
    }

    hann_window(window, fft_size);
    float window_sum = 0.0f;
    for (unsigned int i = 0; i < fft_size; i++) {
        float sample = i < sample_count ? samples[i] : 0.0f;
        real[i] = sample * window[i];
        window_sum += window[i];
    }

    fft_radix2(real, imaginary, fft_size);

    spectrum.count = fft_size / 2;
    spectrum.frequencies = (float *)calloc(spectrum.count, sizeof(float));
    spectrum.magnitudes = (float *)calloc(spectrum.count, sizeof(float));
    if (spectrum.frequencies == NULL || spectrum.magnitudes == NULL) {
        spectrum_free(&spectrum);
        free(window);
        free(real);
        free(imaginary);
        return spectrum;
    }

    float scale = window_sum > 0.0f ? 2.0f / window_sum : 1.0f / (float)fft_size;
    for (size_t bin = 0; bin < spectrum.count; bin++) {
        spectrum.frequencies[bin] = ((float)bin * (float)sample_rate) / (float)fft_size;
        spectrum.magnitudes[bin] = sqrtf(real[bin] * real[bin] + imaginary[bin] * imaginary[bin]) * scale;
    }

    free(window);
    free(real);
    free(imaginary);
    return spectrum;
}

Spectrum compute_averaged_magnitude_spectrum(
    const float *samples,
    size_t sample_count,
    unsigned int sample_rate) {
    Spectrum spectrum = {0};
    if (samples == NULL || sample_count == 0U || sample_rate == 0U) {
        return spectrum;
    }
    if (sample_count <= MAX_FFT_SIZE) {
        return compute_magnitude_spectrum(samples, sample_count, sample_rate);
    }

    const unsigned int fft_size = MAX_FFT_SIZE;
    const size_t hop_size = (size_t)fft_size / 2U;
    const size_t last_frame_start = sample_count - (size_t)fft_size;
    const size_t regular_frame_count = last_frame_start / hop_size + 1U;
    const bool needs_end_frame =
        (regular_frame_count - 1U) * hop_size != last_frame_start;
    const size_t frame_count =
        regular_frame_count + (needs_end_frame ? 1U : 0U);

    const size_t bin_count = (size_t)fft_size / 2U;
    unsigned int worker_count =
        spectra_parallel_worker_count(
            frame_count,
            4U);
    float *window =
        (float *)calloc(fft_size, sizeof(float));
    float *worker_real = (float *)calloc(
        (size_t)worker_count * fft_size,
        sizeof(float));
    float *worker_imaginary = (float *)calloc(
        (size_t)worker_count * fft_size,
        sizeof(float));
    double *worker_power = (double *)calloc(
        (size_t)worker_count * bin_count,
        sizeof(double));
    SpectraFftPlan plan = {0};
    if (window == NULL || worker_real == NULL ||
        worker_imaginary == NULL || worker_power == NULL ||
        !spectra_fft_plan_init(&plan, fft_size)) {
        free(window);
        free(worker_real);
        free(worker_imaginary);
        free(worker_power);
        spectra_fft_plan_free(&plan);
        return spectrum;
    }

    hann_window(window, fft_size);
    float window_sum = 0.0f;
    for (unsigned int index = 0U; index < fft_size; ++index) {
        window_sum += window[index];
    }
    const float scale =
        window_sum > 0.0f
            ? 2.0f / window_sum
            : 1.0f / (float)fft_size;
    const double power_scale = (double)scale * (double)scale;

    AveragedSpectrumContext context = {
        .samples = samples,
        .window = window,
        .plan = &plan,
        .worker_real = worker_real,
        .worker_imaginary = worker_imaginary,
        .worker_power = worker_power,
        .regular_frame_count = regular_frame_count,
        .last_frame_start = last_frame_start,
        .hop_size = hop_size,
        .bin_count = bin_count,
        .power_scale = power_scale,
    };
    spectra_parallel_for_limited(
        frame_count,
        4U,
        worker_count,
        accumulate_spectrum_frames,
        &context);

    spectrum.count = (size_t)fft_size / 2U;
    spectrum.frequencies =
        (float *)calloc(spectrum.count, sizeof(float));
    spectrum.magnitudes =
        (float *)calloc(spectrum.count, sizeof(float));
    if (spectrum.frequencies == NULL || spectrum.magnitudes == NULL) {
        spectrum_free(&spectrum);
        free(window);
        free(worker_real);
        free(worker_imaginary);
        free(worker_power);
        spectra_fft_plan_free(&plan);
        return spectrum;
    }

    for (size_t bin = 0U; bin < spectrum.count; ++bin) {
        double power_sum = 0.0;
        for (unsigned int worker = 0U;
             worker < worker_count;
             ++worker) {
            power_sum += worker_power[
                (size_t)worker * bin_count + bin];
        }
        spectrum.frequencies[bin] =
            ((float)bin * (float)sample_rate) / (float)fft_size;
        spectrum.magnitudes[bin] =
            sqrtf((float)(power_sum / (double)frame_count));
    }

    free(window);
    free(worker_real);
    free(worker_imaginary);
    free(worker_power);
    spectra_fft_plan_free(&plan);
    return spectrum;
}
