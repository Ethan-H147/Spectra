#include "dsp/signal_utils.h"

#include <math.h>

float clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

float peak_amplitude(const float *samples, size_t count) {
    float peak = 0.0f;

    for (size_t i = 0; i < count; i++) {
        float value = fabsf(samples[i]);
        if (value > peak) {
            peak = value;
        }
    }

    return peak;
}

void normalize_samples(float *samples, size_t count, float target_peak) {
    float peak = peak_amplitude(samples, count);
    if (peak <= target_peak || peak <= 0.0f) {
        return;
    }

    float gain = target_peak / peak;
    for (size_t i = 0; i < count; i++) {
        samples[i] *= gain;
    }
}

float linear_to_db(float value) {
    if (value <= 0.000001f) {
        return -120.0f;
    }

    float db = 20.0f * log10f(value);
    if (db < -120.0f) {
        return -120.0f;
    }
    return db;
}

unsigned int next_power_of_two(unsigned int value) {
    if (value <= 1) {
        return 1;
    }

    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1;
}
