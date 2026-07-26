#include "dsp/harmonic_resynthesis.h"

#include "dsp/additive_synth.h"

#include <stdlib.h>

SampleBuffer resynthesize_from_harmonics(float fundamental_frequency,
                                         const ExtractedHarmonic *harmonics,
                                         int harmonic_count,
                                         float duration_seconds,
                                         unsigned int sample_rate) {
    SampleBuffer empty = {0};
    if (fundamental_frequency <= 0.0f || harmonics == NULL || harmonic_count <= 0 ||
        duration_seconds <= 0.0f || sample_rate == 0U) {
        return empty;
    }

    Harmonic *partials = (Harmonic *)calloc((size_t)harmonic_count, sizeof(Harmonic));
    if (partials == NULL) {
        return empty;
    }

    int partial_count = 0;
    for (int index = 0; index < harmonic_count; index++) {
        if (!harmonics[index].detected || harmonics[index].amplitude <= 0.0f) {
            continue;
        }
        partials[partial_count++] = (Harmonic){
            .multiple = harmonics[index].harmonic_number,
            .amplitude = harmonics[index].amplitude,
            .phase = 0.0f,
        };
    }

    if (partial_count == 0) {
        free(partials);
        return empty;
    }

    ADSREnvelope envelope = {
        .attack_seconds = 0.008f,
        .decay_seconds = 0.020f,
        .sustain_level = 1.0f,
        .release_seconds = 0.030f,
    };
    SampleBuffer result = generate_additive_tone(
        fundamental_frequency, partials, partial_count, duration_seconds, sample_rate, envelope);
    free(partials);
    return result;
}
