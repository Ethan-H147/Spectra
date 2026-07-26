#ifndef SPECTRA_FOURIER_RECONSTRUCTION_H
#define SPECTRA_FOURIER_RECONSTRUCTION_H

#include "dsp/dsp_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    unsigned int bin_index;
    float frequency;
    float magnitude;
    float db;
    float phase;
    float real;
    float imaginary;
} FourierComponent;

typedef struct {
    SampleBuffer windowed_frame;
    FourierComponent *ranked_components;
    size_t component_count;
    unsigned int fft_size;
    float dc_real;
    float nyquist_real;
} FourierFrameAnalysis;

void fourier_frame_analysis_init(FourierFrameAnalysis *analysis);
void fourier_frame_analysis_free(FourierFrameAnalysis *analysis);
bool analyze_fourier_frame(const SampleBuffer *region, FourierFrameAnalysis *analysis);
SampleBuffer reconstruct_fourier_frame(const FourierFrameAnalysis *analysis, size_t component_count);

#endif
