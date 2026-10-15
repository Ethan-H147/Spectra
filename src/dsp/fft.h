#ifndef SPECTRA_FFT_H
#define SPECTRA_FFT_H

#include "dsp/dsp_types.h"

Spectrum compute_magnitude_spectrum(const float *samples, size_t sample_count, unsigned int sample_rate);
Spectrum compute_averaged_magnitude_spectrum(
    const float *samples,
    size_t sample_count,
    unsigned int sample_rate);

#endif
