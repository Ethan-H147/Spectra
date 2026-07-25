#include "dsp/additive_synth.h"
#include "dsp/fft.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/pitch_detection.h"
#include "dsp/signal_utils.h"
#include "dsp/windowing.h"

#include <math.h>
#include <stdio.h>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int test_sine_length(void) {
    SampleBuffer sine = generate_sine_wave(440.0f, 1.0f, 0.5f, 44100);
    ASSERT_TRUE(sine.count == 22050, "sine wave length should match duration * sample rate");
    ASSERT_TRUE(fabsf(sine.samples[0]) < 0.0001f, "sine should start near zero");
    sample_buffer_free(&sine);
    return 0;
}

static int test_hann_window(void) {
    float window[5] = {0};
    hann_window(window, 5);
    ASSERT_TRUE(fabsf(window[0]) < 0.0001f, "Hann first sample should be zero");
    ASSERT_TRUE(fabsf(window[2] - 1.0f) < 0.0001f, "Hann center sample should be one for size 5");
    ASSERT_TRUE(fabsf(window[4]) < 0.0001f, "Hann last sample should be zero");
    return 0;
}

static int test_additive_normalization(void) {
    Harmonic harmonics[3] = {
        {1, 1.0f, 0.0f},
        {2, 1.0f, 0.0f},
        {3, 1.0f, 0.0f},
    };
    ADSREnvelope envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    SampleBuffer tone = generate_additive_tone(110.0f, harmonics, 3, 1.0f, 44100, envelope);
    ASSERT_TRUE(peak_amplitude(tone.samples, tone.count) <= 0.9501f, "additive tone should be normalized");
    sample_buffer_free(&tone);
    return 0;
}

static int test_peak_detection(void) {
    float frequencies[] = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};
    float magnitudes[] = {0.01f, 0.2f, 1.0f, 0.2f, 0.01f};
    Spectrum spectrum = {frequencies, magnitudes, 5};
    Peak peaks[4] = {0};
    int count = find_peaks(&spectrum, 50.0f, 600.0f, -20.0f, peaks, 4);
    ASSERT_TRUE(count == 1, "synthetic spectrum should have one peak");
    ASSERT_TRUE(fabsf(peaks[0].frequency - 300.0f) < 0.01f, "peak should be at 300 Hz");
    return 0;
}

static int test_spectrum_for_a4(void) {
    SampleBuffer sine = generate_sine_wave(440.0f, 1.0f, 1.0f, 44100);
    Spectrum spectrum = compute_magnitude_spectrum(sine.samples, sine.count, sine.sample_rate);
    Peak peaks[4] = {0};
    int count = find_peaks(&spectrum, 50.0f, 1000.0f, -60.0f, peaks, 4);
    ASSERT_TRUE(count >= 1, "440 Hz sine should produce a spectrum peak");
    ASSERT_TRUE(fabsf(peaks[0].frequency - 440.0f) < 4.0f, "strongest A4 peak should be near 440 Hz");
    spectrum_free(&spectrum);
    sample_buffer_free(&sine);
    return 0;
}

static int test_pitch_for_a4(void) {
    SampleBuffer sine = generate_sine_wave(440.0f, 0.8f, 0.5f, 44100);
    PitchEstimate pitch = estimate_pitch(&sine, 40.0f, 1200.0f);
    ASSERT_TRUE(pitch.valid, "440 Hz sine should produce a valid pitch estimate");
    ASSERT_TRUE(fabsf(pitch.frequency_hz - 440.0f) < 1.0f, "estimated A4 frequency should be near 440 Hz");
    ASSERT_TRUE(pitch.midi_note == 69, "440 Hz should map to MIDI note 69");
    ASSERT_TRUE(pitch_note_name(pitch.midi_note)[0] == 'A', "MIDI note 69 should be named A");
    ASSERT_TRUE(pitch_note_octave(pitch.midi_note) == 4, "MIDI note 69 should be in octave 4");
    ASSERT_TRUE(fabsf(pitch.cents) < 4.0f, "440 Hz should be close to zero cents from A4");
    ASSERT_TRUE(pitch.confidence > 0.90f, "clean sine pitch confidence should be high");
    sample_buffer_free(&sine);
    return 0;
}

static int test_pitch_for_harmonic_tone(void) {
    Harmonic harmonics[] = {
        {2, 0.80f, 0.0f},
        {3, 0.50f, 0.0f},
        {4, 0.30f, 0.0f},
    };
    ADSREnvelope envelope = {0.0f, 0.0f, 1.0f, 0.0f};
    SampleBuffer tone = generate_additive_tone(110.0f, harmonics, 3, 0.5f, 44100, envelope);
    PitchEstimate pitch = estimate_pitch(&tone, 40.0f, 1200.0f);
    ASSERT_TRUE(pitch.valid, "harmonic tone should produce a valid pitch estimate");
    ASSERT_TRUE(fabsf(pitch.frequency_hz - 110.0f) < 1.0f,
                "estimator should recover a missing 110 Hz fundamental from its harmonics");
    ASSERT_TRUE(pitch.confidence > 0.80f, "periodic harmonic tone pitch confidence should be high");
    sample_buffer_free(&tone);
    return 0;
}

static int test_pitch_rejects_silence(void) {
    float samples[4096] = {0};
    SampleBuffer silence = {samples, 4096, 44100};
    PitchEstimate pitch = estimate_pitch(&silence, 40.0f, 1200.0f);
    ASSERT_TRUE(!pitch.valid, "silence should not produce a pitch estimate");
    return 0;
}

int main(void) {
    if (test_sine_length() != 0) return 1;
    if (test_hann_window() != 0) return 1;
    if (test_additive_normalization() != 0) return 1;
    if (test_peak_detection() != 0) return 1;
    if (test_spectrum_for_a4() != 0) return 1;
    if (test_pitch_for_a4() != 0) return 1;
    if (test_pitch_for_harmonic_tone() != 0) return 1;
    if (test_pitch_rejects_silence() != 0) return 1;

    puts("All DSP tests passed.");
    return 0;
}
