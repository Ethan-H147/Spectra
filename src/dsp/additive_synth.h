#ifndef SPECTRA_ADDITIVE_SYNTH_H
#define SPECTRA_ADDITIVE_SYNTH_H

#include "dsp/dsp_types.h"

SampleBuffer generate_sine_wave(float frequency,
                                float amplitude,
                                float duration_seconds,
                                unsigned int sample_rate);

SampleBuffer generate_additive_tone(float fundamental_frequency,
                                    const Harmonic *harmonics,
                                    int harmonic_count,
                                    float duration_seconds,
                                    unsigned int sample_rate,
                                    ADSREnvelope envelope);

void apply_adsr_envelope(float *samples, size_t count, unsigned int sample_rate, ADSREnvelope envelope);

#endif
