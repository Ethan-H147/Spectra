#include "dsp/dsp_types.h"
#include "dsp/fft.h"
#include "dsp/spectral_effects.h"
#include "platform/monotonic_clock.h"
#include "platform/parallel_for.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool continue_processing(float progress, void *context) {
    (void)progress;
    (void)context;
    return true;
}

static void fill_test_audio(
    InterleavedBuffer *buffer,
    float *mono,
    double seconds) {
    const unsigned int sample_rate = 44100U;
    const unsigned int channels = 2U;
    const size_t frames =
        (size_t)(seconds * (double)sample_rate);
    buffer->samples = (float *)calloc(
        frames * (size_t)channels,
        sizeof(float));
    buffer->frame_count = frames;
    buffer->sample_rate = sample_rate;
    buffer->channel_count = channels;
    if (buffer->samples == NULL) {
        return;
    }

    for (size_t frame = 0U; frame < frames; ++frame) {
        const double time =
            (double)frame / (double)sample_rate;
        const float sample =
            0.18f * (float)sin(2.0 * M_PI * 110.0 * time) +
            0.12f * (float)sin(2.0 * M_PI * 440.0 * time) +
            0.08f * (float)sin(2.0 * M_PI * 1760.0 * time);
        buffer->samples[frame * 2U] = sample;
        buffer->samples[frame * 2U + 1U] = sample * 0.92f;
        if (mono != NULL) {
            mono[frame] = sample;
        }
    }
}

static void print_result(
    const char *label,
    double started,
    bool succeeded) {
    const double elapsed =
        platform_monotonic_seconds() - started;
    printf(
        "%-28s %8.3f s  %s\n",
        label,
        elapsed,
        succeeded ? "ok" : "failed");
}

int main(int argument_count, char **arguments) {
    const bool single_threaded =
        argument_count > 1 &&
        strcmp(arguments[1], "--single") == 0;
    spectra_set_parallel_thread_limit(
        single_threaded ? 1U : 0U);

    const double duration = 30.0;
    InterleavedBuffer source = {0};
    float *mono = (float *)calloc(
        (size_t)(duration * 44100.0),
        sizeof(float));
    fill_test_audio(&source, mono, duration);
    if (source.samples == NULL || mono == NULL) {
        free(mono);
        interleaved_buffer_free(&source);
        fprintf(stderr, "Could not allocate benchmark audio\n");
        return 1;
    }

    printf(
        "Spectra DSP benchmark: %.0f s stereo at %u Hz (%u worker%s)\n",
        duration,
        source.sample_rate,
        spectra_effective_worker_count(),
        spectra_effective_worker_count() == 1U ? "" : "s");

    double started = platform_monotonic_seconds();
    Spectrum spectrum = compute_averaged_magnitude_spectrum(
        mono,
        source.frame_count,
        source.sample_rate);
    print_result(
        "Full-track spectrum",
        started,
        spectrum.count > 0U);
    spectrum_free(&spectrum);

    InterleavedBuffer output = {0};
    started = platform_monotonic_seconds();
    bool succeeded = playback_rate_pitch_shift(
        &source,
        1.25f,
        &output,
        continue_processing,
        NULL);
    print_result("Tape speed", started, succeeded);
    interleaved_buffer_free(&output);

    const SpectralEqBand bands[] = {
        {20.0f, 250.0f, 4.0f, true,
         SPECTRAL_EQ_SHAPE_RANGE},
        {1800.0f, 4800.0f, 2.0f, true,
         SPECTRAL_EQ_SHAPE_BELL},
        {7000.0f, 15000.0f, -1.5f, true,
         SPECTRAL_EQ_SHAPE_HIGH_SHELF},
    };
    started = platform_monotonic_seconds();
    succeeded = spectral_range_equalize(
        &source,
        bands,
        sizeof(bands) / sizeof(bands[0]),
        2048U,
        512U,
        &output,
        continue_processing,
        NULL);
    print_result("Range EQ", started, succeeded);
    interleaved_buffer_free(&output);

    started = platform_monotonic_seconds();
    succeeded = analytic_frequency_shift(
        &source,
        250.0f,
        2048U,
        512U,
        &output,
        continue_processing,
        NULL);
    print_result("Frequency shift", started, succeeded);
    interleaved_buffer_free(&output);

    started = platform_monotonic_seconds();
    succeeded = phase_vocoder_pitch_shift(
        &source,
        1.25f,
        2048U,
        512U,
        &output,
        continue_processing,
        NULL);
    print_result("Pitch shift", started, succeeded);
    interleaved_buffer_free(&output);

    free(mono);
    interleaved_buffer_free(&source);
    return 0;
}
