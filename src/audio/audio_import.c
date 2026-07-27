#include "audio/audio_import.h"

#include "dsp/channel_mix.h"
#include "platform/file_io.h"

#include "raylib.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool extension_matches(const char *extension, const char *expected) {
    if (extension == NULL || expected == NULL) {
        return false;
    }
    while (*extension != '\0' && *expected != '\0') {
        if (tolower((unsigned char)*extension) != tolower((unsigned char)*expected)) {
            return false;
        }
        extension++;
        expected++;
    }
    return *extension == '\0' && *expected == '\0';
}

static void set_error(char *error, size_t error_size, const char *message) {
    if (error == NULL || error_size == 0U) {
        return;
    }
    snprintf(error, error_size, "%s", message);
}

static unsigned char *load_audio_bytes(const char *path,
                                       int *data_size,
                                       char *error,
                                       size_t error_size) {
    *data_size = 0;
    FILE *file = platform_fopen_utf8(path, "rb");
    if (file == NULL) {
        set_error(error,
                  error_size,
                  "Could not open the selected file. Check its location and permissions.");
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(error, error_size, "Could not measure the selected audio file.");
        return NULL;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || file_size > INT_MAX ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error,
                  error_size,
                  file_size > INT_MAX ? "The selected audio file is too large."
                                      : "The selected audio file is empty or unreadable.");
        return NULL;
    }

    unsigned char *bytes = (unsigned char *)malloc((size_t)file_size);
    if (bytes == NULL) {
        fclose(file);
        set_error(error, error_size, "Not enough memory to read this audio file.");
        return NULL;
    }
    size_t read_count = fread(bytes, 1U, (size_t)file_size, file);
    bool read_ok = read_count == (size_t)file_size && ferror(file) == 0;
    fclose(file);
    if (!read_ok) {
        free(bytes);
        set_error(error, error_size, "The selected audio file could not be read completely.");
        return NULL;
    }

    *data_size = (int)file_size;
    return bytes;
}

void imported_audio_init(ImportedAudio *audio) {
    if (audio == NULL) {
        return;
    }
    *audio = (ImportedAudio){0};
}

void imported_audio_unload(ImportedAudio *audio) {
    if (audio == NULL) {
        return;
    }
    interleaved_buffer_free(&audio->interleaved);
    sample_buffer_free(&audio->mono);
    audio->source_channels = 0U;
    audio->source_sample_size = 0U;
    audio->duration_seconds = 0.0f;
}

bool audio_file_is_supported(const char *path) {
    if (path == NULL) {
        return false;
    }
    const char *extension = strrchr(path, '.');
    return extension_matches(extension, ".wav") ||
           extension_matches(extension, ".mp3") ||
           extension_matches(extension, ".ogg") ||
           extension_matches(extension, ".flac");
}

bool imported_audio_load(const char *path, ImportedAudio *audio, char *error, size_t error_size) {
    if (path == NULL || audio == NULL) {
        set_error(error, error_size, "Invalid audio import request.");
        return false;
    }
    if (!audio_file_is_supported(path)) {
        set_error(error, error_size, "Unsupported file type. Choose WAV, MP3, OGG, or FLAC.");
        return false;
    }
    const char *extension = strrchr(path, '.');
    char file_type[8] = {0};
    size_t extension_length = extension != NULL ? strlen(extension) : 0U;
    if (extension_length == 0U || extension_length >= sizeof(file_type)) {
        set_error(error, error_size, "The selected audio file has an invalid extension.");
        return false;
    }
    for (size_t index = 0U; index < extension_length; index++) {
        file_type[index] = (char)tolower((unsigned char)extension[index]);
    }

    int data_size = 0;
    unsigned char *file_data =
        load_audio_bytes(path, &data_size, error, error_size);
    if (file_data == NULL) {
        return false;
    }
    Wave wave = LoadWaveFromMemory(file_type, file_data, data_size);
    free(file_data);
    if (wave.data == NULL || wave.frameCount == 0U || wave.sampleRate == 0U || wave.channels == 0U) {
        if (wave.data != NULL) {
            UnloadWave(wave);
        }
        set_error(error,
                  error_size,
                  "The file was opened, but its audio stream could not be decoded.");
        return false;
    }
    if ((size_t)wave.frameCount > SIZE_MAX / sizeof(float)) {
        UnloadWave(wave);
        set_error(error, error_size, "The decoded audio file is too large.");
        return false;
    }

    float *interleaved = LoadWaveSamples(wave);
    float *mono = (float *)calloc((size_t)wave.frameCount, sizeof(float));
    unsigned int preserved_channels = wave.channels <= 2U ? wave.channels : 1U;
    if ((size_t)wave.frameCount >
        SIZE_MAX / ((size_t)preserved_channels * sizeof(float))) {
        if (interleaved != NULL) {
            UnloadWaveSamples(interleaved);
        }
        free(mono);
        UnloadWave(wave);
        set_error(error, error_size, "The decoded audio file is too large.");
        return false;
    }
    size_t preserved_sample_count =
        (size_t)wave.frameCount * (size_t)preserved_channels;
    float *preserved =
        (float *)calloc(preserved_sample_count, sizeof(float));
    if (interleaved == NULL || mono == NULL || preserved == NULL) {
        if (interleaved != NULL) {
            UnloadWaveSamples(interleaved);
        }
        free(mono);
        free(preserved);
        UnloadWave(wave);
        set_error(error, error_size, "Not enough memory to load this audio file.");
        return false;
    }

    bool mixed = downmix_interleaved_to_mono(interleaved,
                                             (size_t)wave.frameCount,
                                             wave.channels,
                                             mono);
    if (!mixed) {
        UnloadWaveSamples(interleaved);
        free(mono);
        free(preserved);
        UnloadWave(wave);
        set_error(error, error_size, "Could not convert decoded audio to mono.");
        return false;
    }
    if (preserved_channels == wave.channels) {
        memcpy(preserved,
               interleaved,
               preserved_sample_count * sizeof(float));
    } else {
        memcpy(preserved,
               mono,
               (size_t)wave.frameCount * sizeof(float));
    }

    ImportedAudio loaded = {
        .interleaved = {
            .samples = preserved,
            .frame_count = (size_t)wave.frameCount,
            .sample_rate = wave.sampleRate,
            .channel_count = preserved_channels,
        },
        .mono = {
            .samples = mono,
            .count = (size_t)wave.frameCount,
            .sample_rate = wave.sampleRate,
        },
        .source_channels = wave.channels,
        .source_sample_size = wave.sampleSize,
        .duration_seconds = (float)wave.frameCount / (float)wave.sampleRate,
    };

    UnloadWaveSamples(interleaved);
    UnloadWave(wave);
    imported_audio_unload(audio);
    *audio = loaded;
    set_error(error, error_size, "");
    return true;
}
