#ifndef SPECTRA_HARMONIC_ANALYSIS_H
#define SPECTRA_HARMONIC_ANALYSIS_H

#include "dsp/dsp_types.h"

int find_peaks(const Spectrum *spectrum,
               float min_frequency,
               float max_frequency,
               float threshold_db,
               Peak *peaks,
               int max_peaks);

#endif
