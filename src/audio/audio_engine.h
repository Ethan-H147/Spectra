#ifndef SPECTRA_AUDIO_ENGINE_H
#define SPECTRA_AUDIO_ENGINE_H

#include "dsp/dsp_types.h"
#include "raylib.h"

#include <stdbool.h>

typedef struct {
    Sound sound;
    bool loaded;
    bool paused;
    float duration_seconds;
    float paused_position_seconds;
    double playback_started_at;
} AudioClip;

void audio_clip_init(AudioClip *clip);
bool audio_clip_set_samples(AudioClip *clip, const SampleBuffer *buffer);
bool audio_clip_play(AudioClip *clip);
bool audio_clip_pause(AudioClip *clip);
bool audio_clip_resume(AudioClip *clip);
void audio_clip_stop(AudioClip *clip);
bool audio_clip_is_playing(const AudioClip *clip);
bool audio_clip_is_paused(const AudioClip *clip);
bool audio_clip_is_active(const AudioClip *clip);
float audio_clip_position_seconds(const AudioClip *clip);
float audio_clip_duration_seconds(const AudioClip *clip);
void audio_clip_unload(AudioClip *clip);
bool export_samples_to_wav(const char *path, const SampleBuffer *buffer);

#endif

