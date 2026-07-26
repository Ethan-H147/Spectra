#ifndef SPECTRA_PLATFORM_FILE_IO_H
#define SPECTRA_PLATFORM_FILE_IO_H

#include <stdbool.h>
#include <stdio.h>

FILE *platform_fopen_utf8(const char *path, const char *mode);
bool platform_remove_utf8(const char *path);

#endif
