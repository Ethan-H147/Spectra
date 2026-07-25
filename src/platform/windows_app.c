#include "platform/windows_app.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

#define IDI_SPECTRA_ICON 101

static HICON g_large_icon = NULL;
static HICON g_small_icon = NULL;

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

#else

void windows_app_prepare_process(void) {
}

void windows_app_apply_window_icon(void *window_handle) {
    (void)window_handle;
}

#endif
