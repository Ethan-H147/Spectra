#ifndef SPECTRA_PITCH_DETECTION_H
#define SPECTRA_PITCH_DETECTION_H

#include "dsp/dsp_types.h"

#include <stdbool.h>

typedef struct {
    bool valid;
    float frequency_hz;
    float confidence;
    int midi_note;
    float cents;
} PitchEstimate;

PitchEstimate estimate_pitch(const SampleBuffer *buffer, float min_frequency, float max_frequency);
const char *pitch_note_name(int midi_note);
int pitch_note_octave(int midi_note);

#endif
