#include "audio/audio_engine.h"
#include "dsp/additive_synth.h"
#include "dsp/fft.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/signal_utils.h"
#include "ui/widgets.h"
#include "ui/theme.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>

static AppTheme *g_theme = NULL;

static void app_draw_text(const char *text, int x, int y, int font_size, Color color) {
    if (g_theme != NULL) {
        theme_draw_text(g_theme, text, (float)x, (float)y, (float)font_size, color);
        return;
    }

    DrawText(text, x, y, font_size, color);
}

#define DrawText app_draw_text

#define SCREEN_WIDTH 1180
#define SCREEN_HEIGHT 840
#define SAMPLE_RATE 44100U
#define HARMONIC_COUNT 16
#define MAX_PEAKS 12
#define TARGET_FPS 60.0

static const ADSREnvelope DEFAULT_ENVELOPE = {0.015f, 0.08f, 0.75f, 0.12f};

typedef enum {
    PRESET_SINE = 0,
    PRESET_SQUARE,
    PRESET_SAW,
    PRESET_CLARINET,
    PRESET_STRING,
    PRESET_COUNT
} PresetId;

typedef struct {
    const char *name;
    const char *description;
} PresetInfo;

static const PresetInfo PRESETS[PRESET_COUNT] = {
    {"Sine", "Fundamental only"},
    {"Square-like", "Odd harmonics, 1/n rolloff"},
    {"Saw-like", "All harmonics, 1/n rolloff"},
    {"Clarinet-like", "Odd-heavy softer stack"},
    {"Bright string", "Many upper harmonics"},
};

static void apply_preset(PresetId preset, Harmonic harmonics[HARMONIC_COUNT]) {
    for (int i = 0; i < HARMONIC_COUNT; i++) {
        int multiple = i + 1;
        float amplitude = 0.0f;

        switch (preset) {
            case PRESET_SINE:
                amplitude = multiple == 1 ? 0.8f : 0.0f;
                break;
            case PRESET_SQUARE:
                amplitude = multiple % 2 == 1 ? 0.8f / (float)multiple : 0.0f;
                break;
            case PRESET_SAW:
                amplitude = 0.65f / (float)multiple;
                break;
            case PRESET_CLARINET:
                amplitude = multiple % 2 == 1 ? 0.85f / powf((float)multiple, 1.25f) : 0.03f / (float)multiple;
                break;
            case PRESET_STRING:
                amplitude = 0.72f * expf(-0.16f * (float)(multiple - 1));
                break;
            default:
                break;
        }

        harmonics[i].multiple = multiple;
        harmonics[i].amplitude = amplitude;
        harmonics[i].phase = 0.0f;
    }
}

static SampleBuffer render_tone(float fundamental,
                                const Harmonic harmonics[HARMONIC_COUNT],
                                float duration_seconds,
                                float master_gain) {
    Harmonic active[HARMONIC_COUNT];
    for (int i = 0; i < HARMONIC_COUNT; i++) {
        active[i] = harmonics[i];
        active[i].amplitude *= master_gain;
    }

    return generate_additive_tone(fundamental, active, HARMONIC_COUNT, duration_seconds, SAMPLE_RATE, DEFAULT_ENVELOPE);
}

static void draw_waveform(Rectangle bounds, const SampleBuffer *buffer) {
    DrawRectangleRec(bounds, (Color){246, 248, 249, 255});
    DrawRectangleLinesEx(bounds, 1, (Color){222, 228, 231, 255});
    DrawLine((int)bounds.x,
             (int)(bounds.y + bounds.height * 0.5f),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height * 0.5f),
             (Color){198, 207, 211, 255});

    if (buffer == NULL || buffer->samples == NULL || buffer->count == 0) {
        return;
    }

    int width = (int)bounds.width;
    size_t samples_per_pixel = buffer->count / (size_t)width;
    if (samples_per_pixel < 1) {
        samples_per_pixel = 1;
    }

    for (int x = 0; x < width; x++) {
        size_t start = (size_t)x * samples_per_pixel;
        if (start >= buffer->count) {
            break;
        }

        float min_value = 1.0f;
        float max_value = -1.0f;
        for (size_t i = 0; i < samples_per_pixel && start + i < buffer->count; i++) {
            float value = buffer->samples[start + i];
            if (value < min_value) {
                min_value = value;
            }
            if (value > max_value) {
                max_value = value;
            }
        }

        float y_min = bounds.y + (1.0f - min_value) * 0.5f * bounds.height;
        float y_max = bounds.y + (1.0f - max_value) * 0.5f * bounds.height;
        DrawLine((int)bounds.x + x, (int)y_min, (int)bounds.x + x, (int)y_max, (Color){15, 118, 110, 255});
    }
}

static void draw_spectrum(Rectangle bounds, const Spectrum *spectrum, const Peak *peaks, int peak_count) {
    const float max_frequency = 8000.0f;
    const float min_db = -90.0f;
    const float max_db = 0.0f;

    DrawRectangleRec(bounds, (Color){246, 248, 249, 255});
    DrawRectangleLinesEx(bounds, 1, (Color){222, 228, 231, 255});

    for (int i = 0; i <= 4; i++) {
        float y = bounds.y + (bounds.height * (float)i / 4.0f);
        DrawLine((int)bounds.x, (int)y, (int)(bounds.x + bounds.width), (int)y, (Color){225, 231, 234, 255});
    }

    if (spectrum == NULL || spectrum->count == 0) {
        return;
    }

    Vector2 previous = {0};
    bool has_previous = false;
    for (size_t i = 0; i < spectrum->count; i++) {
        float frequency = spectrum->frequencies[i];
        if (frequency > max_frequency) {
            break;
        }

        float db = clampf(linear_to_db(spectrum->magnitudes[i]), min_db, max_db);
        float x = bounds.x + (frequency / max_frequency) * bounds.width;
        float y = bounds.y + bounds.height - ((db - min_db) / (max_db - min_db)) * bounds.height;
        Vector2 current = {x, y};
        if (has_previous) {
            DrawLineV(previous, current, (Color){37, 99, 235, 255});
        }
        previous = current;
        has_previous = true;
    }

    for (int i = 0; i < peak_count; i++) {
        if (peaks[i].frequency > max_frequency) {
            continue;
        }
        float db = clampf(peaks[i].db, min_db, max_db);
        float x = bounds.x + (peaks[i].frequency / max_frequency) * bounds.width;
        float y = bounds.y + bounds.height - ((db - min_db) / (max_db - min_db)) * bounds.height;
        DrawCircle((int)x, (int)y, 4, (Color){180, 35, 24, 255});
    }
}

static void draw_peak_readout(Rectangle bounds, const Peak *peaks, int peak_count) {
    DrawText("Detected spectral peaks", (int)bounds.x, (int)bounds.y, 18, (Color){26, 34, 36, 255});
    int shown = peak_count < 6 ? peak_count : 6;
    for (int i = 0; i < shown; i++) {
        char text[96];
        snprintf(text, sizeof(text), "%2d. %7.1f Hz  %6.1f dB", i + 1, peaks[i].frequency, peaks[i].db);
        DrawText(text, (int)bounds.x, (int)bounds.y + 28 + i * 22, 17, (Color){86, 98, 102, 255});
    }
    if (shown == 0) {
        DrawText("No peaks above threshold yet.", (int)bounds.x, (int)bounds.y + 28, 17, (Color){86, 98, 102, 255});
    }
}

int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Spectra - Fourier Additive Synth Desktop");
    InitAudioDevice();
    bool audio_ready = IsAudioDeviceReady();
    if (audio_ready) {
        SetMasterVolume(1.0f);
    }
#if !defined(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
    SetTargetFPS(60);
#endif

    AppTheme theme;
    theme_init(&theme);
    g_theme = &theme;

    Harmonic harmonics[HARMONIC_COUNT];
    PresetId selected_preset = PRESET_SINE;
    apply_preset(selected_preset, harmonics);

    float fundamental = 440.0f;
    float duration_seconds = 1.2f;
    float master_gain = 0.85f;
    bool dirty = true;
    char status[180];
    snprintf(status, sizeof(status), audio_ready ? "Audio ready: press Play tone or space." : "Audio device unavailable: export WAV will still work.");

    SampleBuffer generated = {0};
    Spectrum spectrum = {0};
    Peak peaks[MAX_PEAKS] = {0};
    int peak_count = 0;
    AudioClip clip;
    audio_clip_init(&clip);

    while (!WindowShouldClose()) {
#if defined(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
        double frame_started_at = GetTime();
#endif

        if (dirty) {
            sample_buffer_free(&generated);
            spectrum_free(&spectrum);
            generated = render_tone(fundamental, harmonics, duration_seconds, master_gain);
            spectrum = compute_magnitude_spectrum(generated.samples, generated.count, generated.sample_rate);
            peak_count = find_peaks(&spectrum, 20.0f, 8000.0f, -55.0f, peaks, MAX_PEAKS);
            audio_clip_set_samples(&clip, &generated);
            dirty = false;
        }

        BeginDrawing();
        ClearBackground(theme.background);

        DrawText("Spectra", 28, 24, 42, (Color){23, 32, 34, 255});
        DrawText("Fourier-based desktop audio analysis and additive synthesis for musical sound.",
                 30,
                 72,
                 18,
                 (Color){85, 98, 102, 255});
        DrawText(status, 30, 102, 17, (Color){15, 118, 110, 255});

        Rectangle synth_panel = {24, 134, 460, 680};
        Rectangle visual_panel = {504, 134, 652, 680};
        draw_panel(&theme, synth_panel, "Harmonic Synth");
        draw_panel(&theme, visual_panel, "Visualization");

        Rectangle play_button = {42, 178, 128, 42};
        Rectangle stop_button = {182, 178, 112, 42};
        Rectangle export_button = {306, 178, 152, 42};
        if (draw_button(&theme, play_button, "Play tone", (Color){15, 118, 110, 255}) || IsKeyPressed(KEY_SPACE)) {
            if (!audio_ready) {
                snprintf(status, sizeof(status), "No audio device detected. Try export WAV or check WSL/Windows audio output.");
            } else if (audio_clip_play(&clip)) {
                snprintf(status, sizeof(status), "Playing %.1f Hz %s tone at full app volume.", fundamental, PRESETS[selected_preset].name);
            } else {
                snprintf(status, sizeof(status), "Playback buffer was not ready. Try adjusting a slider and pressing Play again.");
            }
        }
        if (draw_button(&theme, stop_button, "Stop", (Color){120, 75, 20, 255})) {
            audio_clip_stop(&clip);
            snprintf(status, sizeof(status), "Stopped playback.");
        }
        if (draw_button(&theme, export_button, "Export WAV", (Color){37, 99, 235, 255})) {
            if (export_samples_to_wav("spectra-tone.wav", &generated)) {
                snprintf(status, sizeof(status), "Exported spectra-tone.wav");
            } else {
                snprintf(status, sizeof(status), "Could not export spectra-tone.wav");
            }
        }

        float new_fundamental =
            draw_slider(&theme, (Rectangle){42, 244, 392, 48}, "Fundamental frequency (Hz)", fundamental, 40.0f, 1200.0f);
        float new_duration =
            draw_slider(&theme, (Rectangle){42, 306, 392, 48}, "Duration (seconds)", duration_seconds, 0.2f, 3.0f);
        float new_gain = draw_slider(&theme, (Rectangle){42, 368, 392, 48}, "Master gain", master_gain, 0.05f, 1.0f);
        if (fabsf(new_fundamental - fundamental) > 0.01f || fabsf(new_duration - duration_seconds) > 0.001f ||
            fabsf(new_gain - master_gain) > 0.001f) {
            fundamental = new_fundamental;
            duration_seconds = new_duration;
            master_gain = new_gain;
            dirty = true;
        }

        DrawText("Presets", 42, 438, 18, (Color){26, 34, 36, 255});
        for (int i = 0; i < PRESET_COUNT; i++) {
            Rectangle bounds = {42.0f + (float)(i % 2) * 202.0f, 466.0f + (float)(i / 2) * 44.0f, 190.0f, 34.0f};
            Color accent = selected_preset == (PresetId)i ? (Color){15, 118, 110, 255} : (Color){67, 78, 82, 255};
            if (draw_button(&theme, bounds, PRESETS[i].name, accent)) {
                selected_preset = (PresetId)i;
                apply_preset(selected_preset, harmonics);
                dirty = true;
                snprintf(status, sizeof(status), "Loaded %s preset: %s.", PRESETS[i].name, PRESETS[i].description);
            }
        }

        DrawText("Harmonics", 42, 602, 18, (Color){26, 34, 36, 255});
        for (int i = 0; i < HARMONIC_COUNT; i++) {
            int column = i % 4;
            int row = i / 4;
            char label[32];
            snprintf(label, sizeof(label), "H%02d", i + 1);
            Rectangle slider = {42.0f + (float)column * 102.0f, 630.0f + (float)row * 42.0f, 86.0f, 36.0f};
            float new_amplitude = draw_slider(&theme, slider, label, harmonics[i].amplitude, 0.0f, 1.0f);
            if (fabsf(new_amplitude - harmonics[i].amplitude) > 0.001f) {
                harmonics[i].amplitude = new_amplitude;
                dirty = true;
            }
        }

        DrawText("Waveform", 528, 178, 20, (Color){26, 34, 36, 255});
        draw_waveform((Rectangle){528, 208, 604, 170}, &generated);

        DrawText("Frequency spectrum", 528, 402, 20, (Color){26, 34, 36, 255});
        draw_spectrum((Rectangle){528, 432, 604, 180}, &spectrum, peaks, peak_count);
        draw_peak_readout((Rectangle){528, 634, 310, 90}, peaks, peak_count);

        DrawText("Fourier idea: timbre is shaped by energy at harmonic frequencies.",
                 844,
                 634,
                 18,
                 (Color){26, 34, 36, 255});
        DrawText("This MVP generates additive tones, applies ADSR, normalizes,", 844, 664, 16, (Color){86, 98, 102, 255});
        DrawText("plays them with Raylib audio, and visualizes waveform + FFT.", 844, 686, 16, (Color){86, 98, 102, 255});

        EndDrawing();

#if defined(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
        SwapScreenBuffer();
        PollInputEvents();

        double frame_seconds = GetTime() - frame_started_at;
        double remaining_seconds = (1.0 / TARGET_FPS) - frame_seconds;
        if (remaining_seconds > 0.0) {
            WaitTime(remaining_seconds);
        }
#endif
    }

    audio_clip_unload(&clip);
    spectrum_free(&spectrum);
    sample_buffer_free(&generated);
    CloseAudioDevice();
    theme_unload(&theme);
    CloseWindow();
    return 0;
}





