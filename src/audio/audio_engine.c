#include "audio/audio_engine.h"

#include "dsp/signal_utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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

void audio_clip_init(AudioClip *clip) {
    if (clip == NULL) {
        return;
    }
    clip->loaded = false;
}

bool audio_clip_set_samples(AudioClip *clip, const SampleBuffer *buffer) {
    if (clip == NULL || buffer == NULL || buffer->samples == NULL || buffer->count == 0) {
        return false;
    }

    audio_clip_unload(clip);

    int16_t *pcm = (int16_t *)calloc(buffer->count, sizeof(int16_t));
    if (pcm == NULL) {
        return false;
    }

    for (size_t i = 0; i < buffer->count; i++) {
        pcm[i] = float_to_pcm16(buffer->samples[i]);
    }

    Wave wave = {0};
    wave.frameCount = (unsigned int)buffer->count;
    wave.sampleRate = buffer->sample_rate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = pcm;

    clip->sound = LoadSoundFromWave(wave);
    clip->loaded = clip->sound.frameCount > 0;
    if (clip->loaded) {
        SetSoundVolume(clip->sound, 1.0f);
    }
    free(pcm);
    return clip->loaded;
}

bool audio_clip_play(AudioClip *clip) {
    if (clip == NULL || !clip->loaded) {
        return false;
    }

    StopSound(clip->sound);
    SetSoundVolume(clip->sound, 1.0f);
    PlaySound(clip->sound);
    return true;
}

void audio_clip_stop(AudioClip *clip) {
    if (clip == NULL || !clip->loaded) {
        return;
    }

    StopSound(clip->sound);
}

void audio_clip_unload(AudioClip *clip) {
    if (clip == NULL || !clip->loaded) {
        return;
    }

    UnloadSound(clip->sound);
    clip->loaded = false;
}

bool export_samples_to_wav(const char *path, const SampleBuffer *buffer) {
    if (path == NULL || buffer == NULL || buffer->samples == NULL || buffer->count == 0) {
        return false;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return false;
    }

    uint16_t channels = 1;
    uint16_t bits_per_sample = 16;
    uint32_t data_bytes = (uint32_t)(buffer->count * sizeof(int16_t));
    uint32_t byte_rate = buffer->sample_rate * channels * bits_per_sample / 8;
    uint16_t block_align = channels * bits_per_sample / 8;

    fwrite("RIFF", 1, 4, file);
    write_u32_le(file, 36 + data_bytes);
    fwrite("WAVE", 1, 4, file);
    fwrite("fmt ", 1, 4, file);
    write_u32_le(file, 16);
    write_u16_le(file, 1);
    write_u16_le(file, channels);
    write_u32_le(file, buffer->sample_rate);
    write_u32_le(file, byte_rate);
    write_u16_le(file, block_align);
    write_u16_le(file, bits_per_sample);
    fwrite("data", 1, 4, file);
    write_u32_le(file, data_bytes);

    for (size_t i = 0; i < buffer->count; i++) {
        write_u16_le(file, (uint16_t)float_to_pcm16(buffer->samples[i]));
    }

    fclose(file);
    return true;
}

