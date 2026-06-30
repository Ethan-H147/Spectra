#ifndef SPECTRA_SIGNAL_UTILS_H
#define SPECTRA_SIGNAL_UTILS_H

#include <stddef.h>

float clampf(float value, float min_value, float max_value);
float peak_amplitude(const float *samples, size_t count);
void normalize_samples(float *samples, size_t count, float target_peak);
float linear_to_db(float value);
unsigned int next_power_of_two(unsigned int value);

#endif
