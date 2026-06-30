#ifndef SPECTRA_AUDIO_ENGINE_H
#define SPECTRA_AUDIO_ENGINE_H

#include "dsp/dsp_types.h"
#include "raylib.h"

#include <stdbool.h>

typedef struct {
    Sound sound;
    bool loaded;
} AudioClip;

void audio_clip_init(AudioClip *clip);
bool audio_clip_set_samples(AudioClip *clip, const SampleBuffer *buffer);
bool audio_clip_play(AudioClip *clip);
void audio_clip_stop(AudioClip *clip);
void audio_clip_unload(AudioClip *clip);
bool export_samples_to_wav(const char *path, const SampleBuffer *buffer);

#endif

