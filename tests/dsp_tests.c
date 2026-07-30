#include "dsp/additive_synth.h"
#include "dsp/channel_mix.h"
#include "dsp/fft.h"
#include "dsp/fourier_reconstruction.h"
#include "dsp/global_fourier_reconstruction.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/harmonic_resynthesis.h"
#include "dsp/pitch_detection.h"
#include "dsp/signal_utils.h"
#include "dsp/stft_reconstruction.h"
#include "dsp/windowing.h"
#include "dsp/waveform_summary.h"

#include <math.h>
#include <stdio.h>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int test_sine_length(void) {
    SampleBuffer sine = generate_sine_wave(440.0f, 1.0f, 0.5f, 44100);
    ASSERT_TRUE(sine.count == 22050, "sine wave length should match duration * sample rate");
    ASSERT_TRUE(fabsf(sine.samples[0]) < 0.0001f, "sine should start near zero");
    sample_buffer_free(&sine);
    return 0;
}

static int test_hann_window(void) {
    float window[5] = {0};
    hann_window(window, 5);
    ASSERT_TRUE(fabsf(window[0]) < 0.0001f, "Hann first sample should be zero");
    ASSERT_TRUE(fabsf(window[2] - 1.0f) < 0.0001f, "Hann center sample should be one for size 5");
    ASSERT_TRUE(fabsf(window[4]) < 0.0001f, "Hann last sample should be zero");
    return 0;
}

static int test_additive_normalization(void) {
    Harmonic harmonics[3] = {
        {1, 1.0f, 0.0f},
        {2, 1.0f, 0.0f},
        {3, 1.0f, 0.0f},
    };
    ADSREnvelope envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    SampleBuffer tone = generate_additive_tone(110.0f, harmonics, 3, 1.0f, 44100, envelope);
    ASSERT_TRUE(peak_amplitude(tone.samples, tone.count) <= 0.9501f, "additive tone should be normalized");
    sample_buffer_free(&tone);
    return 0;
}

static int test_peak_detection(void) {
    float frequencies[] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
    float magnitudes[] = {0.01f, 0.2f, 1.0f, 0.2f, 0.01f};
    Spectrum spectrum = {frequencies, magnitudes, 5};
    Peak peaks[4] = {0};
    int count = find_peaks(&spectrum, 50.0f, 600.0f, -20.0f, peaks, 4);
    ASSERT_TRUE(count == 1, "synthetic spectrum should have one peak");
    ASSERT_TRUE(fabsf(peaks[0].frequency - 300.0f) < 0.01f, "peak should be at 300 Hz");
    return 0;
}

static int test_spectrum_for_a4(void) {
    SampleBuffer sine = generate_sine_wave(440.0f, 1.0f, 1.0f, 44100);
    Spectrum spectrum = compute_magnitude_spectrum(sine.samples, sine.count, sine.sample_rate);
    Peak bin_peaks[4] = {0};
    Peak precise_peaks[4] = {0};
    int bin_count =
        find_peaks(&spectrum, 50.0f, 1000.0f, -60.0f, bin_peaks, 4);
    int precise_count = find_interpolated_peaks(
        &spectrum, 50.0f, 1000.0f, -60.0f, precise_peaks, 4);
    ASSERT_TRUE(bin_count >= 1 && precise_count == bin_count,
                "440 Hz sine should produce matching raw and precise peaks");
    ASSERT_TRUE(fabsf(bin_peaks[0].frequency - 440.0f) < 4.0f,
                "strongest A4 FFT bin should be near 440 Hz");
    ASSERT_TRUE(fabsf(precise_peaks[0].frequency - 440.0f) < 0.15f,
                "interpolated A4 peak should closely estimate 440 Hz");
    ASSERT_TRUE(fabsf(precise_peaks[0].frequency - 440.0f) <
                    fabsf(bin_peaks[0].frequency - 440.0f),
                "interpolation should improve on the raw FFT-bin center");
    spectrum_free(&spectrum);
    sample_buffer_free(&sine);
    return 0;
}

static int test_pitch_for_a4(void) {
    SampleBuffer sine = generate_sine_wave(440.0f, 0.8f, 0.5f, 44100);
    PitchEstimate pitch = estimate_pitch(&sine, 40.0f, 1200.0f);
    ASSERT_TRUE(pitch.valid, "440 Hz sine should produce a valid pitch estimate");
    ASSERT_TRUE(fabsf(pitch.frequency_hz - 440.0f) < 1.0f, "estimated A4 frequency should be near 440 Hz");
    ASSERT_TRUE(pitch.midi_note == 69, "440 Hz should map to MIDI note 69");
    ASSERT_TRUE(pitch_note_name(pitch.midi_note)[0] == 'A', "MIDI note 69 should be named A");
    ASSERT_TRUE(pitch_note_octave(pitch.midi_note) == 4, "MIDI note 69 should be in octave 4");
    ASSERT_TRUE(fabsf(pitch.cents) < 4.0f, "440 Hz should be close to zero cents from A4");
    ASSERT_TRUE(pitch.confidence > 0.90f, "clean sine pitch confidence should be high");
    sample_buffer_free(&sine);
    return 0;
}

static int test_pitch_for_harmonic_tone(void) {
    Harmonic harmonics[] = {
        {2, 0.80f, 0.0f},
        {3, 0.50f, 0.0f},
        {4, 0.30f, 0.0f},
    };
    ADSREnvelope envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    SampleBuffer tone = generate_additive_tone(110.0f, harmonics, 3, 0.5f, 44100, envelope);
    PitchEstimate pitch = estimate_pitch(&tone, 40.0f, 1200.0f);
    ASSERT_TRUE(pitch.valid, "harmonic tone should produce a valid pitch estimate");
    ASSERT_TRUE(fabsf(pitch.frequency_hz - 110.0f) < 1.0f,
                "estimator should recover a missing 110 Hz fundamental from its harmonics");
    ASSERT_TRUE(pitch.confidence > 0.80f, "periodic harmonic tone pitch confidence should be high");
    sample_buffer_free(&tone);
    return 0;
}

static int test_pitch_rejects_silence(void) {
    float samples[4096] = {0};
    SampleBuffer silence = {samples, 4096, 44100};
    PitchEstimate pitch = estimate_pitch(&silence, 40.0f, 1200.0f);
    ASSERT_TRUE(!pitch.valid, "silence should not produce a pitch estimate");
    return 0;
}

static int test_stereo_downmix(void) {
    const float stereo[] = {1.0f, -1.0f, 0.5f, 0.5f, -0.25f, 0.75f};
    float mono[3] = {0};
    ASSERT_TRUE(downmix_interleaved_to_mono(stereo, 3, 2, mono), "stereo downmix should succeed");
    ASSERT_TRUE(fabsf(mono[0]) < 0.0001f, "opposite stereo channels should average to zero");
    ASSERT_TRUE(fabsf(mono[1] - 0.5f) < 0.0001f, "equal stereo channels should preserve their value");
    ASSERT_TRUE(fabsf(mono[2] - 0.25f) < 0.0001f, "stereo channels should use their arithmetic mean");

    float left_samples[] = {0.1f, 0.2f, 0.3f};
    float right_samples[] = {-0.1f, -0.2f, -0.3f};
    SampleBuffer channels[] = {
        {left_samples, 3U, 48000U},
        {right_samples, 3U, 48000U},
    };
    InterleavedBuffer rebuilt = {0};
    ASSERT_TRUE(interleave_sample_buffers(channels, 2U, &rebuilt),
                "two matching mono buffers should interleave");
    ASSERT_TRUE(rebuilt.channel_count == 2U &&
                    rebuilt.frame_count == 3U &&
                    fabsf(rebuilt.samples[2] - 0.2f) < 0.0001f &&
                    fabsf(rebuilt.samples[3] + 0.2f) < 0.0001f,
                "interleaving should preserve left/right frame order");
    interleaved_buffer_free(&rebuilt);
    return 0;
}

static int test_waveform_summary(void) {
    const float samples[] = {-1.0f, -0.5f, 0.2f, 0.8f, -0.2f, 0.4f};
    float minimums[3] = {0};
    float maximums[3] = {0};
    size_t count = summarize_waveform(samples, 6, minimums, maximums, 3);
    ASSERT_TRUE(count == 3, "waveform summary should fill the requested three bins");
    ASSERT_TRUE(fabsf(minimums[0] + 1.0f) < 0.0001f && fabsf(maximums[0] + 0.5f) < 0.0001f,
                "first waveform bin should preserve its minimum and maximum");
    ASSERT_TRUE(fabsf(minimums[1] - 0.2f) < 0.0001f && fabsf(maximums[1] - 0.8f) < 0.0001f,
                "second waveform bin should preserve its minimum and maximum");
    ASSERT_TRUE(fabsf(minimums[2] + 0.2f) < 0.0001f && fabsf(maximums[2] - 0.4f) < 0.0001f,
                "third waveform bin should preserve its minimum and maximum");
    return 0;
}

static int test_harmonic_extraction(void) {
    float frequencies[501] = {0};
    float magnitudes[501] = {0};
    for (int index = 0; index < 501; index++) {
        frequencies[index] = (float)index * 2.0f;
        magnitudes[index] = 0.00001f;
    }
    magnitudes[50] = 0.8f;
    magnitudes[101] = 0.4f;
    magnitudes[150] = 0.2f;
    Spectrum spectrum = {frequencies, magnitudes, 501};
    ExtractedHarmonic harmonics[4] = {0};

    int count = extract_harmonics(&spectrum, 100.0f, 4, 30.0f, -70.0f, harmonics, 4);
    ASSERT_TRUE(count == 4, "harmonic extraction should return every in-range harmonic row");
    ASSERT_TRUE(harmonics[0].detected && harmonics[1].detected && harmonics[2].detected,
                "first three synthetic harmonics should be detected");
    ASSERT_TRUE(!harmonics[3].detected, "missing fourth harmonic should remain explicitly undetected");
    ASSERT_TRUE(fabsf(harmonics[0].amplitude - 1.0f) < 0.0001f,
                "strongest harmonic should normalize to amplitude one");
    ASSERT_TRUE(fabsf(harmonics[1].amplitude - 0.5f) < 0.01f,
                "second harmonic should retain its relative amplitude");
    ASSERT_TRUE(fabsf(harmonics[1].detected_frequency - 202.0f) < 0.1f,
                "harmonic extraction should report the measured peak frequency");
    return 0;
}

static int test_harmonic_resynthesis(void) {
    ExtractedHarmonic harmonics[3] = {
        {1, 110.0f, 110.0f, 1.0f, 1.0f, 0.0f, true},
        {2, 220.0f, 220.0f, 0.5f, 0.5f, -6.02f, true},
        {3, 330.0f, 0.0f, 0.0f, 0.0f, -120.0f, false},
    };
    SampleBuffer rebuilt = resynthesize_from_harmonics(110.0f, harmonics, 3, 0.5f, 44100);
    ASSERT_TRUE(rebuilt.samples != NULL && rebuilt.count == 22050,
                "harmonic resynthesis should create the requested buffer");
    ASSERT_TRUE(peak_amplitude(rebuilt.samples, rebuilt.count) <= 0.9501f,
                "harmonic resynthesis should preserve the additive synth safety ceiling");
    sample_buffer_free(&rebuilt);
    return 0;
}

static float root_mean_square_error(const SampleBuffer *left, const SampleBuffer *right) {
    if (left == NULL || right == NULL || left->samples == NULL || right->samples == NULL ||
        left->count != right->count || left->count == 0U) {
        return INFINITY;
    }
    double sum = 0.0;
    for (size_t index = 0U; index < left->count; index++) {
        double difference = (double)left->samples[index] - (double)right->samples[index];
        sum += difference * difference;
    }
    return sqrtf((float)(sum / (double)left->count));
}

static int test_fourier_component_reconstruction(void) {
    const unsigned int sample_rate = 1024U;
    const size_t count = 1024U;
    float samples[1024] = {0};
    for (size_t index = 0U; index < count; index++) {
        float time = (float)index / (float)sample_rate;
        samples[index] = 0.7f * sinf(2.0f * 3.14159265358979323846f * 32.0f * time) +
                         0.3f * sinf(2.0f * 3.14159265358979323846f * 96.0f * time + 0.4f) +
                         0.1f * sinf(2.0f * 3.14159265358979323846f * 200.0f * time - 0.7f);
    }
    SampleBuffer source = {samples, count, sample_rate};
    FourierFrameAnalysis analysis;
    fourier_frame_analysis_init(&analysis);
    ASSERT_TRUE(analyze_fourier_frame(&source, &analysis), "Fourier frame analysis should succeed");
    ASSERT_TRUE(analysis.component_count == 511U, "1024-point frame should expose 511 positive-frequency bins");
    ASSERT_TRUE(analysis.ranked_components[0].magnitude >= analysis.ranked_components[1].magnitude,
                "Fourier components should be ranked strongest first");

    SampleBuffer top_one = reconstruct_fourier_frame(&analysis, 1U);
    SampleBuffer top_forty = reconstruct_fourier_frame(&analysis, 40U);
    SampleBuffer complete = reconstruct_fourier_frame(&analysis, analysis.component_count);
    float top_one_error = root_mean_square_error(&analysis.windowed_frame, &top_one);
    float top_forty_error = root_mean_square_error(&analysis.windowed_frame, &top_forty);
    float complete_error = root_mean_square_error(&analysis.windowed_frame, &complete);
    ASSERT_TRUE(top_forty_error < top_one_error,
                "adding stronger Fourier components should improve reconstruction error");
    ASSERT_TRUE(complete_error < 0.00001f,
                "all Fourier components should reconstruct the windowed frame numerically");

    sample_buffer_free(&top_one);
    sample_buffer_free(&top_forty);
    sample_buffer_free(&complete);
    fourier_frame_analysis_free(&analysis);
    return 0;
}

static bool finish_global_fourier_job(GlobalFourierJob *job,
                                      size_t operations_per_step) {
    size_t guard = 0U;
    uint64_t expected_steps =
        job != NULL && operations_per_step > 0U
            ? job->total_work / operations_per_step + 4U
            : 0U;
    while (job != NULL && job->active && (uint64_t)guard < expected_steps) {
        global_fourier_job_process(job, operations_per_step);
        guard++;
    }
    return job != NULL && !job->active;
}

static int test_global_fourier_fixed_components(void) {
    const unsigned int sample_rate = 1024U;
    const size_t count = 1024U;
    float samples[1024] = {0};
    for (size_t index = 0U; index < count; index++) {
        float time = (float)index / (float)sample_rate;
        samples[index] =
            0.70f * sinf(2.0f * 3.14159265358979323846f * 32.0f * time) +
            0.30f * sinf(2.0f * 3.14159265358979323846f * 96.0f * time + 0.4f) +
            0.10f * sinf(2.0f * 3.14159265358979323846f * 200.0f * time - 0.7f);
    }
    SampleBuffer source = {samples, count, sample_rate};
    GlobalFourierJob limited_job;
    global_fourier_job_init(&limited_job);
    ASSERT_TRUE(
        global_fourier_job_begin_analysis(
            &limited_job,
            &source,
            3),
        "limited whole-file analysis should initialize");
    ASSERT_TRUE(
        finish_global_fourier_job(
            &limited_job,
            31U) &&
            limited_job.analysis_ready &&
            limited_job.component_count == 3,
        "limited whole-file analysis should retain only its requested strongest bins");
    ASSERT_TRUE(
        limited_job.components[0].bin == 32U &&
            limited_job.components[1].bin == 96U &&
            limited_job.components[2].bin == 200U,
        "limited whole-file analysis should preserve the strongest-bin ranking");
    ASSERT_TRUE(
        global_fourier_job_begin_reconstruction(
            &limited_job,
            3) &&
            finish_global_fourier_job(
                &limited_job,
                29U),
        "limited whole-file analysis should reconstruct from its retained bins");
    SampleBuffer limited_output =
        global_fourier_job_take_output(
            &limited_job);
    ASSERT_TRUE(
        root_mean_square_error(
            &source,
            &limited_output) < 0.00002f,
        "limited strongest-bin analysis should reconstruct the known three-tone source");
    sample_buffer_free(&limited_output);
    global_fourier_job_free(&limited_job);

    GlobalFourierJob job;
    global_fourier_job_init(&job);
    ASSERT_TRUE(global_fourier_available_component_count(count) == 513,
                "1024 real samples should have 513 independent FFT bins including DC and Nyquist");
    ASSERT_TRUE(global_fourier_job_begin_analysis(&job, &source, 1000),
                "whole-file Fourier analysis should initialize");
    ASSERT_TRUE(global_fourier_job_process(&job, 17U) == 17U &&
                    job.active &&
                    global_fourier_job_progress(&job) > 0.0f &&
                    global_fourier_job_progress(&job) < 1.0f,
                "whole-file Fourier analysis should advance incrementally");
    ASSERT_TRUE(finish_global_fourier_job(&job, 31U) && job.analysis_ready,
                "whole-file Fourier analysis should complete");
    ASSERT_TRUE(job.component_count == 513,
                "whole-file analysis should retain the complete independent real-signal spectrum");
    ASSERT_TRUE(job.components[0].bin == 32U &&
                    job.components[1].bin == 96U &&
                    job.components[2].bin == 200U,
                "global components should be ranked once for the complete recording");
    ASSERT_TRUE(global_fourier_job_retained_energy(&job, 3) >
                    global_fourier_job_retained_energy(&job, 1),
                "additional fixed components should retain more global energy");
    int eighty_percent_components =
        global_fourier_job_component_count_for_energy(
            &job, 0.80f);
    int ninety_five_percent_components =
        global_fourier_job_component_count_for_energy(
            &job, 0.95f);
    ASSERT_TRUE(eighty_percent_components == 1,
                "the strongest known sinusoid should exceed an 80 percent energy target");
    ASSERT_TRUE(ninety_five_percent_components == 2,
                "the two strongest known sinusoids should be the smallest set exceeding 95 percent energy");
    ASSERT_TRUE(
        global_fourier_job_retained_energy(
            &job, ninety_five_percent_components) >=
            0.95f,
        "an energy-target lookup should meet or exceed its requested retained energy");
    ASSERT_TRUE(
        global_fourier_job_component_count_for_energy(
            &job, 1.0f) == job.component_count,
        "a 100 percent energy target should select every padded FFT bin");

    ASSERT_TRUE(global_fourier_job_begin_reconstruction(&job, 1),
                "Top-1 global reconstruction should initialize");
    ASSERT_TRUE(finish_global_fourier_job(&job, 29U) &&
                    job.reconstruction_ready,
                "Top-1 global reconstruction should complete");
    SampleBuffer top_one = global_fourier_job_take_output(&job);
    ASSERT_TRUE(top_one.samples != NULL && top_one.count == source.count,
                "Top-1 global reconstruction should preserve source length");

    ASSERT_TRUE(global_fourier_job_begin_reconstruction(&job, 3),
                "Top-3 global reconstruction should initialize");
    ASSERT_TRUE(finish_global_fourier_job(&job, 29U) &&
                    job.reconstruction_ready,
                "Top-3 global reconstruction should complete");
    SampleBuffer top_three = global_fourier_job_take_output(&job);
    float top_one_error = root_mean_square_error(&source, &top_one);
    float top_three_error = root_mean_square_error(&source, &top_three);
    ASSERT_TRUE(top_three_error < top_one_error,
                "adding fixed global frequencies should improve reconstruction error");
    ASSERT_TRUE(top_three_error < 0.00002f,
                "three known sinusoids should reconstruct numerically from three fixed bins");

    ASSERT_TRUE(global_fourier_job_begin_reconstruction(&job, 37),
                "an arbitrary global component count should initialize");
    ASSERT_TRUE(finish_global_fourier_job(&job, 29U) &&
                    job.rendered_component_count == 37,
                "global reconstruction should preserve an exact custom component count");
    SampleBuffer top_thirty_seven =
        global_fourier_job_take_output(&job);
    ASSERT_TRUE(top_thirty_seven.samples != NULL,
                "custom-count global reconstruction should produce audio");

    ASSERT_TRUE(global_fourier_job_begin_reconstruction(
                    &job, job.component_count),
                "all-bin global reconstruction should initialize");
    ASSERT_TRUE(finish_global_fourier_job(&job, 29U) &&
                    job.rendered_component_count == 513,
                "all independent global bins should reconstruct incrementally");
    SampleBuffer complete = global_fourier_job_take_output(&job);
    ASSERT_TRUE(root_mean_square_error(&source, &complete) < 0.00002f,
                "all independent global bins should reconstruct the source numerically");

    sample_buffer_free(&top_one);
    sample_buffer_free(&top_three);
    sample_buffer_free(&top_thirty_seven);
    sample_buffer_free(&complete);
    global_fourier_job_free(&job);
    return 0;
}

static int test_global_fourier_dc_and_nyquist(void) {
    const unsigned int sample_rate = 64U;
    const size_t count = 64U;
    float samples[64] = {0};
    for (size_t index = 0U; index < count; index++) {
        float time = (float)index / (float)sample_rate;
        float nyquist = index % 2U == 0U ? 0.20f : -0.20f;
        samples[index] =
            0.15f + nyquist +
            0.50f * sinf(2.0f * 3.14159265358979323846f * 5.0f * time);
    }

    SampleBuffer source = {samples, count, sample_rate};
    GlobalFourierJob job;
    global_fourier_job_init(&job);
    int available = global_fourier_available_component_count(count);
    ASSERT_TRUE(available == 33,
                "64 real samples should expose 33 independent bins");
    ASSERT_TRUE(global_fourier_job_begin_analysis(
                    &job, &source, available),
                "full-range DC and Nyquist analysis should initialize");
    ASSERT_TRUE(finish_global_fourier_job(&job, 11U) &&
                    job.component_count == available,
                "full-range DC and Nyquist analysis should complete");
    ASSERT_TRUE(global_fourier_job_begin_reconstruction(
                    &job, available),
                "full-range DC and Nyquist reconstruction should initialize");
    ASSERT_TRUE(finish_global_fourier_job(&job, 11U),
                "full-range DC and Nyquist reconstruction should complete");

    SampleBuffer complete = global_fourier_job_take_output(&job);
    ASSERT_TRUE(root_mean_square_error(&source, &complete) < 0.00001f,
                "DC, Nyquist, and conjugate-paired bins should reconstruct exactly");
    sample_buffer_free(&complete);
    global_fourier_job_free(&job);
    return 0;
}

static bool finish_stft_job(StftReconstructionJob *job,
                            size_t frames_per_step);

static int test_stft_spectrogram_only(void) {
    const unsigned int sample_rate = 2048U;
    const size_t count = 2048U;
    float samples[2048] = {0};
    for (size_t index = 0U; index < count; index++) {
        float time = (float)index / (float)sample_rate;
        samples[index] =
            0.5f * sinf(2.0f * 3.14159265358979323846f * 220.0f * time);
    }
    SampleBuffer source = {samples, count, sample_rate};
    StftReconstructionJob job;
    stft_reconstruction_job_init(&job);
    ASSERT_TRUE(stft_reconstruction_job_begin(
                    &job, &source, 512U, 128U, 0, true, 24U, 16U, 900.0f),
                "visualization-only STFT should initialize without reconstruction buffers");
    ASSERT_TRUE(job.output.samples == NULL && job.ranked_bins == NULL &&
                    job.overlap_weights == NULL,
                "visualization-only STFT should not allocate reconstruction state");
    ASSERT_TRUE(finish_stft_job(&job, 8U),
                "visualization-only STFT should complete");
    SpectrogramData spectrogram =
        stft_reconstruction_job_take_spectrogram(&job);
    ASSERT_TRUE(spectrogram.db_values != NULL &&
                    spectrogram.frequency_bins == 16U,
                "visualization-only STFT should still produce spectrogram data");
    spectrogram_data_free(&spectrogram);
    stft_reconstruction_job_free(&job);
    return 0;
}

static bool finish_stft_job(StftReconstructionJob *job, size_t frames_per_step) {
    size_t guard = 0U;
    while (job != NULL && job->active && guard < job->frame_count + 1U) {
        stft_reconstruction_job_process(job, frames_per_step);
        guard++;
    }
    return job != NULL && job->complete && !job->failed;
}

static int test_stft_incremental_reconstruction(void) {
    const unsigned int sample_rate = 4096U;
    const size_t count = 5000U;
    float samples[5000] = {0};
    for (size_t index = 0U; index < count; index++) {
        float time = (float)index / (float)sample_rate;
        samples[index] =
            0.55f * sinf(2.0f * 3.14159265358979323846f * 123.0f * time + 0.1f) +
            0.25f * sinf(2.0f * 3.14159265358979323846f * 431.0f * time - 0.4f) +
            0.10f * sinf(2.0f * 3.14159265358979323846f * 811.0f * time + 0.8f);
    }
    SampleBuffer source = {samples, count, sample_rate};
    StftReconstructionJob job;
    stft_reconstruction_job_init(&job);
    ASSERT_TRUE(stft_reconstruction_job_begin(
                    &job, &source, 1024U, 256U, 511, true, 32U, 24U, 1800.0f),
                "full-bin STFT job should initialize");
    ASSERT_TRUE(job.active && job.next_frame == 0U, "new STFT job should begin at its first frame");
    ASSERT_TRUE(stft_reconstruction_job_process(&job, 3U) == 3U,
                "incremental STFT work should honor its frame budget");
    ASSERT_TRUE(job.active && job.next_frame == 3U,
                "incremental STFT work should preserve progress between calls");
    ASSERT_TRUE(stft_reconstruction_job_progress(&job) > 0.0f &&
                    stft_reconstruction_job_progress(&job) < 1.0f,
                "partial STFT work should report fractional progress");
    ASSERT_TRUE(finish_stft_job(&job, 4U), "incremental STFT job should complete");
    ASSERT_TRUE(fabsf(stft_reconstruction_job_progress(&job) - 1.0f) < 0.0001f,
                "completed STFT job should report full progress");

    SampleBuffer rebuilt = stft_reconstruction_job_take_output(&job);
    SpectrogramData spectrogram = stft_reconstruction_job_take_spectrogram(&job);
    ASSERT_TRUE(rebuilt.samples != NULL && rebuilt.count == source.count,
                "STFT reconstruction should preserve the full source length");
    ASSERT_TRUE(root_mean_square_error(&source, &rebuilt) < 0.00002f,
                "all STFT bins should reconstruct the full source numerically");
    ASSERT_TRUE(spectrogram.db_values != NULL && spectrogram.time_bins == 21U &&
                    spectrogram.frequency_bins == 24U,
                "STFT analysis should produce the requested bounded spectrogram");
    ASSERT_TRUE(spectrogram.maximum_frequency == 1800.0f,
                "spectrogram should retain its configured frequency ceiling");

    sample_buffer_free(&rebuilt);
    spectrogram_data_free(&spectrogram);
    stft_reconstruction_job_free(&job);
    return 0;
}

static int test_stft_progressive_quality(void) {
    const unsigned int sample_rate = 4096U;
    const size_t count = 6144U;
    float samples[6144] = {0};
    for (size_t index = 0U; index < count; index++) {
        float time = (float)index / (float)sample_rate;
        for (int partial = 1; partial <= 28; partial++) {
            float frequency = 37.0f * (float)partial;
            float amplitude = 0.35f / sqrtf((float)partial);
            samples[index] += amplitude *
                              sinf(2.0f * 3.14159265358979323846f * frequency * time +
                                   0.13f * (float)partial);
        }
    }
    SampleBuffer source = {samples, count, sample_rate};
    StftReconstructionJob top_five;
    StftReconstructionJob top_thirty_seven;
    stft_reconstruction_job_init(&top_five);
    stft_reconstruction_job_init(&top_thirty_seven);
    ASSERT_TRUE(stft_reconstruction_job_begin(
                    &top_five, &source, 1024U, 256U, 5, false, 0U, 0U, 0.0f),
                "Top-5 STFT job should initialize");
    ASSERT_TRUE(stft_reconstruction_job_begin(
                    &top_thirty_seven, &source, 1024U, 256U, 37, false, 0U, 0U, 0.0f),
                "an arbitrary Top-37 STFT job should initialize");
    ASSERT_TRUE(finish_stft_job(&top_five, 7U) && finish_stft_job(&top_thirty_seven, 7U),
                "progressive STFT jobs should complete");
    ASSERT_TRUE(top_thirty_seven.top_component_count == 37,
                "STFT reconstruction should preserve an exact custom component count");

    float top_five_energy = stft_reconstruction_job_retained_energy(&top_five);
    float top_thirty_seven_energy =
        stft_reconstruction_job_retained_energy(&top_thirty_seven);
    SampleBuffer five = stft_reconstruction_job_take_output(&top_five);
    SampleBuffer thirty_seven =
        stft_reconstruction_job_take_output(&top_thirty_seven);
    float five_error = root_mean_square_error(&source, &five);
    float thirty_seven_error =
        root_mean_square_error(&source, &thirty_seven);
    ASSERT_TRUE(top_thirty_seven_energy > top_five_energy,
                "Top-37 should retain more spectral energy than Top-5");
    ASSERT_TRUE(thirty_seven_error < five_error,
                "adding STFT components should improve full-file reconstruction error");

    sample_buffer_free(&five);
    sample_buffer_free(&thirty_seven);
    stft_reconstruction_job_free(&top_five);
    stft_reconstruction_job_free(&top_thirty_seven);
    return 0;
}

static int test_strided_stereo_reconstruction(void) {
    const unsigned int sample_rate = 1024U;
    const size_t frame_count = 1024U;
    float stereo[2048] = {0};
    for (size_t frame = 0U; frame < frame_count; frame++) {
        float time = (float)frame / (float)sample_rate;
        stereo[frame * 2U] =
            0.6f * sinf(2.0f * 3.14159265358979323846f *
                        32.0f * time);
        stereo[frame * 2U + 1U] =
            0.4f * sinf(2.0f * 3.14159265358979323846f *
                        96.0f * time);
    }

    GlobalFourierJob left;
    GlobalFourierJob right;
    global_fourier_job_init(&left);
    global_fourier_job_init(&right);
    int available =
        global_fourier_available_component_count(frame_count);
    ASSERT_TRUE(global_fourier_job_begin_analysis_strided(
                    &left,
                    stereo,
                    frame_count,
                    2U,
                    sample_rate,
                    available) &&
                    global_fourier_job_begin_analysis_strided(
                        &right,
                        stereo + 1U,
                        frame_count,
                        2U,
                        sample_rate,
                        available),
                "stereo channel views should initialize independent global models");
    ASSERT_TRUE(finish_global_fourier_job(&left, 41U) &&
                    finish_global_fourier_job(&right, 41U),
                "strided global channel analysis should complete");
    ASSERT_TRUE(left.components[0].bin == 32U &&
                    right.components[0].bin == 96U,
                "strided global analysis should preserve distinct left and right spectra");

    StftReconstructionJob right_stft;
    stft_reconstruction_job_init(&right_stft);
    ASSERT_TRUE(stft_reconstruction_job_begin_strided(
                    &right_stft,
                    stereo + 1U,
                    frame_count,
                    2U,
                    sample_rate,
                    512U,
                    128U,
                    255,
                    false,
                    0U,
                    0U,
                    0.0f),
                "strided right-channel STFT should initialize");
    ASSERT_TRUE(finish_stft_job(&right_stft, 8U),
                "strided right-channel STFT should complete");
    SampleBuffer rebuilt =
        stft_reconstruction_job_take_output(&right_stft);
    double squared_error = 0.0;
    for (size_t frame = 0U; frame < frame_count; frame++) {
        double difference =
            (double)stereo[frame * 2U + 1U] -
            (double)rebuilt.samples[frame];
        squared_error += difference * difference;
    }
    ASSERT_TRUE(sqrt(squared_error / (double)frame_count) <
                    0.00002,
                "full-bin strided STFT should reconstruct the selected channel");

    sample_buffer_free(&rebuilt);
    stft_reconstruction_job_free(&right_stft);
    global_fourier_job_free(&left);
    global_fourier_job_free(&right);
    return 0;
}

static int test_global_fourier_memory_policy(void) {
    const size_t sample_count = 4096U;
    int available =
        global_fourier_available_component_count(sample_count);
    size_t mono_bytes =
        global_fourier_estimated_multichannel_analysis_bytes(
            sample_count, available, 1U);
    size_t stereo_bytes =
        global_fourier_estimated_multichannel_analysis_bytes(
            sample_count, available, 2U);

    ASSERT_TRUE(mono_bytes > 0U &&
                    stereo_bytes == mono_bytes * 2U,
                "multichannel FFT estimates should scale safely by channel count");
    ASSERT_TRUE(
        global_fourier_recommended_channel_count(
            sample_count, available, 2U, mono_bytes) == 1U,
        "a stereo source should fall back to one mono FFT model when only one model fits");
    ASSERT_TRUE(
        global_fourier_recommended_channel_count(
            sample_count, available, 2U, stereo_bytes) == 2U,
        "a stereo source should preserve stereo when both FFT models fit");
    ASSERT_TRUE(
        global_fourier_recommended_channel_count(
            sample_count, available, 2U, mono_bytes - 1U) ==
            0U,
        "whole-file FFT should be unavailable when even the mono model exceeds the limit");
    return 0;
}

int main(void) {
    if (test_sine_length() != 0) return 1;
    if (test_hann_window() != 0) return 1;
    if (test_additive_normalization() != 0) return 1;
    if (test_peak_detection() != 0) return 1;
    if (test_spectrum_for_a4() != 0) return 1;
    if (test_pitch_for_a4() != 0) return 1;
    if (test_pitch_for_harmonic_tone() != 0) return 1;
    if (test_pitch_rejects_silence() != 0) return 1;
    if (test_stereo_downmix() != 0) return 1;
    if (test_waveform_summary() != 0) return 1;
    if (test_harmonic_extraction() != 0) return 1;
    if (test_harmonic_resynthesis() != 0) return 1;
    if (test_fourier_component_reconstruction() != 0) return 1;
    if (test_global_fourier_fixed_components() != 0) return 1;
    if (test_global_fourier_dc_and_nyquist() != 0) return 1;
    if (test_stft_spectrogram_only() != 0) return 1;
    if (test_stft_incremental_reconstruction() != 0) return 1;
    if (test_stft_progressive_quality() != 0) return 1;
    if (test_strided_stereo_reconstruction() != 0) return 1;
    if (test_global_fourier_memory_policy() != 0) return 1;

    puts("All DSP tests passed.");
    return 0;
}
