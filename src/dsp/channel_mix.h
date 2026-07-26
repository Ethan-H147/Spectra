#ifndef SPECTRA_CHANNEL_MIX_H
#define SPECTRA_CHANNEL_MIX_H

#include <stdbool.h>
#include <stddef.h>

bool downmix_interleaved_to_mono(const float *interleaved,
                                 size_t frame_count,
                                 unsigned int channel_count,
                                 float *mono);

#endif
