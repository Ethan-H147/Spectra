#include "dsp/waveform_summary.h"

size_t summarize_waveform(const float *samples,
                          size_t sample_count,
                          float *minimums,
                          float *maximums,
                          size_t capacity) {
    if (samples == NULL || sample_count == 0U || minimums == NULL || maximums == NULL || capacity == 0U) {
        return 0U;
    }

    size_t bin_count = sample_count < capacity ? sample_count : capacity;
    for (size_t bin = 0; bin < bin_count; bin++) {
        size_t start = bin * sample_count / bin_count;
        size_t end = (bin + 1U) * sample_count / bin_count;
        if (end <= start) {
            end = start + 1U;
        }

        float minimum = samples[start];
        float maximum = samples[start];
        for (size_t sample = start + 1U; sample < end; sample++) {
            float value = samples[sample];
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
        minimums[bin] = minimum;
        maximums[bin] = maximum;
    }
    return bin_count;
}
