#ifndef SPECTRA_AUDIO_IMPORT_H
#define SPECTRA_AUDIO_IMPORT_H

#include "dsp/dsp_types.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    InterleavedBuffer interleaved;
    SampleBuffer mono;
    unsigned int source_channels;
    unsigned int source_sample_size;
    float duration_seconds;
} ImportedAudio;

void imported_audio_init(ImportedAudio *audio);
void imported_audio_unload(ImportedAudio *audio);
bool audio_file_is_supported(const char *path);
bool imported_audio_load(const char *path, ImportedAudio *audio, char *error, size_t error_size);

#endif
