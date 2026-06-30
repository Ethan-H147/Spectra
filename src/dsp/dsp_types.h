#ifndef SPECTRA_DSP_TYPES_H
#define SPECTRA_DSP_TYPES_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    float *samples;
    size_t count;
    unsigned int sample_rate;
} SampleBuffer;

typedef struct {
    float attack_seconds;
    float decay_seconds;
    float sustain_level;
    float release_seconds;
} ADSREnvelope;

typedef struct {
    int multiple;
    float amplitude;
    float phase;
} Harmonic;

typedef struct {
    float *frequencies;
    float *magnitudes;
    size_t count;
} Spectrum;

typedef struct {
    float frequency;
    float magnitude;
    float db;
} Peak;

void sample_buffer_free(SampleBuffer *buffer);
void spectrum_free(Spectrum *spectrum);

#endif
