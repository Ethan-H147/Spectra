#ifndef SPECTRA_WAVEFORM_SUMMARY_H
#define SPECTRA_WAVEFORM_SUMMARY_H

#include <stddef.h>

size_t summarize_waveform(const float *samples,
                          size_t sample_count,
                          float *minimums,
                          float *maximums,
                          size_t capacity);

#endif
