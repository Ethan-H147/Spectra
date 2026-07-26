#include "dsp/fourier_reconstruction.h"

#include "dsp/fft.h"
#include "dsp/signal_utils.h"
#include "dsp/windowing.h"

#include <math.h>
#include <stdlib.h>

#define RECONSTRUCTION_MAX_FFT_SIZE 16384U

static unsigned int previous_power_of_two(size_t value) {
    unsigned int result = 1U;
    while (result <= RECONSTRUCTION_MAX_FFT_SIZE / 2U &&
           (size_t)(result << 1U) <= value) {
        result <<= 1U;
    }
    return result;
}

static int compare_components(const void *left_value, const void *right_value) {
    const FourierComponent *left = (const FourierComponent *)left_value;
    const FourierComponent *right = (const FourierComponent *)right_value;
    if (left->magnitude < right->magnitude) return 1;
    if (left->magnitude > right->magnitude) return -1;
    if (left->frequency > right->frequency) return 1;
    if (left->frequency < right->frequency) return -1;
    return 0;
}

void fourier_frame_analysis_init(FourierFrameAnalysis *analysis) {
    if (analysis != NULL) {
        *analysis = (FourierFrameAnalysis){0};
    }
}

void fourier_frame_analysis_free(FourierFrameAnalysis *analysis) {
    if (analysis == NULL) {
        return;
    }
    sample_buffer_free(&analysis->windowed_frame);
    free(analysis->ranked_components);
    *analysis = (FourierFrameAnalysis){0};
}

bool analyze_fourier_frame(const SampleBuffer *region, FourierFrameAnalysis *analysis) {
    if (region == NULL || region->samples == NULL || region->count < 8U ||
        region->sample_rate == 0U || analysis == NULL) {
        return false;
    }

    unsigned int fft_size = previous_power_of_two(region->count);
    if (fft_size < 8U) {
        return false;
    }
    size_t start = (region->count - (size_t)fft_size) / 2U;

    float *window = (float *)calloc(fft_size, sizeof(float));
    float *real = (float *)calloc(fft_size, sizeof(float));
    float *imaginary = (float *)calloc(fft_size, sizeof(float));
    FourierComponent *components =
        (FourierComponent *)calloc((size_t)fft_size / 2U - 1U, sizeof(FourierComponent));
    float *original = (float *)calloc(fft_size, sizeof(float));
    if (window == NULL || real == NULL || imaginary == NULL || components == NULL || original == NULL) {
        free(window);
        free(real);
        free(imaginary);
        free(components);
        free(original);
        return false;
    }

    hann_window(window, fft_size);
    for (unsigned int index = 0U; index < fft_size; index++) {
        original[index] = region->samples[start + index] * window[index];
        real[index] = original[index];
    }
    fft_radix2(real, imaginary, fft_size);

    size_t component_count = (size_t)fft_size / 2U - 1U;
    for (size_t index = 0U; index < component_count; index++) {
        unsigned int bin = (unsigned int)index + 1U;
        float coefficient_magnitude = hypotf(real[bin], imaginary[bin]);
        float signal_magnitude = 2.0f * coefficient_magnitude / (float)fft_size;
        components[index] = (FourierComponent){
            .bin_index = bin,
            .frequency = (float)bin * (float)region->sample_rate / (float)fft_size,
            .magnitude = signal_magnitude,
            .db = linear_to_db(signal_magnitude),
            .phase = atan2f(imaginary[bin], real[bin]),
            .real = real[bin],
            .imaginary = imaginary[bin],
        };
    }
    qsort(components, component_count, sizeof(FourierComponent), compare_components);

    FourierFrameAnalysis completed = {
        .windowed_frame = {
            .samples = original,
            .count = fft_size,
            .sample_rate = region->sample_rate,
        },
        .ranked_components = components,
        .component_count = component_count,
        .fft_size = fft_size,
        .dc_real = real[0],
        .nyquist_real = real[fft_size / 2U],
    };

    free(window);
    free(real);
    free(imaginary);
    fourier_frame_analysis_free(analysis);
    *analysis = completed;
    return true;
}

SampleBuffer reconstruct_fourier_frame(const FourierFrameAnalysis *analysis, size_t component_count) {
    SampleBuffer result = {0};
    if (analysis == NULL || analysis->ranked_components == NULL || analysis->fft_size < 8U ||
        analysis->windowed_frame.sample_rate == 0U) {
        return result;
    }

    if (component_count > analysis->component_count) {
        component_count = analysis->component_count;
    }
    unsigned int fft_size = analysis->fft_size;
    float *real = (float *)calloc(fft_size, sizeof(float));
    float *imaginary = (float *)calloc(fft_size, sizeof(float));
    if (real == NULL || imaginary == NULL) {
        free(real);
        free(imaginary);
        return result;
    }

    real[0] = analysis->dc_real;
    real[fft_size / 2U] = analysis->nyquist_real;
    for (size_t index = 0U; index < component_count; index++) {
        const FourierComponent *component = &analysis->ranked_components[index];
        unsigned int bin = component->bin_index;
        real[bin] = component->real;
        imaginary[bin] = component->imaginary;
        real[fft_size - bin] = component->real;
        imaginary[fft_size - bin] = -component->imaginary;
    }

    for (unsigned int index = 0U; index < fft_size; index++) {
        imaginary[index] = -imaginary[index];
    }
    fft_radix2(real, imaginary, fft_size);
    for (unsigned int index = 0U; index < fft_size; index++) {
        real[index] /= (float)fft_size;
    }
    free(imaginary);

    result.samples = real;
    result.count = fft_size;
    result.sample_rate = analysis->windowed_frame.sample_rate;
    return result;
}
