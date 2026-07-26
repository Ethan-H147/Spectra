#include "platform/file_io.h"

#include <stdlib.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

static wchar_t *utf8_to_wide(const char *text) {
    if (text == NULL) {
        return NULL;
    }

    int required = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (required <= 0) {
        return NULL;
    }
    wchar_t *wide = (wchar_t *)calloc((size_t)required, sizeof(wchar_t));
    if (wide == NULL) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, text, -1, wide, required) <= 0) {
        free(wide);
        return NULL;
    }
    return wide;
}

FILE *platform_fopen_utf8(const char *path, const char *mode) {
    wchar_t *wide_path = utf8_to_wide(path);
    wchar_t *wide_mode = utf8_to_wide(mode);
    if (wide_path == NULL || wide_mode == NULL) {
        free(wide_path);
        free(wide_mode);
        return NULL;
    }

    FILE *file = _wfopen(wide_path, wide_mode);
    free(wide_path);
    free(wide_mode);
    return file;
}

bool platform_remove_utf8(const char *path) {
    wchar_t *wide_path = utf8_to_wide(path);
    if (wide_path == NULL) {
        return false;
    }
    bool removed = _wremove(wide_path) == 0;
    free(wide_path);
    return removed;
}

#else

FILE *platform_fopen_utf8(const char *path, const char *mode) {
    return path != NULL && mode != NULL ? fopen(path, mode) : NULL;
}

bool platform_remove_utf8(const char *path) {
    return path != NULL && remove(path) == 0;
}

#endif
