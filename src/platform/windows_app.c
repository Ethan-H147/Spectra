#include "platform/windows_app.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>

void windows_app_prepare_process(void) {
    SetCurrentProcessExplicitAppUserModelID(L"Spectra.AudioLab.Desktop");
}

#else

void windows_app_prepare_process(void) {
}

#endif
