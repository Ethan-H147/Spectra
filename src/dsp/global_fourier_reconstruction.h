#ifndef SPECTRA_GLOBAL_FOURIER_RECONSTRUCTION_H
#define SPECTRA_GLOBAL_FOURIER_RECONSTRUCTION_H

#include "dsp/dsp_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t bin;
    float real;
    float imaginary;
    double power;
} GlobalFourierComponent;

typedef enum {
    GLOBAL_FOURIER_IDLE = 0,
    GLOBAL_FOURIER_ANALYSIS_BIT_REVERSE,
    GLOBAL_FOURIER_ANALYSIS_FFT,
    GLOBAL_FOURIER_RANKING,
    GLOBAL_FOURIER_SORT_HEAPIFY,
    GLOBAL_FOURIER_SORT_EXTRACT,
    GLOBAL_FOURIER_CUMULATIVE_ENERGY,
    GLOBAL_FOURIER_ANALYSIS_READY,
    GLOBAL_FOURIER_RECONSTRUCTION_POPULATE,
    GLOBAL_FOURIER_INVERSE_BIT_REVERSE,
    GLOBAL_FOURIER_INVERSE_FFT,
    GLOBAL_FOURIER_SCALING,
    GLOBAL_FOURIER_RECONSTRUCTION_READY,
    GLOBAL_FOURIER_FAILED
} GlobalFourierPhase;

typedef struct {
    const float *source_samples;
    size_t source_count;
    unsigned int sample_rate;
    uint32_t fft_size;
    int maximum_component_count;
    int component_count;
    int requested_component_count;
    int rendered_component_count;
    bool active;
    bool analysis_ready;
    bool reconstruction_ready;
    GlobalFourierPhase phase;
    uint64_t completed_work;
    uint64_t total_work;
    uint32_t bit_reverse_index;
    uint32_t bit_reverse_value;
    uint32_t block_size;
    uint32_t block_start;
    uint32_t butterfly_index;
    double twiddle_real;
    double twiddle_imaginary;
    double twiddle_step_real;
    double twiddle_step_imaginary;
    uint32_t ranking_bin;
    int sort_heapify_index;
    int sort_heap_size;
    int cumulative_energy_index;
    double cumulative_energy;
    int population_index;
    size_t scaling_index;
    double total_spectral_energy;
    double retained_spectral_energy;
    float *real;
    float *imaginary;
    GlobalFourierComponent *components;
    SampleBuffer output;
} GlobalFourierJob;

void global_fourier_job_init(GlobalFourierJob *job);
void global_fourier_job_free(GlobalFourierJob *job);
int global_fourier_available_component_count(size_t sample_count);
bool global_fourier_job_begin_analysis(GlobalFourierJob *job,
                                       const SampleBuffer *source,
                                       int maximum_component_count);
bool global_fourier_job_begin_reconstruction(GlobalFourierJob *job,
                                             int component_count);
size_t global_fourier_job_process(GlobalFourierJob *job,
                                  size_t maximum_operations);
float global_fourier_job_progress(const GlobalFourierJob *job);
float global_fourier_job_retained_energy(const GlobalFourierJob *job,
                                         int component_count);
int global_fourier_job_component_count_for_energy(
    const GlobalFourierJob *job,
    float retained_energy_target);
float global_fourier_job_frequency_resolution(const GlobalFourierJob *job);
const char *global_fourier_job_phase_label(const GlobalFourierJob *job);
SampleBuffer global_fourier_job_take_output(GlobalFourierJob *job);

#endif
