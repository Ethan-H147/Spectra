#ifndef SPECTRA_DSP_FFT_PLAN_H
#define SPECTRA_DSP_FFT_PLAN_H

#include <stdbool.h>

/*
 * Precomputed radix-2 FFT tables for one power-of-two size.
 *
 * The twiddle table stores every stage contiguously: the stage with block
 * size B occupies indices [B/2 - 1, B - 1). A half-size transform therefore
 * reuses the same table, which the real-input transforms rely on.
 */
typedef struct {
    unsigned int size;
    unsigned int *reversed_indices;
    unsigned int *half_reversed_indices;
    float *twiddle_real;
    float *twiddle_imaginary;
} SpectraFftPlan;

bool spectra_fft_plan_init(
    SpectraFftPlan *plan,
    unsigned int size);
void spectra_fft_plan_free(SpectraFftPlan *plan);

/* In-place complex forward transform. */
void spectra_fft_forward(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary);

/* In-place complex inverse transform, scaled by 1 / size. */
void spectra_fft_inverse(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary);

/*
 * Forward transform of real input stored in real[0, size). The input in
 * imaginary is ignored. On return both arrays hold the complete conjugate-
 * symmetric spectrum, matching spectra_fft_forward with a zero imaginary
 * input, at roughly half the cost.
 */
void spectra_fft_forward_real(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary);

/*
 * Inverse transform of a conjugate-symmetric spectrum. Only bins
 * [0, size / 2] are read, and the imaginary parts of the DC and Nyquist bins
 * are ignored. On return real[0, size) holds the scaled time-domain signal,
 * matching the real part of spectra_fft_inverse. The contents of imaginary
 * are unspecified afterwards.
 */
void spectra_fft_inverse_real(
    const SpectraFftPlan *plan,
    float *real,
    float *imaginary);

#endif
