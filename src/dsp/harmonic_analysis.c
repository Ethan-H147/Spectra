#include "dsp/harmonic_analysis.h"

#include "dsp/signal_utils.h"

#include <math.h>

static void sort_peaks_by_frequency(Peak *peaks, int peak_count) {
    for (int i = 0; i < peak_count - 1; i++) {
        for (int j = i + 1; j < peak_count; j++) {
            if (peaks[j].frequency < peaks[i].frequency) {
                Peak temp = peaks[i];
                peaks[i] = peaks[j];
                peaks[j] = temp;
            }
        }
    }
}

static int weakest_peak_index(const Peak *peaks, int peak_count) {
    int weakest = 0;
    for (int i = 1; i < peak_count; i++) {
        if (peaks[i].magnitude < peaks[weakest].magnitude) {
            weakest = i;
        }
    }
    return weakest;
}

static void interpolate_peak(const Spectrum *spectrum,
                             size_t bin,
                             float *frequency,
                             float *magnitude);

static int find_peaks_internal(const Spectrum *spectrum,
                               float min_frequency,
                               float max_frequency,
                               float threshold_db,
                               Peak *peaks,
                               int max_peaks,
                               bool interpolate) {
    if (spectrum == NULL || peaks == NULL || max_peaks <= 0 || spectrum->count < 3) {
        return 0;
    }

    int peak_count = 0;
    for (size_t i = 1; i < spectrum->count - 1; i++) {
        float frequency = spectrum->frequencies[i];
        float magnitude = spectrum->magnitudes[i];
        float db = linear_to_db(magnitude);
        int is_local_maximum = magnitude > spectrum->magnitudes[i - 1] && magnitude >= spectrum->magnitudes[i + 1];

        if (!is_local_maximum || frequency < min_frequency || frequency > max_frequency || db < threshold_db) {
            continue;
        }

        Peak peak = {frequency, magnitude, db};
        if (interpolate) {
            interpolate_peak(
                spectrum, i, &peak.frequency, &peak.magnitude);
            peak.db = linear_to_db(peak.magnitude);
        }
        if (peak_count < max_peaks) {
            peaks[peak_count++] = peak;
        } else {
            int weakest = weakest_peak_index(peaks, peak_count);
            if (peak.magnitude > peaks[weakest].magnitude) {
                peaks[weakest] = peak;
            }
        }
    }

    sort_peaks_by_frequency(peaks, peak_count);
    return peak_count;
}

int find_peaks(const Spectrum *spectrum,
               float min_frequency,
               float max_frequency,
               float threshold_db,
               Peak *peaks,
               int max_peaks) {
    return find_peaks_internal(spectrum,
                               min_frequency,
                               max_frequency,
                               threshold_db,
                               peaks,
                               max_peaks,
                               false);
}

int find_interpolated_peaks(const Spectrum *spectrum,
                            float min_frequency,
                            float max_frequency,
                            float threshold_db,
                            Peak *peaks,
                            int max_peaks) {
    return find_peaks_internal(spectrum,
                               min_frequency,
                               max_frequency,
                               threshold_db,
                               peaks,
                               max_peaks,
                               true);
}

static float clamp_local(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void interpolate_peak(const Spectrum *spectrum,
                             size_t bin,
                             float *frequency,
                             float *magnitude) {
    *frequency = spectrum->frequencies[bin];
    *magnitude = spectrum->magnitudes[bin];
    if (bin == 0U || bin + 1U >= spectrum->count) {
        return;
    }

    float left = logf(fmaxf(spectrum->magnitudes[bin - 1U], 1.0e-20f));
    float center = logf(fmaxf(spectrum->magnitudes[bin], 1.0e-20f));
    float right = logf(fmaxf(spectrum->magnitudes[bin + 1U], 1.0e-20f));
    float curvature = left - 2.0f * center + right;
    if (fabsf(curvature) < 1.0e-12f) {
        return;
    }

    float offset = clamp_local(0.5f * (left - right) / curvature, -0.5f, 0.5f);
    float bin_width = spectrum->frequencies[bin + 1U] - spectrum->frequencies[bin];
    *frequency += offset * bin_width;
    *magnitude =
        expf(center - 0.25f * (left - right) * offset);
}

int extract_harmonics(const Spectrum *spectrum,
                      float fundamental_frequency,
                      int harmonic_count,
                      float tolerance_cents,
                      float threshold_db,
                      ExtractedHarmonic *harmonics,
                      int capacity) {
    if (spectrum == NULL || spectrum->frequencies == NULL || spectrum->magnitudes == NULL ||
        spectrum->count < 3U || fundamental_frequency <= 0.0f || harmonic_count <= 0 ||
        tolerance_cents <= 0.0f || harmonics == NULL || capacity <= 0) {
        return 0;
    }

    int entry_count = harmonic_count < capacity ? harmonic_count : capacity;
    float maximum_frequency = spectrum->frequencies[spectrum->count - 1U];
    float strongest_magnitude = 0.0f;
    int valid_entries = 0;

    for (int index = 0; index < entry_count; index++) {
        int number = index + 1;
        float expected = fundamental_frequency * (float)number;
        ExtractedHarmonic entry = {
            .harmonic_number = number,
            .expected_frequency = expected,
            .detected_frequency = 0.0f,
            .amplitude = 0.0f,
            .source_magnitude = 0.0f,
            .db = -120.0f,
            .detected = false,
        };

        if (expected > maximum_frequency) {
            harmonics[index] = entry;
            continue;
        }
        valid_entries = index + 1;

        float ratio = powf(2.0f, tolerance_cents / 1200.0f);
        float minimum = expected / ratio;
        float maximum = expected * ratio;
        size_t best_bin = 0U;
        float best_magnitude = 0.0f;

        for (size_t bin = 1U; bin + 1U < spectrum->count; bin++) {
            float frequency = spectrum->frequencies[bin];
            if (frequency < minimum) continue;
            if (frequency > maximum) break;

            float magnitude = spectrum->magnitudes[bin];
            bool local_maximum =
                magnitude > spectrum->magnitudes[bin - 1U] && magnitude >= spectrum->magnitudes[bin + 1U];
            if (local_maximum && linear_to_db(magnitude) >= threshold_db && magnitude > best_magnitude) {
                best_bin = bin;
                best_magnitude = magnitude;
            }
        }

        if (best_bin != 0U) {
            interpolate_peak(spectrum, best_bin, &entry.detected_frequency, &entry.source_magnitude);
            entry.db = linear_to_db(entry.source_magnitude);
            entry.detected = true;
            if (entry.source_magnitude > strongest_magnitude) {
                strongest_magnitude = entry.source_magnitude;
            }
        }
        harmonics[index] = entry;
    }

    if (strongest_magnitude > 0.0f) {
        for (int index = 0; index < valid_entries; index++) {
            if (harmonics[index].detected) {
                harmonics[index].amplitude = harmonics[index].source_magnitude / strongest_magnitude;
            }
        }
    }
    return valid_entries;
}
