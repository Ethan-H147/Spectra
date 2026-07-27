#include "dsp/global_fourier_reconstruction.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool next_fft_size(size_t sample_count, uint32_t *fft_size) {
    if (sample_count == 0U || fft_size == NULL || sample_count > UINT32_MAX) {
        return false;
    }
    uint32_t size = 1U;
    while ((size_t)size < sample_count) {
        if (size > UINT32_MAX / 2U) return false;
        size <<= 1U;
    }
    *fft_size = size;
    return true;
}

static unsigned int fft_bits(uint32_t size) {
    unsigned int bits = 0U;
    while (size > 1U) {
        size >>= 1U;
        bits++;
    }
    return bits;
}

static uint64_t fft_butterfly_count(uint32_t size) {
    return (uint64_t)(size / 2U) * (uint64_t)fft_bits(size);
}

int global_fourier_available_component_count(size_t sample_count) {
    uint32_t fft_size = 0U;
    if (!next_fft_size(sample_count, &fft_size)) return 0;
    uint32_t independent_bins = fft_size / 2U + 1U;
    return independent_bins <= (uint32_t)INT_MAX
               ? (int)independent_bins
               : 0;
}

size_t global_fourier_estimated_analysis_bytes(
    size_t sample_count,
    int maximum_component_count) {
    uint32_t fft_size = 0U;
    if (!next_fft_size(sample_count, &fft_size) ||
        maximum_component_count <= 0 ||
        (size_t)maximum_component_count >
            SIZE_MAX / sizeof(GlobalFourierComponent)) {
        return 0U;
    }
    size_t transform_bytes =
        (size_t)fft_size * sizeof(float);
    if (transform_bytes > SIZE_MAX / 2U) return 0U;
    transform_bytes *= 2U;
    size_t component_bytes =
        (size_t)maximum_component_count *
        sizeof(GlobalFourierComponent);
    if (transform_bytes > SIZE_MAX - component_bytes) {
        return 0U;
    }
    return transform_bytes + component_bytes;
}

size_t global_fourier_estimated_multichannel_analysis_bytes(
    size_t sample_count,
    int maximum_component_count,
    unsigned int channel_count) {
    if (channel_count == 0U) return 0U;
    size_t per_channel_bytes =
        global_fourier_estimated_analysis_bytes(
            sample_count, maximum_component_count);
    if (per_channel_bytes == 0U ||
        per_channel_bytes >
            SIZE_MAX / (size_t)channel_count) {
        return 0U;
    }
    return per_channel_bytes * (size_t)channel_count;
}

bool global_fourier_analysis_fits_memory(
    size_t sample_count,
    int maximum_component_count,
    unsigned int channel_count,
    size_t byte_limit) {
    size_t estimated_bytes =
        global_fourier_estimated_multichannel_analysis_bytes(
            sample_count,
            maximum_component_count,
            channel_count);
    return estimated_bytes > 0U &&
           (byte_limit == 0U || estimated_bytes <= byte_limit);
}

unsigned int global_fourier_recommended_channel_count(
    size_t sample_count,
    int maximum_component_count,
    unsigned int source_channel_count,
    size_t byte_limit) {
    if (source_channel_count == 0U) return 0U;
    if (global_fourier_analysis_fits_memory(
            sample_count,
            maximum_component_count,
            source_channel_count,
            byte_limit)) {
        return source_channel_count;
    }
    return global_fourier_analysis_fits_memory(
               sample_count,
               maximum_component_count,
               1U,
               byte_limit)
               ? 1U
               : 0U;
}

static void free_transform_buffers(GlobalFourierJob *job) {
    free(job->real);
    free(job->imaginary);
    job->real = NULL;
    job->imaginary = NULL;
    sample_buffer_free(&job->output);
}

void global_fourier_job_init(GlobalFourierJob *job) {
    if (job != NULL) {
        *job = (GlobalFourierJob){0};
    }
}

void global_fourier_job_free(GlobalFourierJob *job) {
    if (job == NULL) return;
    free_transform_buffers(job);
    free(job->components);
    *job = (GlobalFourierJob){0};
}

static void begin_bit_reverse(GlobalFourierJob *job,
                              GlobalFourierPhase phase,
                              uint64_t total_work) {
    job->phase = phase;
    job->active = true;
    job->completed_work = 0U;
    job->total_work = total_work;
    job->bit_reverse_index = 1U;
    job->bit_reverse_value = 0U;
}

bool global_fourier_job_begin_analysis(GlobalFourierJob *job,
                                       const SampleBuffer *source,
                                       int maximum_component_count) {
    if (source == NULL) {
        return false;
    }
    return global_fourier_job_begin_analysis_strided(
        job,
        source->samples,
        source->count,
        1U,
        source->sample_rate,
        maximum_component_count);
}

bool global_fourier_job_begin_analysis_strided(
    GlobalFourierJob *job,
    const float *source_samples,
    size_t source_count,
    size_t source_stride,
    unsigned int sample_rate,
    int maximum_component_count) {
    if (job == NULL || source_samples == NULL ||
        source_count == 0U || source_stride == 0U ||
        sample_rate == 0U ||
        (source_count - 1U) > SIZE_MAX / source_stride ||
        maximum_component_count <= 0) {
        return false;
    }

    uint32_t fft_size = 0U;
    if (!next_fft_size(source_count, &fft_size) ||
        (size_t)fft_size > SIZE_MAX / sizeof(float)) {
        return false;
    }
    uint32_t available_bins = fft_size / 2U + 1U;
    if ((uint32_t)maximum_component_count > available_bins) {
        maximum_component_count = (int)available_bins;
    }
    if ((size_t)maximum_component_count >
        SIZE_MAX / sizeof(GlobalFourierComponent)) {
        return false;
    }

    GlobalFourierJob prepared = {
        .source_samples = source_samples,
        .source_count = source_count,
        .source_stride = source_stride,
        .sample_rate = sample_rate,
        .fft_size = fft_size,
        .maximum_component_count = maximum_component_count,
    };
    prepared.real = (float *)calloc((size_t)fft_size, sizeof(float));
    prepared.imaginary = (float *)calloc((size_t)fft_size, sizeof(float));
    prepared.components = (GlobalFourierComponent *)calloc(
        (size_t)maximum_component_count, sizeof(GlobalFourierComponent));
    if (prepared.real == NULL || prepared.imaginary == NULL ||
        prepared.components == NULL) {
        global_fourier_job_free(&prepared);
        return false;
    }
    for (size_t index = 0U; index < source_count; index++) {
        prepared.real[index] =
            source_samples[index * source_stride];
    }

    uint64_t total_work = (uint64_t)(fft_size - 1U) +
                          fft_butterfly_count(fft_size) +
                          (uint64_t)available_bins +
                          (uint64_t)(maximum_component_count / 2) +
                          (uint64_t)(maximum_component_count - 1) +
                          (uint64_t)maximum_component_count;
    begin_bit_reverse(
        &prepared, GLOBAL_FOURIER_ANALYSIS_BIT_REVERSE, total_work);
    global_fourier_job_free(job);
    *job = prepared;
    return true;
}

static void heap_swap(GlobalFourierComponent *left,
                      GlobalFourierComponent *right) {
    GlobalFourierComponent temporary = *left;
    *left = *right;
    *right = temporary;
}

static void heap_sift_up(GlobalFourierComponent *heap, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap[parent].power <= heap[index].power) break;
        heap_swap(&heap[parent], &heap[index]);
        index = parent;
    }
}

static void heap_sift_down(GlobalFourierComponent *heap,
                           int count,
                           int index) {
    for (;;) {
        int smallest = index;
        int left = index * 2 + 1;
        int right = left + 1;
        if (left < count && heap[left].power < heap[smallest].power) {
            smallest = left;
        }
        if (right < count && heap[right].power < heap[smallest].power) {
            smallest = right;
        }
        if (smallest == index) break;
        heap_swap(&heap[index], &heap[smallest]);
        index = smallest;
    }
}

static void setup_fft_stage(GlobalFourierJob *job, bool inverse) {
    job->block_size = 2U;
    job->block_start = 0U;
    job->butterfly_index = 0U;
    job->twiddle_real = 1.0;
    job->twiddle_imaginary = 0.0;
    double angle = (inverse ? 2.0 : -2.0) * M_PI / 2.0;
    job->twiddle_step_real = cos(angle);
    job->twiddle_step_imaginary = sin(angle);
}

static bool advance_fft_stage(GlobalFourierJob *job, bool inverse) {
    if (job->block_size >= job->fft_size) {
        return false;
    }
    job->block_size <<= 1U;
    job->block_start = 0U;
    job->butterfly_index = 0U;
    job->twiddle_real = 1.0;
    job->twiddle_imaginary = 0.0;
    double angle =
        (inverse ? 2.0 : -2.0) * M_PI / (double)job->block_size;
    job->twiddle_step_real = cos(angle);
    job->twiddle_step_imaginary = sin(angle);
    return true;
}

static void process_bit_reverse_step(GlobalFourierJob *job, bool inverse) {
    uint32_t index = job->bit_reverse_index;
    uint32_t bit = job->fft_size >> 1U;
    while ((job->bit_reverse_value & bit) != 0U) {
        job->bit_reverse_value ^= bit;
        bit >>= 1U;
    }
    job->bit_reverse_value ^= bit;
    if (index < job->bit_reverse_value) {
        uint32_t reverse = job->bit_reverse_value;
        float real = job->real[index];
        float imaginary = job->imaginary[index];
        job->real[index] = job->real[reverse];
        job->imaginary[index] = job->imaginary[reverse];
        job->real[reverse] = real;
        job->imaginary[reverse] = imaginary;
    }
    job->bit_reverse_index++;
    job->completed_work++;

    if (job->bit_reverse_index >= job->fft_size) {
        job->phase = inverse ? GLOBAL_FOURIER_INVERSE_FFT
                             : GLOBAL_FOURIER_ANALYSIS_FFT;
        setup_fft_stage(job, inverse);
    }
}

static void process_butterfly_step(GlobalFourierJob *job, bool inverse) {
    uint32_t half_block = job->block_size / 2U;
    uint32_t even_index =
        job->block_start + job->butterfly_index;
    uint32_t odd_index = even_index + half_block;
    float even_real = job->real[even_index];
    float even_imaginary = job->imaginary[even_index];
    float odd_real = job->real[odd_index];
    float odd_imaginary = job->imaginary[odd_index];
    float transformed_real =
        (float)(job->twiddle_real * (double)odd_real -
                job->twiddle_imaginary * (double)odd_imaginary);
    float transformed_imaginary =
        (float)(job->twiddle_real * (double)odd_imaginary +
                job->twiddle_imaginary * (double)odd_real);
    job->real[odd_index] = even_real - transformed_real;
    job->imaginary[odd_index] = even_imaginary - transformed_imaginary;
    job->real[even_index] = even_real + transformed_real;
    job->imaginary[even_index] = even_imaginary + transformed_imaginary;

    double next_real =
        job->twiddle_real * job->twiddle_step_real -
        job->twiddle_imaginary * job->twiddle_step_imaginary;
    job->twiddle_imaginary =
        job->twiddle_real * job->twiddle_step_imaginary +
        job->twiddle_imaginary * job->twiddle_step_real;
    job->twiddle_real = next_real;
    job->butterfly_index++;
    job->completed_work++;

    if (job->butterfly_index >= half_block) {
        job->block_start += job->block_size;
        job->butterfly_index = 0U;
        job->twiddle_real = 1.0;
        job->twiddle_imaginary = 0.0;
        if (job->block_start >= job->fft_size &&
            !advance_fft_stage(job, inverse)) {
            if (inverse) {
                job->phase = GLOBAL_FOURIER_SCALING;
                job->scaling_index = 0U;
            } else {
                job->phase = GLOBAL_FOURIER_RANKING;
                job->ranking_bin = 0U;
            }
        }
    }
}

static void process_ranking_step(GlobalFourierJob *job) {
    uint32_t bin = job->ranking_bin;
    double real = (double)job->real[bin];
    double imaginary = (double)job->imaginary[bin];
    double power = real * real + imaginary * imaginary;
    if (bin != 0U && bin != job->fft_size / 2U) {
        power *= 2.0;
    }
    GlobalFourierComponent component = {
        .bin = bin,
        .real = job->real[bin],
        .imaginary = job->imaginary[bin],
        .power = power,
    };
    job->total_spectral_energy += power;
    if (job->component_count < job->maximum_component_count) {
        int index = job->component_count++;
        job->components[index] = component;
        if (job->maximum_component_count <
            (int)(job->fft_size / 2U + 1U)) {
            heap_sift_up(job->components, index);
        }
    } else if (power > job->components[0].power) {
        job->components[0] = component;
        heap_sift_down(job->components, job->component_count, 0);
    }
    job->ranking_bin++;
    job->completed_work++;

    if (job->ranking_bin > job->fft_size / 2U) {
        job->sort_heapify_index = job->component_count / 2 - 1;
        job->sort_heap_size = job->component_count;
        job->phase = GLOBAL_FOURIER_SORT_HEAPIFY;
    }
}

static void finish_global_analysis(GlobalFourierJob *job) {
    free(job->real);
    free(job->imaginary);
    job->real = NULL;
    job->imaginary = NULL;
    job->analysis_ready = true;
    job->active = false;
    job->phase = GLOBAL_FOURIER_ANALYSIS_READY;
}

static void process_sort_heapify_step(GlobalFourierJob *job) {
    if (job->sort_heapify_index >= 0) {
        heap_sift_down(job->components,
                       job->component_count,
                       job->sort_heapify_index);
        job->sort_heapify_index--;
        job->completed_work++;
    }
    if (job->sort_heapify_index < 0) {
        job->phase = GLOBAL_FOURIER_SORT_EXTRACT;
    }
}

static void process_sort_extract_step(GlobalFourierJob *job) {
    if (job->sort_heap_size <= 1) {
        job->cumulative_energy_index = 0;
        job->cumulative_energy = 0.0;
        job->phase = GLOBAL_FOURIER_CUMULATIVE_ENERGY;
        return;
    }
    heap_swap(&job->components[0],
              &job->components[job->sort_heap_size - 1]);
    job->sort_heap_size--;
    heap_sift_down(job->components, job->sort_heap_size, 0);
    job->completed_work++;
    if (job->sort_heap_size <= 1) {
        job->cumulative_energy_index = 0;
        job->cumulative_energy = 0.0;
        job->phase = GLOBAL_FOURIER_CUMULATIVE_ENERGY;
    }
}

static void process_cumulative_energy_step(GlobalFourierJob *job) {
    job->cumulative_energy +=
        job->components[job->cumulative_energy_index].power;
    job->components[job->cumulative_energy_index].power =
        job->cumulative_energy;
    job->cumulative_energy_index++;
    job->completed_work++;
    if (job->cumulative_energy_index >= job->component_count) {
        finish_global_analysis(job);
    }
}

bool global_fourier_job_begin_reconstruction(GlobalFourierJob *job,
                                             int component_count) {
    if (job == NULL || !job->analysis_ready || job->components == NULL ||
        job->fft_size == 0U || component_count <= 0) {
        return false;
    }
    if (component_count > job->component_count) {
        component_count = job->component_count;
    }

    free_transform_buffers(job);
    job->real = (float *)calloc((size_t)job->fft_size, sizeof(float));
    job->imaginary =
        (float *)calloc((size_t)job->fft_size, sizeof(float));
    if (job->real == NULL || job->imaginary == NULL) {
        free_transform_buffers(job);
        job->phase = GLOBAL_FOURIER_FAILED;
        job->active = false;
        return false;
    }

    job->requested_component_count = component_count;
    job->rendered_component_count = 0;
    job->retained_spectral_energy =
        job->components[component_count - 1].power;
    job->population_index = 0;
    job->reconstruction_ready = false;
    job->phase = GLOBAL_FOURIER_RECONSTRUCTION_POPULATE;
    job->active = true;
    job->completed_work = 0U;
    job->total_work = (uint64_t)component_count +
                          (uint64_t)(job->fft_size - 1U) +
                          fft_butterfly_count(job->fft_size) +
                          (uint64_t)job->source_count;
    return true;
}

static void process_population_step(GlobalFourierJob *job) {
    const GlobalFourierComponent *component =
        &job->components[job->population_index];
    uint32_t bin = component->bin;
    job->real[bin] = component->real;
    job->imaginary[bin] = component->imaginary;
    if (bin != 0U && bin != job->fft_size / 2U) {
        job->real[job->fft_size - bin] = component->real;
        job->imaginary[job->fft_size - bin] =
            -component->imaginary;
    }
    job->population_index++;
    job->completed_work++;
    if (job->population_index >= job->requested_component_count) {
        job->phase = GLOBAL_FOURIER_INVERSE_BIT_REVERSE;
        job->bit_reverse_index = 1U;
        job->bit_reverse_value = 0U;
    }
}

static void process_scaling_step(GlobalFourierJob *job) {
    job->real[job->scaling_index] /= (float)job->fft_size;
    job->scaling_index++;
    job->completed_work++;
    if (job->scaling_index >= job->source_count) {
        free(job->imaginary);
        job->imaginary = NULL;
        job->output = (SampleBuffer){
            .samples = job->real,
            .count = job->source_count,
            .sample_rate = job->sample_rate,
        };
        job->real = NULL;
        job->rendered_component_count = job->requested_component_count;
        job->reconstruction_ready = true;
        job->active = false;
        job->phase = GLOBAL_FOURIER_RECONSTRUCTION_READY;
    }
}

size_t global_fourier_job_process(GlobalFourierJob *job,
                                  size_t maximum_operations) {
    if (job == NULL || !job->active || maximum_operations == 0U) {
        return 0U;
    }
    size_t processed = 0U;
    while (processed < maximum_operations && job->active) {
        switch (job->phase) {
            case GLOBAL_FOURIER_ANALYSIS_BIT_REVERSE:
                process_bit_reverse_step(job, false);
                break;
            case GLOBAL_FOURIER_ANALYSIS_FFT:
                process_butterfly_step(job, false);
                break;
            case GLOBAL_FOURIER_RANKING:
                process_ranking_step(job);
                break;
            case GLOBAL_FOURIER_SORT_HEAPIFY:
                process_sort_heapify_step(job);
                break;
            case GLOBAL_FOURIER_SORT_EXTRACT:
                process_sort_extract_step(job);
                break;
            case GLOBAL_FOURIER_CUMULATIVE_ENERGY:
                process_cumulative_energy_step(job);
                break;
            case GLOBAL_FOURIER_RECONSTRUCTION_POPULATE:
                process_population_step(job);
                break;
            case GLOBAL_FOURIER_INVERSE_BIT_REVERSE:
                process_bit_reverse_step(job, true);
                break;
            case GLOBAL_FOURIER_INVERSE_FFT:
                process_butterfly_step(job, true);
                break;
            case GLOBAL_FOURIER_SCALING:
                process_scaling_step(job);
                break;
            default:
                job->active = false;
                break;
        }
        processed++;
    }
    return processed;
}

float global_fourier_job_progress(const GlobalFourierJob *job) {
    if (job == NULL || job->total_work == 0U) return 0.0f;
    if (!job->active &&
        (job->analysis_ready || job->reconstruction_ready)) {
        return 1.0f;
    }
    double progress =
        (double)job->completed_work / (double)job->total_work;
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;
    return (float)progress;
}

float global_fourier_job_retained_energy(const GlobalFourierJob *job,
                                         int component_count) {
    if (job == NULL || !job->analysis_ready ||
        job->total_spectral_energy <= 0.0 || component_count <= 0) {
        return 0.0f;
    }
    if (component_count > job->component_count) {
        component_count = job->component_count;
    }
    double retained = job->components[component_count - 1].power;
    return (float)(retained / job->total_spectral_energy);
}

int global_fourier_job_component_count_for_energy(
    const GlobalFourierJob *job,
    float retained_energy_target) {
    if (job == NULL || !job->analysis_ready ||
        job->components == NULL || job->component_count <= 0 ||
        job->total_spectral_energy <= 0.0) {
        return 0;
    }
    if (retained_energy_target <= 0.0f) return 1;
    if (retained_energy_target >= 1.0f) {
        return job->component_count;
    }

    double target =
        (double)retained_energy_target *
        job->total_spectral_energy;
    int lower = 0;
    int upper = job->component_count - 1;
    while (lower < upper) {
        int middle = lower + (upper - lower) / 2;
        if (job->components[middle].power >= target) {
            upper = middle;
        } else {
            lower = middle + 1;
        }
    }
    return lower + 1;
}

float global_fourier_job_frequency_resolution(const GlobalFourierJob *job) {
    return job != NULL && job->fft_size > 0U
               ? (float)job->sample_rate / (float)job->fft_size
               : 0.0f;
}

const char *global_fourier_job_phase_label(const GlobalFourierJob *job) {
    if (job == NULL) return "Idle";
    switch (job->phase) {
        case GLOBAL_FOURIER_ANALYSIS_BIT_REVERSE:
            return "Preparing zero-padded whole-file FFT";
        case GLOBAL_FOURIER_ANALYSIS_FFT:
            return "Analyzing zero-padded whole-file FFT";
        case GLOBAL_FOURIER_RANKING:
            return "Ranking padded FFT bins";
        case GLOBAL_FOURIER_SORT_HEAPIFY:
        case GLOBAL_FOURIER_SORT_EXTRACT:
            return "Sorting padded FFT bins by spectral energy";
        case GLOBAL_FOURIER_CUMULATIVE_ENERGY:
            return "Indexing cumulative spectral energy";
        case GLOBAL_FOURIER_ANALYSIS_READY:
            return "Padded FFT ranking ready";
        case GLOBAL_FOURIER_RECONSTRUCTION_POPULATE:
            return "Selecting padded FFT bins";
        case GLOBAL_FOURIER_INVERSE_BIT_REVERSE:
            return "Preparing padded-bin reconstruction";
        case GLOBAL_FOURIER_INVERSE_FFT:
            return "Reconstructing selected padded FFT bins";
        case GLOBAL_FOURIER_SCALING:
            return "Finalizing reconstructed audio";
        case GLOBAL_FOURIER_RECONSTRUCTION_READY:
            return "Fixed-component reconstruction ready";
        case GLOBAL_FOURIER_FAILED:
            return "Whole-file Fourier processing failed";
        default:
            return "Idle";
    }
}

SampleBuffer global_fourier_job_take_output(GlobalFourierJob *job) {
    SampleBuffer output = {0};
    if (job == NULL || !job->reconstruction_ready) return output;
    output = job->output;
    job->output = (SampleBuffer){0};
    job->reconstruction_ready = false;
    return output;
}
