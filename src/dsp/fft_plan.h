#ifndef SPECTRA_DSP_FFT_PLAN_H
#define SPECTRA_DSP_FFT_PLAN_H

#include <stdbool.h>

typedef struct {
    unsigned int size;
    unsigned int *reversed_indices;
    float *twiddle_real;
    float *twiddle_imaginary;
} SpectraFftPlan;

bool spectra_fft_plan_init(
    SpectraFftPlan *plan,
    unsigned int size);
void spectra_fft_plan_free(SpectraFftPlan *plan);
void spectra_fft_forward(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary);

#endif
