#include "audio/audio_engine.h"
#include "dsp/additive_synth.h"
#include "dsp/fft.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/pitch_detection.h"
#include "dsp/signal_utils.h"
#include "platform/windows_app.h"
#include "ui/app_shell.h"
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

#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 900
#define MIN_WINDOW_WIDTH 1100
#define MIN_WINDOW_HEIGHT 800
#define SAMPLE_RATE 44100U
#define HARMONIC_COUNT 16
#define MAX_PEAKS 12
#define TARGET_FPS 60.0

static const ADSREnvelope DEFAULT_ENVELOPE = {0.015f, 0.08f, 0.75f, 0.12f};

static void set_spectra_window_icon(void) {
    const int icon_size = 256;
    const int point_count = 65;
    Image icon = GenImageColor(icon_size, icon_size, (Color){250, 250, 250, 255});
    Vector2 previous = {28.0f, 128.0f};

    for (int i = 1; i < point_count; i++) {
        float t = (float)i / (float)(point_count - 1);
        Vector2 current = {
            28.0f + t * 200.0f,
            128.0f + sinf(t * 4.0f * PI) * 56.0f,
        };
        ImageDrawLineEx(&icon, previous, current, 18, BLACK);
        previous = current;
    }

    SetWindowIcon(icon);
    UnloadImage(icon);
}

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
    DrawRectangleRounded(bounds, 0.025f, 8, (Color){24, 24, 27, 255});
    DrawRectangleRoundedLines(bounds, 0.025f, 8, (Color){57, 57, 62, 255});
    DrawLine((int)bounds.x,
             (int)(bounds.y + bounds.height * 0.5f),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height * 0.5f),
             (Color){66, 66, 72, 255});

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
        DrawLine((int)bounds.x + x, (int)y_min, (int)bounds.x + x, (int)y_max, (Color){47, 181, 169, 255});
    }
}

static void draw_spectrum(Rectangle bounds, const Spectrum *spectrum, const Peak *peaks, int peak_count) {
    const float max_frequency = 8000.0f;
    const float min_db = -90.0f;
    const float max_db = 0.0f;

    DrawRectangleRounded(bounds, 0.025f, 8, (Color){24, 24, 27, 255});
    DrawRectangleRoundedLines(bounds, 0.025f, 8, (Color){57, 57, 62, 255});

    for (int i = 0; i <= 4; i++) {
        float y = bounds.y + (bounds.height * (float)i / 4.0f);
        DrawLine((int)bounds.x, (int)y, (int)(bounds.x + bounds.width), (int)y, (Color){48, 48, 53, 255});
    }

    if (spectrum == NULL || spectrum->count == 0) {
        return;
    }

    size_t visible_count = 0;
    while (visible_count < spectrum->count && spectrum->frequencies[visible_count] <= max_frequency) {
        visible_count++;
    }
    size_t bins_per_point = (visible_count + (size_t)bounds.width - 1U) / (size_t)bounds.width;
    if (bins_per_point < 1U) {
        bins_per_point = 1U;
    }

    Vector2 previous = {0};
    bool has_previous = false;
    for (size_t start = 0; start < visible_count; start += bins_per_point) {
        size_t end = start + bins_per_point;
        if (end > visible_count) {
            end = visible_count;
        }

        size_t strongest = start;
        for (size_t i = start + 1U; i < end; i++) {
            if (spectrum->magnitudes[i] > spectrum->magnitudes[strongest]) {
                strongest = i;
            }
        }

        float frequency = spectrum->frequencies[strongest];
        float db = clampf(linear_to_db(spectrum->magnitudes[strongest]), min_db, max_db);
        float x = bounds.x + (frequency / max_frequency) * bounds.width;
        float y = bounds.y + bounds.height - ((db - min_db) / (max_db - min_db)) * bounds.height;
        Vector2 current = {x, y};
        if (has_previous) {
            DrawLineV(previous, current, (Color){62, 145, 242, 255});
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
    theme_draw_heading(g_theme, "Detected peaks", bounds.x, bounds.y, 18.0f, g_theme->text);
    float rows_y = bounds.y + theme_scaled_size(g_theme, 18.0f) + 8.0f;
    float row_step = theme_scaled_size(g_theme, 16.0f) + 5.0f;
    int capacity = (int)((bounds.y + bounds.height - rows_y) / row_step);
    if (capacity < 1) capacity = 1;
    if (capacity > 5) capacity = 5;
    int shown = peak_count < capacity ? peak_count : capacity;
    for (int i = 0; i < shown; i++) {
        char text[96];
        snprintf(text, sizeof(text), "%2d. %7.1f Hz  %6.1f dB", i + 1, peaks[i].frequency, peaks[i].db);
        theme_draw_text(g_theme, text, bounds.x, rows_y + (float)i * row_step, 16.0f, g_theme->muted_text);
    }
    if (shown == 0) {
        theme_draw_text(g_theme, "No peaks above threshold yet.", bounds.x, rows_y, 16.0f, g_theme->muted_text);
    }
}

static void draw_pitch_readout(Rectangle bounds, const PitchEstimate *pitch) {
    theme_draw_heading(g_theme, "Estimated pitch", bounds.x, bounds.y, 17.0f, g_theme->text);
    float frequency_y = bounds.y + theme_scaled_size(g_theme, 17.0f) + 7.0f;
    if (pitch == NULL || !pitch->valid) {
        theme_draw_heading(g_theme, "No stable pitch", bounds.x, frequency_y, 20.0f, g_theme->muted_text);
        theme_draw_text(g_theme, "Raise a harmonic above silence.", bounds.x,
                        frequency_y + theme_scaled_size(g_theme, 20.0f) + 7.0f, 13.0f,
                        g_theme->muted_text);
        return;
    }

    char frequency_text[48];
    char note_text[64];
    char confidence_text[48];
    snprintf(frequency_text, sizeof(frequency_text), "%.2f Hz", pitch->frequency_hz);
    snprintf(note_text,
             sizeof(note_text),
             "%s%d  |  %+.1f cents",
             pitch_note_name(pitch->midi_note),
             pitch_note_octave(pitch->midi_note),
             pitch->cents);
    snprintf(confidence_text, sizeof(confidence_text), "CONFIDENCE  %d%%", (int)(pitch->confidence * 100.0f + 0.5f));

    theme_draw_heading(g_theme, frequency_text, bounds.x, frequency_y, 22.0f, g_theme->text);
    float note_y = frequency_y + theme_scaled_size(g_theme, 22.0f) + 6.0f;
    theme_draw_text(g_theme, note_text, bounds.x, note_y, 14.0f, g_theme->muted_text);
    float badge_y = note_y + theme_scaled_size(g_theme, 14.0f) + 9.0f;
    shell_draw_badge(g_theme, (Rectangle){bounds.x, badge_y, 164.0f, 30.0f}, confidence_text,
                     pitch->confidence >= 0.90f ? (Color){70, 190, 120, 255} : (Color){229, 160, 62, 255});
}

int main(void) {
    windows_app_prepare_process();
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Spectra - Fourier Additive Synth Desktop");
    set_spectra_window_icon();
    windows_app_apply_window_icon(GetWindowHandle());
    SetWindowMinSize(MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
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
    PitchEstimate pitch = {0};
    AudioClip clip;
    audio_clip_init(&clip);
    AppPage active_page = APP_PAGE_OVERVIEW;

    while (!WindowShouldClose()) {
#if defined(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
        double frame_started_at = GetTime();
#endif

        bool toggle_fullscreen_requested = IsKeyPressed(KEY_F11);

        if (IsKeyPressed(KEY_ONE)) active_page = APP_PAGE_OVERVIEW;
        if (IsKeyPressed(KEY_TWO)) active_page = APP_PAGE_SYNTH;
        if (IsKeyPressed(KEY_THREE)) active_page = APP_PAGE_ANALYSIS;
        if (IsKeyPressed(KEY_FOUR)) active_page = APP_PAGE_HARMONIC_LAB;
        if (IsKeyPressed(KEY_FIVE)) active_page = APP_PAGE_SPECTROGRAM;

        if (dirty) {
            sample_buffer_free(&generated);
            spectrum_free(&spectrum);
            generated = render_tone(fundamental, harmonics, duration_seconds, master_gain);
            spectrum = compute_magnitude_spectrum(generated.samples, generated.count, generated.sample_rate);
            peak_count = find_peaks(&spectrum, 20.0f, 8000.0f, -55.0f, peaks, MAX_PEAKS);
            pitch = estimate_pitch(&generated, 40.0f, 1200.0f);
            audio_clip_set_samples(&clip, &generated);
            dirty = false;
        }

        BeginDrawing();
        ClearBackground(theme.background);

        AppShellFrame shell_frame = draw_app_shell(&theme, active_page, audio_ready);
        active_page = shell_frame.page;
        if (shell_frame.toggle_fullscreen) toggle_fullscreen_requested = true;

        if (active_page == APP_PAGE_OVERVIEW) {
            AppPage requested_page = draw_overview_page(&theme, shell_frame.workspace);
            if (requested_page != APP_PAGE_COUNT) active_page = requested_page;
        } else if (active_page == APP_PAGE_SYNTH) {
            Rectangle workspace = shell_frame.workspace;
            float panel_gap = 16.0f;
            float synth_width = fminf(520.0f, fmaxf(460.0f, workspace.width * 0.36f));
            Rectangle synth_panel = {workspace.x, workspace.y, synth_width, workspace.height};
            Rectangle visual_panel = {synth_panel.x + synth_panel.width + panel_gap, workspace.y,
                                      workspace.width - synth_panel.width - panel_gap, workspace.height};
            draw_panel(&theme, synth_panel, "Harmonic Synth");
            draw_panel(&theme, visual_panel, "Visualization");

            float controls_x = synth_panel.x + 20.0f;
            float controls_width = synth_panel.width - 40.0f;
            float button_space = controls_width - 20.0f;
            float play_width = button_space * 0.30f;
            float stop_width = button_space * 0.27f;
            Rectangle play_button = {controls_x, synth_panel.y + 68.0f, play_width, 44};
            Rectangle stop_button = {play_button.x + play_button.width + 10.0f, play_button.y, stop_width, 44};
            Rectangle export_button = {stop_button.x + stop_button.width + 10.0f, play_button.y,
                                       controls_x + controls_width - stop_button.x - stop_button.width - 10.0f, 44};
            if (draw_button(&theme, play_button, "Play", theme.accent) || IsKeyPressed(KEY_SPACE)) {
                if (!audio_ready) {
                    snprintf(status, sizeof(status), "No audio device detected. Check Windows audio output.");
                } else if (audio_clip_play(&clip)) {
                    snprintf(status, sizeof(status), "Playing %.1f Hz %s tone.", fundamental, PRESETS[selected_preset].name);
                } else {
                    snprintf(status, sizeof(status), "Playback buffer was not ready. Adjust a control and try again.");
                }
            }
            if (draw_button(&theme, stop_button, "Stop", (Color){226, 146, 58, 255})) {
                audio_clip_stop(&clip);
                snprintf(status, sizeof(status), "Stopped playback.");
            }
            if (draw_button(&theme, export_button, "Export WAV", theme.blue)) {
                if (export_samples_to_wav("spectra-tone.wav", &generated)) {
                    snprintf(status, sizeof(status), "Exported spectra-tone.wav");
                } else {
                    snprintf(status, sizeof(status), "Could not export spectra-tone.wav");
                }
            }

            DrawText(status, (int)controls_x, (int)synth_panel.y + 122, 12, theme.muted_text);
            float new_fundamental =
                draw_slider(&theme, (Rectangle){controls_x, synth_panel.y + 154.0f, controls_width, 42},
                            "Fundamental frequency (Hz)", fundamental, 40.0f, 1200.0f);
            float new_duration =
                draw_slider(&theme, (Rectangle){controls_x, synth_panel.y + 214.0f, controls_width, 42},
                            "Duration (seconds)", duration_seconds, 0.2f, 3.0f);
            float new_gain = draw_slider(&theme, (Rectangle){controls_x, synth_panel.y + 274.0f, controls_width, 42},
                                         "Master gain", master_gain, 0.05f, 1.0f);
            if (fabsf(new_fundamental - fundamental) > 0.01f || fabsf(new_duration - duration_seconds) > 0.001f ||
                fabsf(new_gain - master_gain) > 0.001f) {
                fundamental = new_fundamental;
                duration_seconds = new_duration;
                master_gain = new_gain;
                dirty = true;
            }

            theme_draw_heading(&theme, "Presets", controls_x, synth_panel.y + 334.0f, 16.0f, theme.text);
            float preset_width = (controls_width - 16.0f) / 3.0f;
            for (int i = 0; i < PRESET_COUNT; i++) {
                Rectangle bounds = {controls_x + (float)(i % 3) * (preset_width + 8.0f),
                                    synth_panel.y + 370.0f + (float)(i / 3) * 44.0f, preset_width, 36.0f};
                Color accent = selected_preset == (PresetId)i ? theme.accent : theme.muted_text;
                if (draw_button(&theme, bounds, PRESETS[i].name, accent)) {
                    selected_preset = (PresetId)i;
                    apply_preset(selected_preset, harmonics);
                    dirty = true;
                    snprintf(status, sizeof(status), "Loaded %s: %s.", PRESETS[i].name, PRESETS[i].description);
                }
            }

            theme_draw_heading(&theme, "Harmonics", controls_x, synth_panel.y + 462.0f, 16.0f, theme.text);
            float harmonic_width = (controls_width - 30.0f) / 4.0f;
            float harmonic_row_gap = fmaxf(34.0f, fminf(52.0f, (synth_panel.height - 544.0f) / 3.0f));
            for (int i = 0; i < HARMONIC_COUNT; i++) {
                int column = i % 4;
                int row = i / 4;
                char label[32];
                snprintf(label, sizeof(label), "H%d", i + 1);
                Rectangle slider = {controls_x + (float)column * (harmonic_width + 10.0f),
                                    synth_panel.y + 500.0f + (float)row * harmonic_row_gap, harmonic_width, 34.0f};
                float new_amplitude = draw_slider(&theme, slider, label, harmonics[i].amplitude, 0.0f, 1.0f);
                if (fabsf(new_amplitude - harmonics[i].amplitude) > 0.001f) {
                    harmonics[i].amplitude = new_amplitude;
                    dirty = true;
                }
            }

            float visual_x = visual_panel.x + 24.0f;
            float visual_width = visual_panel.width - 48.0f;
            float waveform_height = fminf(186.0f, fmaxf(110.0f, visual_panel.height * 0.24f));
            theme_draw_heading(&theme, "Waveform", visual_x, visual_panel.y + 70.0f, 17.0f, theme.text);
            Rectangle waveform_bounds = {visual_x, visual_panel.y + 112.0f, visual_width, waveform_height};
            draw_waveform(waveform_bounds, &generated);

            float spectrum_title_y = waveform_bounds.y + waveform_bounds.height + 28.0f;
            theme_draw_heading(&theme, "Frequency spectrum", visual_x, spectrum_title_y, 17.0f, theme.text);
            float bottom_height = 170.0f;
            float bottom_y = visual_panel.y + visual_panel.height - bottom_height - 22.0f;
            Rectangle spectrum_bounds = {visual_x, spectrum_title_y + 30.0f, visual_width,
                                         bottom_y - spectrum_title_y - 52.0f};
            draw_spectrum(spectrum_bounds, &spectrum, peaks, peak_count);

            float readout_width = visual_width * 0.46f;
            draw_peak_readout((Rectangle){visual_x, bottom_y, readout_width, bottom_height}, peaks, peak_count);
            float pitch_x = visual_x + readout_width + 28.0f;
            draw_pitch_readout((Rectangle){pitch_x, bottom_y, visual_width - readout_width - 28.0f, bottom_height},
                               &pitch);
        } else if (active_page == APP_PAGE_ANALYSIS) {
            draw_analysis_page(&theme, shell_frame.workspace);
        } else if (active_page == APP_PAGE_HARMONIC_LAB) {
            draw_harmonic_lab_page(&theme, shell_frame.workspace);
        } else if (active_page == APP_PAGE_SPECTROGRAM) {
            draw_spectrogram_page(&theme, shell_frame.workspace);
        } else if (active_page == APP_PAGE_SETTINGS) {
            draw_settings_page(&theme, shell_frame.workspace, audio_ready);
        }

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

        if (toggle_fullscreen_requested) {
            ToggleBorderlessWindowed();
        }
    }

    audio_clip_unload(&clip);
    spectrum_free(&spectrum);
    sample_buffer_free(&generated);
    CloseAudioDevice();
    theme_unload(&theme);
    CloseWindow();
    return 0;
}





