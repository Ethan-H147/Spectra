#ifndef SPECTRA_STFT_RECONSTRUCTION_H
#define SPECTRA_STFT_RECONSTRUCTION_H

#include "dsp/dsp_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    float *db_values;
    unsigned int time_bins;
    unsigned int frequency_bins;
    float maximum_frequency;
    float duration_seconds;
} SpectrogramData;

typedef struct {
    unsigned int bin;
    float power;
} StftBinRank;

typedef struct {
    const float *source_samples;
    size_t source_count;
    unsigned int sample_rate;
    unsigned int window_size;
    unsigned int hop_size;
    size_t frame_count;
    size_t next_frame;
    int top_component_count;
    bool include_spectrogram;
    bool active;
    bool complete;
    bool failed;
    float window_sum;
    double total_spectral_energy;
    double retained_spectral_energy;
    float *window;
    float *real;
    float *imaginary;
    float *overlap_weights;
    unsigned char *kept_bins;
    StftBinRank *ranked_bins;
    SampleBuffer output;
    SpectrogramData spectrogram;
} StftReconstructionJob;

void spectrogram_data_free(SpectrogramData *spectrogram);
void stft_reconstruction_job_init(StftReconstructionJob *job);
void stft_reconstruction_job_free(StftReconstructionJob *job);
bool stft_reconstruction_job_begin(StftReconstructionJob *job,
                                   const SampleBuffer *source,
                                   unsigned int window_size,
                                   unsigned int hop_size,
                                   int top_component_count,
                                   bool include_spectrogram,
                                   unsigned int spectrogram_time_bins,
                                   unsigned int spectrogram_frequency_bins,
                                   float spectrogram_maximum_frequency);
size_t stft_reconstruction_job_process(StftReconstructionJob *job, size_t maximum_frames);
float stft_reconstruction_job_progress(const StftReconstructionJob *job);
float stft_reconstruction_job_retained_energy(const StftReconstructionJob *job);
SampleBuffer stft_reconstruction_job_take_output(StftReconstructionJob *job);
SpectrogramData stft_reconstruction_job_take_spectrogram(StftReconstructionJob *job);

#endif
