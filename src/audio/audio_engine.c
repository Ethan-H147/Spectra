#include "audio/audio_engine.h"

#include "dsp/signal_utils.h"
#include "platform/file_io.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int16_t float_to_pcm16(float value) {
    float clipped = clampf(value, -1.0f, 1.0f);
    return (int16_t)(clipped < 0.0f ? clipped * 32768.0f : clipped * 32767.0f);
}

static void write_u16_le(FILE *file, uint16_t value) {
    fputc(value & 0xff, file);
    fputc((value >> 8) & 0xff, file);
}

static void write_u32_le(FILE *file, uint32_t value) {
    fputc(value & 0xff, file);
    fputc((value >> 8) & 0xff, file);
    fputc((value >> 16) & 0xff, file);
    fputc((value >> 24) & 0xff, file);
}

static void store_u16_le(unsigned char *bytes, uint16_t value) {
    bytes[0] = (unsigned char)(value & 0xffU);
    bytes[1] = (unsigned char)((value >> 8U) & 0xffU);
}

static void store_u32_le(unsigned char *bytes, uint32_t value) {
    bytes[0] = (unsigned char)(value & 0xffU);
    bytes[1] = (unsigned char)((value >> 8U) & 0xffU);
    bytes[2] = (unsigned char)((value >> 16U) & 0xffU);
    bytes[3] = (unsigned char)((value >> 24U) & 0xffU);
}

void audio_clip_init(AudioClip *clip) {
    if (clip == NULL) {
        return;
    }
    *clip = (AudioClip){0};
}

bool audio_clip_set_interleaved(AudioClip *clip,
                                const InterleavedBuffer *buffer) {
    if (clip == NULL || buffer == NULL || buffer->samples == NULL ||
        buffer->frame_count == 0U || buffer->sample_rate == 0U ||
        buffer->channel_count == 0U ||
        buffer->channel_count > UINT16_MAX ||
        buffer->frame_count > UINT_MAX ||
        buffer->frame_count >
            SIZE_MAX / (size_t)buffer->channel_count) {
        return false;
    }
    const uint64_t byte_rate =
        (uint64_t)buffer->sample_rate *
        (uint64_t)buffer->channel_count *
        sizeof(int16_t);
    if (byte_rate > UINT32_MAX) {
        return false;
    }

    audio_clip_unload(clip);

    const size_t sample_count =
        buffer->frame_count * (size_t)buffer->channel_count;
    if (sample_count >
            (UINT32_MAX - 36U) / sizeof(int16_t) ||
        sample_count >
            (size_t)(INT_MAX - 44) / sizeof(int16_t)) {
        return false;
    }
    const uint32_t data_bytes =
        (uint32_t)(sample_count * sizeof(int16_t));
    const size_t encoded_size = 44U + (size_t)data_bytes;
    unsigned char *encoded =
        (unsigned char *)calloc(encoded_size, 1U);
    if (encoded == NULL) {
        return false;
    }

    memcpy(encoded, "RIFF", 4U);
    store_u32_le(encoded + 4U, 36U + data_bytes);
    memcpy(encoded + 8U, "WAVEfmt ", 8U);
    store_u32_le(encoded + 16U, 16U);
    store_u16_le(encoded + 20U, 1U);
    store_u16_le(
        encoded + 22U,
        (uint16_t)buffer->channel_count);
    store_u32_le(encoded + 24U, buffer->sample_rate);
    store_u32_le(
        encoded + 28U,
        (uint32_t)byte_rate);
    store_u16_le(
        encoded + 32U,
        (uint16_t)(buffer->channel_count *
                   (unsigned int)sizeof(int16_t)));
    store_u16_le(encoded + 34U, 16U);
    memcpy(encoded + 36U, "data", 4U);
    store_u32_le(encoded + 40U, data_bytes);
    for (size_t i = 0; i < sample_count; i++) {
        store_u16_le(
            encoded + 44U + i * sizeof(int16_t),
            (uint16_t)float_to_pcm16(buffer->samples[i]));
    }

    clip->music = LoadMusicStreamFromMemory(
        ".wav",
        encoded,
        (int)encoded_size);
    clip->loaded = IsMusicValid(clip->music);
    if (clip->loaded) {
        clip->encoded_data = encoded;
        clip->encoded_size = (int)encoded_size;
        clip->duration_seconds =
            (float)buffer->frame_count /
            (float)buffer->sample_rate;
        clip->music.looping = false;
        SetMusicVolume(clip->music, 1.0f);
    } else {
        free(encoded);
    }
    return clip->loaded;
}

bool audio_clip_set_samples(AudioClip *clip, const SampleBuffer *buffer) {
    if (buffer == NULL) {
        return false;
    }
    InterleavedBuffer interleaved = {
        .samples = buffer->samples,
        .frame_count = buffer->count,
        .sample_rate = buffer->sample_rate,
        .channel_count = 1U,
    };
    return audio_clip_set_interleaved(clip, &interleaved);
}

void audio_clip_set_looping(AudioClip *clip, bool looping) {
    if (clip == NULL || !clip->loaded) {
        return;
    }
    clip->music.looping = looping;
}

bool audio_clip_play(AudioClip *clip) {
    if (clip == NULL || !clip->loaded) {
        return false;
    }

    StopMusicStream(clip->music);
    SetMusicVolume(clip->music, 1.0f);
    PlayMusicStream(clip->music);
    clip->paused = false;
    return true;
}

bool audio_clip_pause(AudioClip *clip) {
    if (clip == NULL || !clip->loaded || clip->paused ||
        !IsMusicStreamPlaying(clip->music)) {
        return false;
    }

    PauseMusicStream(clip->music);
    clip->paused = true;
    return true;
}

bool audio_clip_resume(AudioClip *clip) {
    if (clip == NULL || !clip->loaded || !clip->paused) {
        return false;
    }

    if (audio_clip_position_seconds(clip) >=
        clip->duration_seconds - 0.001f) {
        SeekMusicStream(clip->music, 0.0f);
    }
    ResumeMusicStream(clip->music);
    clip->paused = false;
    return true;
}

void audio_clip_stop(AudioClip *clip) {
    if (clip == NULL || !clip->loaded) {
        return;
    }

    StopMusicStream(clip->music);
    clip->paused = false;
}

bool audio_clip_seek(AudioClip *clip, float position_seconds) {
    if (clip == NULL || !clip->loaded) {
        return false;
    }

    const float position =
        clampf(position_seconds, 0.0f, clip->duration_seconds);
    const bool was_playing = audio_clip_is_playing(clip);
    const bool was_paused = audio_clip_is_paused(clip);
    if (!was_playing && !was_paused) {
        PlayMusicStream(clip->music);
    }
    SeekMusicStream(clip->music, position);
    if (was_playing) {
        clip->paused = false;
    } else {
        PauseMusicStream(clip->music);
        clip->paused = true;
    }
    return true;
}

void audio_clip_update(AudioClip *clip) {
    if (clip != NULL && clip->loaded && !clip->paused &&
        IsMusicStreamPlaying(clip->music)) {
        UpdateMusicStream(clip->music);
    }
}

bool audio_clip_is_playing(const AudioClip *clip) {
    return clip != NULL && clip->loaded && !clip->paused &&
           IsMusicStreamPlaying(clip->music);
}

bool audio_clip_is_paused(const AudioClip *clip) {
    return clip != NULL && clip->loaded && clip->paused;
}

bool audio_clip_is_active(const AudioClip *clip) {
    return audio_clip_is_playing(clip) || audio_clip_is_paused(clip);
}

float audio_clip_position_seconds(const AudioClip *clip) {
    if (clip == NULL || !clip->loaded) {
        return 0.0f;
    }
    if (!clip->paused &&
        !IsMusicStreamPlaying(clip->music)) {
        return 0.0f;
    }

    float position = GetMusicTimePlayed(clip->music);
    if (position < 0.0f) position = 0.0f;
    if (position > clip->duration_seconds) position = clip->duration_seconds;
    return position;
}

float audio_clip_duration_seconds(const AudioClip *clip) {
    return clip != NULL && clip->loaded ? clip->duration_seconds : 0.0f;
}

void audio_clip_unload(AudioClip *clip) {
    if (clip == NULL) {
        return;
    }

    if (clip->loaded) {
        UnloadMusicStream(clip->music);
    }
    free(clip->encoded_data);
    *clip = (AudioClip){0};
}

bool export_interleaved_to_wav(const char *path,
                               const InterleavedBuffer *buffer) {
    if (path == NULL || buffer == NULL || buffer->samples == NULL ||
        buffer->frame_count == 0U || buffer->sample_rate == 0U ||
        buffer->channel_count == 0U ||
        buffer->channel_count > UINT16_MAX ||
        buffer->frame_count >
            SIZE_MAX / (size_t)buffer->channel_count) {
        return false;
    }
    size_t sample_count =
        buffer->frame_count * (size_t)buffer->channel_count;
    if (sample_count >
        (UINT32_MAX - 36U) / sizeof(int16_t)) {
        return false;
    }
    uint64_t byte_rate =
        (uint64_t)buffer->sample_rate *
        (uint64_t)buffer->channel_count *
        sizeof(int16_t);
    if (byte_rate > UINT32_MAX) return false;

    FILE *file = platform_fopen_utf8(path, "wb");
    if (file == NULL) {
        return false;
    }

    uint16_t channels = (uint16_t)buffer->channel_count;
    uint16_t bits_per_sample = 16;
    uint32_t data_bytes =
        (uint32_t)(sample_count * sizeof(int16_t));
    uint16_t block_align = channels * bits_per_sample / 8;

    fwrite("RIFF", 1, 4, file);
    write_u32_le(file, 36 + data_bytes);
    fwrite("WAVE", 1, 4, file);
    fwrite("fmt ", 1, 4, file);
    write_u32_le(file, 16);
    write_u16_le(file, 1);
    write_u16_le(file, channels);
    write_u32_le(file, buffer->sample_rate);
    write_u32_le(file, (uint32_t)byte_rate);
    write_u16_le(file, block_align);
    write_u16_le(file, bits_per_sample);
    fwrite("data", 1, 4, file);
    write_u32_le(file, data_bytes);

    for (size_t i = 0; i < sample_count; i++) {
        write_u16_le(file, (uint16_t)float_to_pcm16(buffer->samples[i]));
    }

    bool written = ferror(file) == 0;
    if (fclose(file) != 0) {
        written = false;
    }
    return written;
}

bool export_samples_to_wav(const char *path, const SampleBuffer *buffer) {
    if (buffer == NULL) {
        return false;
    }
    InterleavedBuffer interleaved = {
        .samples = buffer->samples,
        .frame_count = buffer->count,
        .sample_rate = buffer->sample_rate,
        .channel_count = 1U,
    };
    return export_interleaved_to_wav(path, &interleaved);
}

