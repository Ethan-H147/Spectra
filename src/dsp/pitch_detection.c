#include "dsp/pitch_detection.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define PITCH_FRAME_SIZE 8192U
#define YIN_THRESHOLD 0.15f
#define YIN_FALLBACK_THRESHOLD 0.35f
#define MIN_RMS 0.0001f

static float clampf_local(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float interpolated_lag(const float *difference, unsigned int lag, unsigned int max_lag) {
    if (difference == NULL || lag == 0U || lag >= max_lag) {
        return (float)lag;
    }

    float left = difference[lag - 1U];
    float center = difference[lag];
    float right = difference[lag + 1U];
    float curvature = left - 2.0f * center + right;
    if (fabsf(curvature) < 1.0e-8f) {
        return (float)lag;
    }

    float offset = 0.5f * (left - right) / curvature;
    return (float)lag + clampf_local(offset, -1.0f, 1.0f);
}

PitchEstimate estimate_pitch(const SampleBuffer *buffer, float min_frequency, float max_frequency) {
    PitchEstimate estimate = {0};
    if (buffer == NULL || buffer->samples == NULL || buffer->count < 4U || buffer->sample_rate == 0U ||
        min_frequency <= 0.0f || max_frequency <= min_frequency) {
        return estimate;
    }

    unsigned int min_lag = (unsigned int)floorf((float)buffer->sample_rate / max_frequency);
    unsigned int max_lag = (unsigned int)ceilf((float)buffer->sample_rate / min_frequency);
    if (min_lag < 2U) min_lag = 2U;

    size_t frame_count = buffer->count < PITCH_FRAME_SIZE ? buffer->count : PITCH_FRAME_SIZE;
    if (max_lag + 3U >= frame_count) {
        max_lag = (unsigned int)frame_count - 3U;
    }
    if (min_lag >= max_lag) {
        return estimate;
    }

    size_t frame_start = (buffer->count - frame_count) / 2U;
    size_t comparison_count = frame_count - (size_t)max_lag;
    if (comparison_count < 2U) {
        return estimate;
    }

    double energy = 0.0;
    for (size_t i = 0; i < frame_count; i++) {
        double sample = buffer->samples[frame_start + i];
        energy += sample * sample;
    }
    float rms = sqrtf((float)(energy / (double)frame_count));
    if (rms < MIN_RMS) {
        return estimate;
    }

    float *difference = (float *)calloc((size_t)max_lag + 1U, sizeof(float));
    if (difference == NULL) {
        return estimate;
    }

    for (unsigned int lag = 1U; lag <= max_lag; lag++) {
        double sum = 0.0;
        for (size_t i = 0; i < comparison_count; i++) {
            double delta = (double)buffer->samples[frame_start + i] -
                           (double)buffer->samples[frame_start + i + lag];
            sum += delta * delta;
        }
        difference[lag] = (float)sum;
    }

    double cumulative = 0.0;
    difference[0] = 1.0f;
    for (unsigned int lag = 1U; lag <= max_lag; lag++) {
        cumulative += difference[lag];
        difference[lag] = cumulative > 0.0 ? difference[lag] * (float)lag / (float)cumulative : 1.0f;
    }

    unsigned int selected_lag = 0U;
    for (unsigned int lag = min_lag; lag <= max_lag; lag++) {
        if (difference[lag] >= YIN_THRESHOLD) {
            continue;
        }

        selected_lag = lag;
        while (selected_lag < max_lag && difference[selected_lag + 1U] < difference[selected_lag]) {
            selected_lag++;
        }
        break;
    }

    if (selected_lag == 0U) {
        selected_lag = min_lag;
        for (unsigned int lag = min_lag + 1U; lag <= max_lag; lag++) {
            if (difference[lag] < difference[selected_lag]) {
                selected_lag = lag;
            }
        }
        if (difference[selected_lag] > YIN_FALLBACK_THRESHOLD) {
            free(difference);
            return estimate;
        }
    }

    float lag = interpolated_lag(difference, selected_lag, max_lag);
    float frequency = (float)buffer->sample_rate / lag;
    float confidence = clampf_local(1.0f - difference[selected_lag], 0.0f, 1.0f);
    free(difference);

    if (!isfinite(frequency) || frequency < min_frequency || frequency > max_frequency) {
        return estimate;
    }

    float midi_value = 69.0f + 12.0f * log2f(frequency / 440.0f);
    int midi_note = (int)lroundf(midi_value);
    float reference_frequency = 440.0f * powf(2.0f, ((float)midi_note - 69.0f) / 12.0f);

    estimate.valid = true;
    estimate.frequency_hz = frequency;
    estimate.confidence = confidence;
    estimate.midi_note = midi_note;
    estimate.cents = 1200.0f * log2f(frequency / reference_frequency);
    return estimate;
}

const char *pitch_note_name(int midi_note) {
    static const char *NAMES[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
    };
    int index = midi_note % 12;
    if (index < 0) index += 12;
    return NAMES[index];
}

int pitch_note_octave(int midi_note) {
    return (int)floorf((float)midi_note / 12.0f) - 1;
}
