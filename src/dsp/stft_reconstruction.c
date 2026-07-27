#include "dsp/stft_reconstruction.h"

#include "dsp/fft.h"
#include "dsp/signal_utils.h"
#include "dsp/windowing.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool is_power_of_two(unsigned int value) {
    return value >= 8U && (value & (value - 1U)) == 0U;
}

static int compare_ranked_bins(const void *left_value, const void *right_value) {
    const StftBinRank *left = (const StftBinRank *)left_value;
    const StftBinRank *right = (const StftBinRank *)right_value;
    if (left->power < right->power) return 1;
    if (left->power > right->power) return -1;
    if (left->bin > right->bin) return 1;
    if (left->bin < right->bin) return -1;
    return 0;
}

void spectrogram_data_free(SpectrogramData *spectrogram) {
    if (spectrogram == NULL) {
        return;
    }
    free(spectrogram->db_values);
    *spectrogram = (SpectrogramData){0};
}

void stft_reconstruction_job_init(StftReconstructionJob *job) {
    if (job != NULL) {
        *job = (StftReconstructionJob){0};
    }
}

void stft_reconstruction_job_free(StftReconstructionJob *job) {
    if (job == NULL) {
        return;
    }
    free(job->window);
    free(job->real);
    free(job->imaginary);
    free(job->overlap_weights);
    free(job->kept_bins);
    free(job->ranked_bins);
    sample_buffer_free(&job->output);
    spectrogram_data_free(&job->spectrogram);
    *job = (StftReconstructionJob){0};
}

bool stft_reconstruction_job_begin(StftReconstructionJob *job,
                                   const SampleBuffer *source,
                                   unsigned int window_size,
                                   unsigned int hop_size,
                                   int top_component_count,
                                   bool include_spectrogram,
                                   unsigned int spectrogram_time_bins,
                                   unsigned int spectrogram_frequency_bins,
                                   float spectrogram_maximum_frequency) {
    if (source == NULL) {
        return false;
    }
    return stft_reconstruction_job_begin_strided(
        job,
        source->samples,
        source->count,
        1U,
        source->sample_rate,
        window_size,
        hop_size,
        top_component_count,
        include_spectrogram,
        spectrogram_time_bins,
        spectrogram_frequency_bins,
        spectrogram_maximum_frequency);
}

bool stft_reconstruction_job_begin_strided(
    StftReconstructionJob *job,
    const float *source_samples,
    size_t source_count,
    size_t source_stride,
    unsigned int sample_rate,
    unsigned int window_size,
    unsigned int hop_size,
    int top_component_count,
    bool include_spectrogram,
    unsigned int spectrogram_time_bins,
    unsigned int spectrogram_frequency_bins,
    float spectrogram_maximum_frequency) {
    if (job == NULL || source_samples == NULL || source_count == 0U ||
        source_stride == 0U || sample_rate == 0U ||
        (source_count - 1U) > SIZE_MAX / source_stride ||
        !is_power_of_two(window_size) || hop_size == 0U ||
        hop_size > window_size || top_component_count < 0 ||
        (top_component_count == 0 && !include_spectrogram)) {
        return false;
    }
    if (source_count > SIZE_MAX / sizeof(float) ||
        source_count > SIZE_MAX - ((size_t)hop_size - 1U)) {
        return false;
    }

    size_t positive_bin_count = (size_t)window_size / 2U - 1U;
    if ((size_t)top_component_count > positive_bin_count) {
        top_component_count = (int)positive_bin_count;
    }
    size_t frame_count =
        1U +
        (source_count + (size_t)hop_size - 1U) /
            (size_t)hop_size;

    StftReconstructionJob prepared = {
        .source_samples = source_samples,
        .source_count = source_count,
        .source_stride = source_stride,
        .sample_rate = sample_rate,
        .window_size = window_size,
        .hop_size = hop_size,
        .frame_count = frame_count,
        .top_component_count = top_component_count,
        .include_spectrogram = include_spectrogram,
        .active = true,
        .output = {
            .count = source_count,
            .sample_rate = sample_rate,
        },
    };
    prepared.window = (float *)calloc(window_size, sizeof(float));
    prepared.real = (float *)calloc(window_size, sizeof(float));
    prepared.imaginary = (float *)calloc(window_size, sizeof(float));
    if (top_component_count > 0) {
        prepared.overlap_weights = (float *)calloc(source_count, sizeof(float));
        prepared.kept_bins =
            (unsigned char *)calloc((size_t)window_size / 2U + 1U, sizeof(unsigned char));
        prepared.ranked_bins =
            (StftBinRank *)calloc(positive_bin_count, sizeof(StftBinRank));
        prepared.output.samples = (float *)calloc(source_count, sizeof(float));
    }
    if (prepared.window == NULL || prepared.real == NULL || prepared.imaginary == NULL ||
        (top_component_count > 0 &&
         (prepared.overlap_weights == NULL || prepared.kept_bins == NULL ||
          prepared.ranked_bins == NULL || prepared.output.samples == NULL))) {
        stft_reconstruction_job_free(&prepared);
        return false;
    }

    hann_window(prepared.window, window_size);
    for (unsigned int index = 0U; index < window_size; index++) {
        prepared.window_sum += prepared.window[index];
    }

    if (include_spectrogram) {
        if (spectrogram_time_bins == 0U || spectrogram_frequency_bins == 0U ||
            (size_t)spectrogram_time_bins > SIZE_MAX / (size_t)spectrogram_frequency_bins) {
            stft_reconstruction_job_free(&prepared);
            return false;
        }
        if ((size_t)spectrogram_time_bins > frame_count) {
            spectrogram_time_bins = (unsigned int)frame_count;
        }
        float nyquist = (float)sample_rate * 0.5f;
        if (spectrogram_maximum_frequency <= 0.0f || spectrogram_maximum_frequency > nyquist) {
            spectrogram_maximum_frequency = nyquist;
        }
        size_t value_count = (size_t)spectrogram_time_bins * (size_t)spectrogram_frequency_bins;
        if (value_count > SIZE_MAX / sizeof(float)) {
            stft_reconstruction_job_free(&prepared);
            return false;
        }
        prepared.spectrogram.db_values = (float *)malloc(value_count * sizeof(float));
        if (prepared.spectrogram.db_values == NULL) {
            stft_reconstruction_job_free(&prepared);
            return false;
        }
        for (size_t index = 0U; index < value_count; index++) {
            prepared.spectrogram.db_values[index] = -120.0f;
        }
        prepared.spectrogram.time_bins = spectrogram_time_bins;
        prepared.spectrogram.frequency_bins = spectrogram_frequency_bins;
        prepared.spectrogram.maximum_frequency = spectrogram_maximum_frequency;
        prepared.spectrogram.duration_seconds =
            (float)source_count / (float)sample_rate;
    }

    stft_reconstruction_job_free(job);
    *job = prepared;
    return true;
}

static void capture_spectrogram_frame(StftReconstructionJob *job, size_t frame_index) {
    if (!job->include_spectrogram || job->spectrogram.db_values == NULL) {
        return;
    }
    unsigned int time_bin =
        (unsigned int)(frame_index * (size_t)job->spectrogram.time_bins / job->frame_count);
    if (time_bin >= job->spectrogram.time_bins) {
        time_bin = job->spectrogram.time_bins - 1U;
    }

    float magnitude_scale = job->window_sum > 0.0f ? 2.0f / job->window_sum : 1.0f;
    unsigned int last_bin = job->window_size / 2U;
    for (unsigned int bin = 1U; bin < last_bin; bin++) {
        float frequency = (float)bin * (float)job->sample_rate / (float)job->window_size;
        if (frequency > job->spectrogram.maximum_frequency) {
            break;
        }
        unsigned int frequency_bin =
            (unsigned int)(frequency / job->spectrogram.maximum_frequency *
                           (float)job->spectrogram.frequency_bins);
        if (frequency_bin >= job->spectrogram.frequency_bins) {
            frequency_bin = job->spectrogram.frequency_bins - 1U;
        }
        float magnitude = hypotf(job->real[bin], job->imaginary[bin]) * magnitude_scale;
        float db = linear_to_db(magnitude);
        size_t output_index =
            (size_t)frequency_bin * (size_t)job->spectrogram.time_bins + (size_t)time_bin;
        if (db > job->spectrogram.db_values[output_index]) {
            job->spectrogram.db_values[output_index] = db;
        }
    }
}

static void select_strongest_bins(StftReconstructionJob *job) {
    size_t positive_bin_count = (size_t)job->window_size / 2U - 1U;
    for (size_t index = 0U; index < positive_bin_count; index++) {
        unsigned int bin = (unsigned int)index + 1U;
        float real = job->real[bin];
        float imaginary = job->imaginary[bin];
        job->ranked_bins[index] = (StftBinRank){
            .bin = bin,
            .power = real * real + imaginary * imaginary,
        };
    }
    qsort(job->ranked_bins, positive_bin_count, sizeof(StftBinRank), compare_ranked_bins);
    memset(job->kept_bins, 0, ((size_t)job->window_size / 2U + 1U) * sizeof(unsigned char));

    size_t retained_count = (size_t)job->top_component_count;
    if (retained_count > positive_bin_count) retained_count = positive_bin_count;
    for (size_t index = 0U; index < positive_bin_count; index++) {
        double power = (double)job->ranked_bins[index].power;
        job->total_spectral_energy += power;
        if (index < retained_count) {
            job->kept_bins[job->ranked_bins[index].bin] = 1U;
            job->retained_spectral_energy += power;
        }
    }

    unsigned int positive_end = job->window_size / 2U;
    for (unsigned int bin = 1U; bin < positive_end; bin++) {
        if (job->kept_bins[bin] != 0U) continue;
        job->real[bin] = 0.0f;
        job->imaginary[bin] = 0.0f;
        job->real[job->window_size - bin] = 0.0f;
        job->imaginary[job->window_size - bin] = 0.0f;
    }
}

static void inverse_fft(float *real, float *imaginary, unsigned int size) {
    for (unsigned int index = 0U; index < size; index++) {
        imaginary[index] = -imaginary[index];
    }
    fft_radix2(real, imaginary, size);
    for (unsigned int index = 0U; index < size; index++) {
        real[index] /= (float)size;
    }
}

static void process_frame(StftReconstructionJob *job, size_t frame_index) {
    size_t padding = (size_t)job->window_size / 2U;
    size_t padded_start = frame_index * (size_t)job->hop_size;
    memset(job->real, 0, (size_t)job->window_size * sizeof(float));
    memset(job->imaginary, 0, (size_t)job->window_size * sizeof(float));

    for (unsigned int index = 0U; index < job->window_size; index++) {
        size_t padded_index = padded_start + (size_t)index;
        if (padded_index < padding) continue;
        size_t source_index = padded_index - padding;
        if (source_index >= job->source_count) continue;
        job->real[index] =
            job->source_samples[source_index * job->source_stride] *
            job->window[index];
    }

    fft_radix2(job->real, job->imaginary, job->window_size);
    capture_spectrogram_frame(job, frame_index);
    if (job->top_component_count == 0) {
        return;
    }
    select_strongest_bins(job);
    inverse_fft(job->real, job->imaginary, job->window_size);

    for (unsigned int index = 0U; index < job->window_size; index++) {
        size_t padded_index = padded_start + (size_t)index;
        if (padded_index < padding) continue;
        size_t source_index = padded_index - padding;
        if (source_index >= job->source_count) continue;
        float window_value = job->window[index];
        job->output.samples[source_index] += job->real[index] * window_value;
        job->overlap_weights[source_index] += window_value * window_value;
    }
}

static void finish_job(StftReconstructionJob *job) {
    if (job->output.samples != NULL && job->overlap_weights != NULL) {
        for (size_t index = 0U; index < job->source_count; index++) {
            float weight = job->overlap_weights[index];
            job->output.samples[index] =
                weight > 1.0e-8f ? job->output.samples[index] / weight : 0.0f;
        }
    }
    job->active = false;
    job->complete = true;
}

size_t stft_reconstruction_job_process(StftReconstructionJob *job, size_t maximum_frames) {
    if (job == NULL || !job->active || job->failed || maximum_frames == 0U) {
        return 0U;
    }
    size_t processed = 0U;
    while (processed < maximum_frames && job->next_frame < job->frame_count) {
        process_frame(job, job->next_frame);
        job->next_frame++;
        processed++;
    }
    if (job->next_frame >= job->frame_count) {
        finish_job(job);
    }
    return processed;
}

float stft_reconstruction_job_progress(const StftReconstructionJob *job) {
    if (job == NULL || job->frame_count == 0U) {
        return 0.0f;
    }
    return (float)job->next_frame / (float)job->frame_count;
}

float stft_reconstruction_job_retained_energy(const StftReconstructionJob *job) {
    if (job == NULL || job->total_spectral_energy <= 0.0) {
        return 0.0f;
    }
    return (float)(job->retained_spectral_energy / job->total_spectral_energy);
}

SampleBuffer stft_reconstruction_job_take_output(StftReconstructionJob *job) {
    SampleBuffer output = {0};
    if (job == NULL || !job->complete) {
        return output;
    }
    output = job->output;
    job->output = (SampleBuffer){0};
    return output;
}

SpectrogramData stft_reconstruction_job_take_spectrogram(StftReconstructionJob *job) {
    SpectrogramData spectrogram = {0};
    if (job == NULL || !job->complete) {
        return spectrogram;
    }
    spectrogram = job->spectrogram;
    job->spectrogram = (SpectrogramData){0};
    return spectrogram;
}
