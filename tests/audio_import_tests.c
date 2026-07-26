#include "audio/audio_import.h"
#include "platform/file_io.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

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

static bool write_test_wave(const char *path) {
    FILE *file = platform_fopen_utf8(path, "wb");
    if (file == NULL) {
        return false;
    }

    const int16_t samples[] = {0, 1200, -1200, 0};
    const uint32_t data_size = (uint32_t)sizeof(samples);
    fwrite("RIFF", 1U, 4U, file);
    write_u32_le(file, 36U + data_size);
    fwrite("WAVEfmt ", 1U, 8U, file);
    write_u32_le(file, 16U);
    write_u16_le(file, 1U);
    write_u16_le(file, 1U);
    write_u32_le(file, 8000U);
    write_u32_le(file, 16000U);
    write_u16_le(file, 2U);
    write_u16_le(file, 16U);
    fwrite("data", 1U, 4U, file);
    write_u32_le(file, data_size);
    for (size_t index = 0U; index < sizeof(samples) / sizeof(samples[0]); index++) {
        write_u16_le(file, (uint16_t)samples[index]);
    }

    bool written = ferror(file) == 0;
    if (fclose(file) != 0) written = false;
    return written;
}

static bool verify_import(const char *path, bool print_details) {
    ImportedAudio audio;
    imported_audio_init(&audio);
    char error[256] = {0};
    bool loaded =
        imported_audio_load(path, &audio, error, sizeof(error));
    if (!loaded) {
        fprintf(stderr, "Import failed: %s\n", error);
        return false;
    }
    if (audio.mono.samples == NULL || audio.mono.count == 0U ||
        audio.mono.sample_rate == 0U || audio.source_channels == 0U) {
        fprintf(stderr, "Import returned incomplete audio metadata.\n");
        imported_audio_unload(&audio);
        return false;
    }
    if (print_details) {
        printf("Imported: %zu frames, %u Hz, %u channel(s), %.2f seconds\n",
               audio.mono.count,
               audio.mono.sample_rate,
               audio.source_channels,
               audio.duration_seconds);
    }
    imported_audio_unload(&audio);
    return true;
}

static int run_unicode_filename_regression(void) {
    const char unicode_path[] =
        "spectra-\xE9\x9F\xB3\xE9\xA2\x91-import-test.wav";
    if (!write_test_wave(unicode_path)) {
        fprintf(stderr, "Could not create the Unicode-path test WAV.\n");
        return 1;
    }

    bool imported = verify_import(unicode_path, false);
    bool removed = platform_remove_utf8(unicode_path);
    if (!imported || !removed) {
        fprintf(stderr, "Unicode-path audio import regression failed.\n");
        return 1;
    }
    printf("Unicode-path audio import test passed.\n");
    return 0;
}

#if defined(_WIN32)

static char *wide_to_utf8(const wchar_t *wide) {
    int required =
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (required <= 0) return NULL;
    char *utf8 = (char *)calloc((size_t)required, 1U);
    if (utf8 == NULL) return NULL;
    if (WideCharToMultiByte(
            CP_UTF8, 0, wide, -1, utf8, required, NULL, NULL) <= 0) {
        free(utf8);
        return NULL;
    }
    return utf8;
}

int wmain(int argc, wchar_t **argv) {
    if (argc <= 1) {
        return run_unicode_filename_regression();
    }
    for (int index = 1; index < argc; index++) {
        char *path = wide_to_utf8(argv[index]);
        if (path == NULL || !verify_import(path, true)) {
            free(path);
            return 1;
        }
        free(path);
    }
    return 0;
}

#else

int main(int argc, char **argv) {
    if (argc <= 1) {
        return run_unicode_filename_regression();
    }
    for (int index = 1; index < argc; index++) {
        if (!verify_import(argv[index], true)) return 1;
    }
    return 0;
}

#endif
