#include "platform/monotonic_clock.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

double platform_monotonic_seconds(void) {
    static LARGE_INTEGER frequency = {0};
    LARGE_INTEGER counter = {0};
    if (frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&frequency);
    }
    if (frequency.QuadPart == 0 ||
        !QueryPerformanceCounter(&counter)) {
        return 0.0;
    }
    return (double)counter.QuadPart /
           (double)frequency.QuadPart;
}

#elif defined(__APPLE__)

#include <mach/mach_time.h>

double platform_monotonic_seconds(void) {
    static mach_timebase_info_data_t timebase = {0};
    if (timebase.denom == 0U) {
        mach_timebase_info(&timebase);
    }
    if (timebase.denom == 0U) {
        return 0.0;
    }
    return (double)mach_absolute_time() *
           (double)timebase.numer /
           (double)timebase.denom /
           1000000000.0;
}

#else

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif
#include <time.h>

double platform_monotonic_seconds(void) {
    struct timespec now = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0.0;
    }
    return (double)now.tv_sec +
           (double)now.tv_nsec / 1000000000.0;
}

#endif
