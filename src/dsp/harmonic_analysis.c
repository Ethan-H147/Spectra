#include "dsp/harmonic_analysis.h"

#include "dsp/signal_utils.h"

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

int find_peaks(const Spectrum *spectrum,
               float min_frequency,
               float max_frequency,
               float threshold_db,
               Peak *peaks,
               int max_peaks) {
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
