#ifndef SPECTRA_SPECTRAL_EFFECTS_H
#define SPECTRA_SPECTRAL_EFFECTS_H

#include "dsp/dsp_types.h"

#include <stdbool.h>

typedef bool (*SpectralEffectProgressCallback)(float progress,
                                               void *context);

typedef enum {
    SPECTRAL_EQ_SHAPE_RANGE = 0,
    SPECTRAL_EQ_SHAPE_BELL = 1,
    SPECTRAL_EQ_SHAPE_LOW_SHELF = 2,
    SPECTRAL_EQ_SHAPE_HIGH_SHELF = 3,
    SPECTRAL_EQ_SHAPE_COUNT = 4
} SpectralEqBandShape;

typedef struct {
    float low_hz;
    float high_hz;
    float gain_db;
    bool enabled;
    SpectralEqBandShape shape;
} SpectralEqBand;

float spectral_eq_band_response_db(
    float frequency,
    const SpectralEqBand *band);

float spectral_eq_response_db(
    float frequency,
    const SpectralEqBand *bands,
    size_t band_count);

bool playback_rate_pitch_shift(
    const InterleavedBuffer *source,
    float pitch_factor,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context);

bool phase_vocoder_pitch_shift(
    const InterleavedBuffer *source,
    float pitch_factor,
    unsigned int window_size,
    unsigned int analysis_hop,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context);

bool analytic_frequency_shift(
    const InterleavedBuffer *source,
    float shift_hz,
    unsigned int window_size,
    unsigned int hop_size,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context);

bool spectral_range_equalize(
    const InterleavedBuffer *source,
    const SpectralEqBand *bands,
    size_t band_count,
    unsigned int window_size,
    unsigned int hop_size,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context);

#endif
