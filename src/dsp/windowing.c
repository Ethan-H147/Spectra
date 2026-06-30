#include "dsp/windowing.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void hann_window(float *window, size_t size) {
    if (window == NULL || size == 0) {
        return;
    }

    if (size == 1) {
        window[0] = 1.0f;
        return;
    }

    for (size_t n = 0; n < size; n++) {
        window[n] = 0.5f * (1.0f - cosf((2.0f * (float)M_PI * (float)n) / (float)(size - 1)));
    }
}

void apply_window(const float *samples, const float *window, float *output, size_t size) {
    for (size_t i = 0; i < size; i++) {
        output[i] = samples[i] * window[i];
    }
}
