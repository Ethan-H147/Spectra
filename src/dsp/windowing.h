#ifndef SPECTRA_WINDOWING_H
#define SPECTRA_WINDOWING_H

#include <stddef.h>

void hann_window(float *window, size_t size);
void apply_window(const float *samples, const float *window, float *output, size_t size);

#endif
