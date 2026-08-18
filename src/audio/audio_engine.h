#ifndef SPECTRA_AUDIO_ENGINE_H
#define SPECTRA_AUDIO_ENGINE_H

#include "dsp/dsp_types.h"
#include "raylib.h"

#include <stdbool.h>

typedef struct {
    Music music;
    unsigned char *encoded_data;
    int encoded_size;
    bool loaded;
    bool paused;
    float duration_seconds;
} AudioClip;

void audio_clip_init(AudioClip *clip);
bool audio_clip_set_interleaved(AudioClip *clip,
                                const InterleavedBuffer *buffer);
bool audio_clip_set_samples(AudioClip *clip, const SampleBuffer *buffer);
void audio_clip_set_looping(AudioClip *clip, bool looping);
bool audio_clip_play(AudioClip *clip);
bool audio_clip_pause(AudioClip *clip);
bool audio_clip_resume(AudioClip *clip);
void audio_clip_stop(AudioClip *clip);
bool audio_clip_seek(AudioClip *clip, float position_seconds);
void audio_clip_update(AudioClip *clip);
bool audio_clip_is_playing(const AudioClip *clip);
bool audio_clip_is_paused(const AudioClip *clip);
bool audio_clip_is_active(const AudioClip *clip);
float audio_clip_position_seconds(const AudioClip *clip);
float audio_clip_duration_seconds(const AudioClip *clip);
void audio_clip_unload(AudioClip *clip);
bool export_interleaved_to_wav(const char *path,
                               const InterleavedBuffer *buffer);
bool export_samples_to_wav(const char *path, const SampleBuffer *buffer);

#endif

