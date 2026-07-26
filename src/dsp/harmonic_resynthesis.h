#ifndef SPECTRA_HARMONIC_RESYNTHESIS_H
#define SPECTRA_HARMONIC_RESYNTHESIS_H

#include "dsp/dsp_types.h"
#include "dsp/harmonic_analysis.h"

SampleBuffer resynthesize_from_harmonics(float fundamental_frequency,
                                         const ExtractedHarmonic *harmonics,
                                         int harmonic_count,
                                         float duration_seconds,
                                         unsigned int sample_rate);

#endif
