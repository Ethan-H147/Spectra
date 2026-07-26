#ifndef SPECTRA_WINDOWS_APP_H
#define SPECTRA_WINDOWS_APP_H

#include <stdbool.h>
#include <stddef.h>

void windows_app_prepare_process(void);
void windows_app_apply_window_icon(void *window_handle);
bool windows_app_choose_audio_file(void *window_handle, char *path, size_t path_size);
bool windows_app_choose_wav_save_path(void *window_handle,
                                      const char *suggested_name,
                                      char *path,
                                      size_t path_size);

#endif
