#include "dsp/additive_synth.h"

#include "dsp/signal_utils.h"

#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SampleBuffer generate_sine_wave(float frequency,
                                float amplitude,
                                float duration_seconds,
                                unsigned int sample_rate) {
    SampleBuffer buffer = {0};
    if (duration_seconds <= 0.0f || sample_rate == 0) {
        return buffer;
    }

    buffer.count = (size_t)(duration_seconds * (float)sample_rate);
    buffer.sample_rate = sample_rate;
    buffer.samples = (float *)calloc(buffer.count, sizeof(float));
    if (buffer.samples == NULL) {
        buffer.count = 0;
        buffer.sample_rate = 0;
        return buffer;
    }

    for (size_t n = 0; n < buffer.count; n++) {
        buffer.samples[n] = amplitude * sinf((2.0f * (float)M_PI * frequency * (float)n) / (float)sample_rate);
    }

    return buffer;
}

void apply_adsr_envelope(float *samples, size_t count, unsigned int sample_rate, ADSREnvelope envelope) {
    if (samples == NULL || count == 0 || sample_rate == 0) {
        return;
    }

    size_t attack_samples = (size_t)(envelope.attack_seconds * (float)sample_rate);
    size_t decay_samples = (size_t)(envelope.decay_seconds * (float)sample_rate);
    size_t release_samples = (size_t)(envelope.release_seconds * (float)sample_rate);
    float sustain = clampf(envelope.sustain_level, 0.0f, 1.0f);
    size_t release_start = release_samples < count ? count - release_samples : 0;

    for (size_t n = 0; n < count; n++) {
        float gain = sustain;

        if (attack_samples > 0 && n < attack_samples) {
            gain = (float)n / (float)attack_samples;
        } else if (decay_samples > 0 && n < attack_samples + decay_samples) {
            float progress = (float)(n - attack_samples) / (float)decay_samples;
            gain = 1.0f - progress * (1.0f - sustain);
        }

        if (release_samples > 0 && n >= release_start) {
            float progress = (float)(n - release_start) / (float)release_samples;
            gain *= 1.0f - clampf(progress, 0.0f, 1.0f);
        }

        samples[n] *= gain;
    }
}

SampleBuffer generate_additive_tone(float fundamental_frequency,
                                    const Harmonic *harmonics,
                                    int harmonic_count,
                                    float duration_seconds,
                                    unsigned int sample_rate,
                                    ADSREnvelope envelope) {
    SampleBuffer buffer = {0};
    if (duration_seconds <= 0.0f || sample_rate == 0 || harmonics == NULL || harmonic_count <= 0) {
        return buffer;
    }

    buffer.count = (size_t)(duration_seconds * (float)sample_rate);
    buffer.sample_rate = sample_rate;
    buffer.samples = (float *)calloc(buffer.count, sizeof(float));
    if (buffer.samples == NULL) {
        buffer.count = 0;
        buffer.sample_rate = 0;
        return buffer;
    }

    for (size_t n = 0; n < buffer.count; n++) {
        float value = 0.0f;
        for (int h = 0; h < harmonic_count; h++) {
            if (harmonics[h].amplitude == 0.0f) {
                continue;
            }

            float frequency = fundamental_frequency * (float)harmonics[h].multiple;
            float phase = harmonics[h].phase;
            value += harmonics[h].amplitude * sinf((2.0f * (float)M_PI * frequency * (float)n) / (float)sample_rate + phase);
        }
        buffer.samples[n] = value;
    }

    apply_adsr_envelope(buffer.samples, buffer.count, sample_rate, envelope);
    normalize_samples(buffer.samples, buffer.count, 0.95f);
    return buffer;
}
