#ifndef SPECTRA_HARMONIC_ANALYSIS_H
#define SPECTRA_HARMONIC_ANALYSIS_H

#include "dsp/dsp_types.h"

typedef struct {
    int harmonic_number;
    float expected_frequency;
    float detected_frequency;
    float amplitude;
    float source_magnitude;
    float db;
    bool detected;
} ExtractedHarmonic;

int find_peaks(const Spectrum *spectrum,
               float min_frequency,
               float max_frequency,
               float threshold_db,
               Peak *peaks,
               int max_peaks);

int find_interpolated_peaks(const Spectrum *spectrum,
                            float min_frequency,
                            float max_frequency,
                            float threshold_db,
                            Peak *peaks,
                            int max_peaks);

int extract_harmonics(const Spectrum *spectrum,
                      float fundamental_frequency,
                      int harmonic_count,
                      float tolerance_cents,
                      float threshold_db,
                      ExtractedHarmonic *harmonics,
                      int capacity);

#endif
