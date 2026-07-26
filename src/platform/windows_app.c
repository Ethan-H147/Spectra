#include "platform/windows_app.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>

#define IDI_SPECTRA_ICON 101

static HICON g_large_icon = NULL;
static HICON g_small_icon = NULL;

static bool wide_path_to_utf8(const wchar_t *wide_path,
                              char *path,
                              size_t path_size) {
    int required =
        WideCharToMultiByte(CP_UTF8, 0, wide_path, -1, NULL, 0, NULL, NULL);
    if (required <= 0 || (size_t)required > path_size) {
        return false;
    }
    return WideCharToMultiByte(
               CP_UTF8, 0, wide_path, -1, path, (int)path_size, NULL, NULL) >
           0;
}

void windows_app_prepare_process(void) {
    SetCurrentProcessExplicitAppUserModelID(L"Spectra.AudioLab.Desktop");
}

void windows_app_apply_window_icon(void *window_handle) {
    HWND window = (HWND)window_handle;
    HINSTANCE instance = GetModuleHandleW(NULL);
    if (window == NULL || instance == NULL) {
        return;
    }

    g_large_icon = (HICON)LoadImageW(instance,
                                    MAKEINTRESOURCEW(IDI_SPECTRA_ICON),
                                    IMAGE_ICON,
                                    GetSystemMetrics(SM_CXICON),
                                    GetSystemMetrics(SM_CYICON),
                                    LR_DEFAULTCOLOR);
    g_small_icon = (HICON)LoadImageW(instance,
                                    MAKEINTRESOURCEW(IDI_SPECTRA_ICON),
                                    IMAGE_ICON,
                                    GetSystemMetrics(SM_CXSMICON),
                                    GetSystemMetrics(SM_CYSMICON),
                                    LR_DEFAULTCOLOR);

    if (g_large_icon != NULL) {
        SendMessageW(window, WM_SETICON, ICON_BIG, (LPARAM)g_large_icon);
        SetClassLongPtrW(window, GCLP_HICON, (LONG_PTR)g_large_icon);
    }
    if (g_small_icon != NULL) {
        SendMessageW(window, WM_SETICON, ICON_SMALL, (LPARAM)g_small_icon);
        SetClassLongPtrW(window, GCLP_HICONSM, (LONG_PTR)g_small_icon);
    }
    RedrawWindow(window, NULL, NULL, RDW_FRAME | RDW_INVALIDATE);
}

bool windows_app_choose_audio_file(void *window_handle, char *path, size_t path_size) {
    if (path == NULL || path_size == 0U) {
        return false;
    }

    wchar_t selected[32768] = {0};
    static const wchar_t FILTER[] =
        L"Audio files (*.wav;*.mp3;*.ogg;*.flac)\0*.wav;*.mp3;*.ogg;*.flac\0"
        L"WAV files (*.wav)\0*.wav\0"
        L"MP3 files (*.mp3)\0*.mp3\0"
        L"OGG files (*.ogg)\0*.ogg\0"
        L"FLAC files (*.flac)\0*.flac\0"
        L"All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog = {0};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = (HWND)window_handle;
    dialog.lpstrFilter = FILTER;
    dialog.lpstrFile = selected;
    dialog.nMaxFile = (DWORD)(sizeof(selected) / sizeof(selected[0]));
    dialog.lpstrTitle = L"Import audio into Spectra";
    dialog.lpstrDefExt = L"wav";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&dialog)) {
        return false;
    }

    return wide_path_to_utf8(selected, path, path_size);
}

bool windows_app_choose_wav_save_path(void *window_handle,
                                      const char *suggested_name,
                                      char *path,
                                      size_t path_size) {
    if (path == NULL || path_size == 0U) {
        return false;
    }

    wchar_t selected[32768] = {0};
    if (suggested_name != NULL && suggested_name[0] != '\0') {
        int converted = MultiByteToWideChar(CP_UTF8,
                                            0,
                                            suggested_name,
                                            -1,
                                            selected,
                                            (int)(sizeof(selected) /
                                                  sizeof(selected[0])));
        if (converted <= 0) {
            selected[0] = L'\0';
        }
    }

    static const wchar_t FILTER[] =
        L"WAV audio (*.wav)\0*.wav\0"
        L"All files (*.*)\0*.*\0\0";

    OPENFILENAMEW dialog = {0};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = (HWND)window_handle;
    dialog.lpstrFilter = FILTER;
    dialog.lpstrFile = selected;
    dialog.nMaxFile = (DWORD)(sizeof(selected) / sizeof(selected[0]));
    dialog.lpstrTitle = L"Export WAV from Spectra";
    dialog.lpstrDefExt = L"wav";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT |
                   OFN_NOCHANGEDIR;

    if (!GetSaveFileNameW(&dialog)) {
        return false;
    }
    return wide_path_to_utf8(selected, path, path_size);
}

#else

void windows_app_prepare_process(void) {
}

void windows_app_apply_window_icon(void *window_handle) {
    (void)window_handle;
}

bool windows_app_choose_audio_file(void *window_handle, char *path, size_t path_size) {
    (void)window_handle;
    if (path != NULL && path_size > 0U) {
        path[0] = '\0';
    }
    return false;
}

bool windows_app_choose_wav_save_path(void *window_handle,
                                      const char *suggested_name,
                                      char *path,
                                      size_t path_size) {
    (void)window_handle;
    (void)suggested_name;
    if (path != NULL && path_size > 0U) {
        path[0] = '\0';
    }
    return false;
}

#endif
