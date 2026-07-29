#include "audio/audio_engine.h"
#include "audio/audio_import.h"
#include "audio/reconstruction_cache.h"
#include "dsp/additive_synth.h"
#include "dsp/channel_mix.h"
#include "dsp/fft.h"
#include "dsp/fourier_reconstruction.h"
#include "dsp/global_fourier_reconstruction.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/harmonic_resynthesis.h"
#include "dsp/pitch_detection.h"
#include "dsp/signal_utils.h"
#include "dsp/stft_reconstruction.h"
#include "dsp/waveform_summary.h"
#include "platform/background_task.h"
#include "platform/windows_app.h"
#include "ui/app_shell.h"
#include "ui/help_center.h"
#include "ui/widgets.h"
#include "ui/theme.h"

#include "raylib.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#define ANALYSIS_MAX_PEAKS 64
#define ANALYSIS_WAVEFORM_BINS 2048
#define EXTRACTED_HARMONIC_COUNT 24
#define IMPORT_PATH_CAPACITY 1024
#define IMPORT_TEXT_CAPACITY 256
#define STFT_WINDOW_SIZE 2048U
#define STFT_HOP_SIZE 512U
#define STFT_DEFAULT_TOP_COMPONENTS 100
#define STFT_MAX_TOP_COMPONENTS ((int)(STFT_WINDOW_SIZE / 2U - 1U))
#define GLOBAL_DEFAULT_TOP_COMPONENTS 5
#define GLOBAL_DEFAULT_ENERGY_TARGET 0.90f
#define STFT_SPECTROGRAM_TIME_BINS 256U
#define STFT_SPECTROGRAM_FREQUENCY_BINS 128U
#define STFT_MAXIMUM_FREQUENCY 12000.0f
#define STFT_MAX_DURATION_SECONDS 600.0f
#define GLOBAL_FOURIER_OPERATION_BATCH 65536U
#define RECONSTRUCTION_CHANNEL_LIMIT 2U
#define RECONSTRUCTION_CACHE_BYTE_LIMIT \
    ((size_t)256U * 1024U * 1024U)
#define GLOBAL_MODEL_MEMORY_LIMIT \
    ((size_t)768U * 1024U * 1024U)
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

typedef enum {
    GLOBAL_BACKGROUND_IDLE = 0,
    GLOBAL_BACKGROUND_ANALYSIS,
    GLOBAL_BACKGROUND_RECONSTRUCTION
} GlobalBackgroundWork;

typedef struct {
    ImportedAudio audio;
    AudioClip full_clip;
    AudioClip region_clip;
    Spectrum spectrum;
    Peak peaks[ANALYSIS_MAX_PEAKS];
    Peak interpolated_peaks[ANALYSIS_MAX_PEAKS];
    int peak_count;
    int interpolated_peak_count;
    PitchEstimate pitch;
    ExtractedHarmonic harmonics[EXTRACTED_HARMONIC_COUNT];
    int harmonic_count;
    int detected_harmonic_count;
    SampleBuffer harmonic_resynthesis;
    AudioClip harmonic_clip;
    FourierFrameAnalysis fourier_analysis;
    SampleBuffer fourier_reconstruction;
    AudioClip frame_original_clip;
    AudioClip fourier_clip;
    GlobalFourierJob global_fourier_job;
    GlobalFourierJob global_fourier_job_right;
    StftReconstructionJob stft_job;
    StftReconstructionJob adaptive_stft_job;
    StftReconstructionJob adaptive_stft_job_right;
    SpectrogramData spectrogram;
    InterleavedBuffer global_reconstruction;
    AudioClip global_clip;
    InterleavedBuffer adaptive_reconstruction;
    AudioClip adaptive_clip;
    ReconstructionCache reconstruction_cache;
    BackgroundTask spectrogram_task;
    BackgroundTask global_task;
    BackgroundTask adaptive_task;
    GlobalBackgroundWork global_background_work;
    Texture2D spectrogram_texture;
    size_t stft_frame_count;
    unsigned int reconstruction_channels;
    unsigned int global_reconstruction_channels;
    GlobalFourierChannelMode global_channel_mode;
    size_t global_model_memory_limit_bytes;
    int fourier_top_components;
    int fourier_rendered_components;
    int global_bin_budget_components;
    int global_selected_components;
    int global_rendered_components;
    float global_retained_energy;
    float global_selected_energy_target;
    GlobalFourierSelectionMode global_selection_mode;
    int adaptive_selected_components;
    int adaptive_rendered_components;
    float adaptive_retained_energy;
    float analyzed_region_start_seconds;
    float analyzed_region_duration_seconds;
    bool harmonic_ready;
    bool fourier_ready;
    bool global_ready;
    bool adaptive_ready;
    bool spectrogram_texture_loaded;
    FullFileReconstructionMode full_file_mode;
    float waveform_minimums[ANALYSIS_WAVEFORM_BINS];
    float waveform_maximums[ANALYSIS_WAVEFORM_BINS];
    size_t waveform_bin_count;
    float region_start_seconds;
    float region_duration_seconds;
    bool analyzed;
    unsigned int analysis_revision;
    char path[IMPORT_PATH_CAPACITY];
    char file_name[IMPORT_TEXT_CAPACITY];
    char status[IMPORT_TEXT_CAPACITY];
    char lab_status[IMPORT_TEXT_CAPACITY];
    char global_status[IMPORT_TEXT_CAPACITY];
    char adaptive_status[IMPORT_TEXT_CAPACITY];
} ImportedAnalysisState;

typedef enum {
    PLAYBACK_SYNTH = 0,
    PLAYBACK_ANALYSIS_FULL,
    PLAYBACK_ANALYSIS_REGION,
    PLAYBACK_LAB_ORIGINAL,
    PLAYBACK_LAB_HARMONIC,
    PLAYBACK_LAB_FRAME,
    PLAYBACK_LAB_FOURIER,
    PLAYBACK_SPECTROGRAM_ORIGINAL,
    PLAYBACK_SPECTROGRAM_RECONSTRUCTION
} PlaybackTarget;

static void imported_analysis_init(ImportedAnalysisState *state) {
    memset(state, 0, sizeof(*state));
    imported_audio_init(&state->audio);
    audio_clip_init(&state->full_clip);
    audio_clip_init(&state->region_clip);
    audio_clip_init(&state->harmonic_clip);
    audio_clip_init(&state->frame_original_clip);
    audio_clip_init(&state->fourier_clip);
    audio_clip_init(&state->global_clip);
    audio_clip_init(&state->adaptive_clip);
    fourier_frame_analysis_init(&state->fourier_analysis);
    global_fourier_job_init(&state->global_fourier_job);
    global_fourier_job_init(&state->global_fourier_job_right);
    stft_reconstruction_job_init(&state->stft_job);
    stft_reconstruction_job_init(&state->adaptive_stft_job);
    stft_reconstruction_job_init(
        &state->adaptive_stft_job_right);
    reconstruction_cache_init(
        &state->reconstruction_cache,
        RECONSTRUCTION_CACHE_BYTE_LIMIT);
    state->global_model_memory_limit_bytes =
        GLOBAL_MODEL_MEMORY_LIMIT;
    state->global_channel_mode =
        GLOBAL_FOURIER_CHANNEL_MONO;
    background_task_init(&state->spectrogram_task);
    background_task_init(&state->global_task);
    background_task_init(&state->adaptive_task);
    state->global_selected_components = GLOBAL_DEFAULT_TOP_COMPONENTS;
    state->global_bin_budget_components =
        GLOBAL_DEFAULT_TOP_COMPONENTS;
    state->global_selected_energy_target =
        GLOBAL_DEFAULT_ENERGY_TARGET;
    state->global_selection_mode =
        GLOBAL_SELECTION_PADDED_FFT_BINS;
    state->adaptive_selected_components = STFT_DEFAULT_TOP_COMPONENTS;
    state->full_file_mode = FULL_FILE_MODE_FIXED_GLOBAL;
    snprintf(state->status, sizeof(state->status), "Choose or drop an audio file to begin.");
    snprintf(state->lab_status, sizeof(state->lab_status), "Analyze audio to build reconstruction models.");
    snprintf(state->global_status,
             sizeof(state->global_status),
             "Import audio to rank a zero-padded whole-file FFT.");
    snprintf(state->adaptive_status,
             sizeof(state->adaptive_status),
             "Import audio to build a time-varying per-frame reconstruction.");
}

static unsigned char mix_color_channel(unsigned char from, unsigned char to, float amount) {
    return (unsigned char)((float)from + ((float)to - (float)from) * amount + 0.5f);
}

static Color spectrogram_color(float db) {
    float level = (db + 100.0f) / 100.0f;
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;

    const Color stops[] = {
        {14, 16, 28, 255},
        {34, 72, 144, 255},
        {114, 69, 173, 255},
        {221, 87, 77, 255},
        {250, 207, 83, 255},
    };
    float scaled = level * 4.0f;
    int segment = (int)scaled;
    if (segment > 3) segment = 3;
    float amount = scaled - (float)segment;
    if (level >= 1.0f) amount = 1.0f;
    Color from = stops[segment];
    Color to = stops[segment + 1];
    return (Color){
        mix_color_channel(from.r, to.r, amount),
        mix_color_channel(from.g, to.g, amount),
        mix_color_channel(from.b, to.b, amount),
        255,
    };
}

static bool create_spectrogram_texture(const SpectrogramData *spectrogram, Texture2D *texture) {
    if (spectrogram == NULL || texture == NULL || spectrogram->db_values == NULL ||
        spectrogram->time_bins == 0U || spectrogram->frequency_bins == 0U) {
        return false;
    }
    if ((size_t)spectrogram->time_bins > SIZE_MAX / (size_t)spectrogram->frequency_bins) {
        return false;
    }
    size_t pixel_count =
        (size_t)spectrogram->time_bins * (size_t)spectrogram->frequency_bins;
    if (pixel_count > SIZE_MAX / sizeof(Color)) {
        return false;
    }
    Color *pixels = (Color *)malloc(pixel_count * sizeof(Color));
    if (pixels == NULL) {
        return false;
    }

    for (unsigned int y = 0U; y < spectrogram->frequency_bins; y++) {
        unsigned int source_frequency_bin = spectrogram->frequency_bins - 1U - y;
        for (unsigned int x = 0U; x < spectrogram->time_bins; x++) {
            size_t source_index =
                (size_t)source_frequency_bin * (size_t)spectrogram->time_bins + (size_t)x;
            size_t pixel_index =
                (size_t)y * (size_t)spectrogram->time_bins + (size_t)x;
            pixels[pixel_index] = spectrogram_color(spectrogram->db_values[source_index]);
        }
    }

    Image image = {
        .data = pixels,
        .width = (int)spectrogram->time_bins,
        .height = (int)spectrogram->frequency_bins,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    };
    *texture = LoadTextureFromImage(image);
    free(pixels);
    if (texture->id == 0U) {
        *texture = (Texture2D){0};
        return false;
    }
    SetTextureFilter(*texture, TEXTURE_FILTER_BILINEAR);
    return true;
}

static GlobalFourierJob *global_job_for_channel(
    ImportedAnalysisState *state,
    unsigned int channel) {
    if (state == NULL ||
        channel >= state->global_reconstruction_channels) {
        return NULL;
    }
    return channel == 0U ? &state->global_fourier_job
                         : &state->global_fourier_job_right;
}

static StftReconstructionJob *adaptive_job_for_channel(
    ImportedAnalysisState *state,
    unsigned int channel) {
    if (state == NULL || channel >= state->reconstruction_channels) {
        return NULL;
    }
    return channel == 0U ? &state->adaptive_stft_job
                         : &state->adaptive_stft_job_right;
}

static void spectrogram_background_run(
    BackgroundTaskControl *control,
    void *context) {
    ImportedAnalysisState *state =
        (ImportedAnalysisState *)context;
    while (state->stft_job.active &&
           !background_task_cancel_requested(control)) {
        stft_reconstruction_job_process(&state->stft_job, 4U);
        background_task_report_progress(
            control,
            stft_reconstruction_job_progress(&state->stft_job));
    }
    if (state->stft_job.complete) {
        background_task_report_progress(control, 1.0f);
    }
}

static void global_background_run(
    BackgroundTaskControl *control,
    void *context) {
    ImportedAnalysisState *state =
        (ImportedAnalysisState *)context;
    for (;;) {
        bool any_active = false;
        float progress_sum = 0.0f;
        for (unsigned int channel = 0U;
             channel < state->global_reconstruction_channels;
             channel++) {
            GlobalFourierJob *job =
                global_job_for_channel(state, channel);
            if (job != NULL && job->active) {
                any_active = true;
                global_fourier_job_process(
                    job, GLOBAL_FOURIER_OPERATION_BATCH);
            }
            if (job != NULL) {
                progress_sum +=
                    global_fourier_job_progress(job);
            }
        }
        if (state->global_reconstruction_channels > 0U) {
            background_task_report_progress(
                control,
                progress_sum /
                    (float)state
                        ->global_reconstruction_channels);
        }
        if (!any_active ||
            background_task_cancel_requested(control)) {
            break;
        }
    }
}

static void adaptive_background_run(
    BackgroundTaskControl *control,
    void *context) {
    ImportedAnalysisState *state =
        (ImportedAnalysisState *)context;
    for (;;) {
        bool any_active = false;
        float progress_sum = 0.0f;
        for (unsigned int channel = 0U;
             channel < state->reconstruction_channels;
             channel++) {
            StftReconstructionJob *job =
                adaptive_job_for_channel(state, channel);
            if (job != NULL && job->active) {
                any_active = true;
                stft_reconstruction_job_process(job, 2U);
            }
            if (job != NULL) {
                progress_sum +=
                    stft_reconstruction_job_progress(job);
            }
        }
        if (state->reconstruction_channels > 0U) {
            background_task_report_progress(
                control,
                progress_sum /
                    (float)state->reconstruction_channels);
        }
        if (!any_active ||
            background_task_cancel_requested(control)) {
            break;
        }
    }
}

static bool global_analysis_ready(
    const ImportedAnalysisState *state) {
    if (state == NULL ||
        state->global_reconstruction_channels == 0U) {
        return false;
    }
    if (!state->global_fourier_job.analysis_ready) return false;
    return state->global_reconstruction_channels == 1U ||
           state->global_fourier_job_right.analysis_ready;
}

static bool global_reconstruction_ready(
    const ImportedAnalysisState *state) {
    if (state == NULL ||
        state->global_reconstruction_channels == 0U) {
        return false;
    }
    if (!state->global_fourier_job.reconstruction_ready) {
        return false;
    }
    return state->global_reconstruction_channels == 1U ||
           state->global_fourier_job_right.reconstruction_ready;
}

static float combined_global_retained_energy(
    const ImportedAnalysisState *state,
    int component_count) {
    if (state == NULL || component_count <= 0) return 0.0f;
    double retained = 0.0;
    double total = 0.0;
    for (unsigned int channel = 0U;
         channel < state->global_reconstruction_channels;
         channel++) {
        const GlobalFourierJob *job =
            channel == 0U ? &state->global_fourier_job
                          : &state->global_fourier_job_right;
        int count = component_count;
        if (count > job->component_count) {
            count = job->component_count;
        }
        if (count > 0 && job->components != NULL) {
            retained += job->components[count - 1].power;
            total += job->total_spectral_energy;
        }
    }
    return total > 0.0 ? (float)(retained / total) : 0.0f;
}

static float combined_adaptive_retained_energy(
    const ImportedAnalysisState *state) {
    if (state == NULL) return 0.0f;
    double retained = 0.0;
    double total = 0.0;
    for (unsigned int channel = 0U;
         channel < state->reconstruction_channels;
         channel++) {
        const StftReconstructionJob *job =
            channel == 0U ? &state->adaptive_stft_job
                          : &state->adaptive_stft_job_right;
        retained += job->retained_spectral_energy;
        total += job->total_spectral_energy;
    }
    return total > 0.0 ? (float)(retained / total) : 0.0f;
}

static void imported_full_file_unload(ImportedAnalysisState *state) {
    if (state == NULL) {
        return;
    }
    background_task_cancel_and_join(&state->spectrogram_task);
    background_task_cancel_and_join(&state->global_task);
    background_task_cancel_and_join(&state->adaptive_task);
    state->global_background_work = GLOBAL_BACKGROUND_IDLE;
    audio_clip_unload(&state->global_clip);
    audio_clip_unload(&state->adaptive_clip);
    global_fourier_job_free(&state->global_fourier_job);
    global_fourier_job_free(
        &state->global_fourier_job_right);
    stft_reconstruction_job_free(&state->stft_job);
    stft_reconstruction_job_free(&state->adaptive_stft_job);
    stft_reconstruction_job_free(
        &state->adaptive_stft_job_right);
    interleaved_buffer_free(&state->global_reconstruction);
    interleaved_buffer_free(&state->adaptive_reconstruction);
    reconstruction_cache_free(&state->reconstruction_cache);
    reconstruction_cache_init(
        &state->reconstruction_cache,
        RECONSTRUCTION_CACHE_BYTE_LIMIT);
    spectrogram_data_free(&state->spectrogram);
    if (state->spectrogram_texture_loaded) {
        UnloadTexture(state->spectrogram_texture);
    }
    state->spectrogram_texture = (Texture2D){0};
    state->spectrogram_texture_loaded = false;
    state->stft_frame_count = 0U;
    state->reconstruction_channels = 0U;
    state->global_reconstruction_channels = 0U;
    state->global_channel_mode =
        GLOBAL_FOURIER_CHANNEL_MONO;
    state->global_selected_components = GLOBAL_DEFAULT_TOP_COMPONENTS;
    state->global_bin_budget_components =
        GLOBAL_DEFAULT_TOP_COMPONENTS;
    state->global_rendered_components = 0;
    state->global_retained_energy = 0.0f;
    state->global_selected_energy_target =
        GLOBAL_DEFAULT_ENERGY_TARGET;
    state->global_selection_mode =
        GLOBAL_SELECTION_PADDED_FFT_BINS;
    state->global_ready = false;
    state->adaptive_selected_components = STFT_DEFAULT_TOP_COMPONENTS;
    state->adaptive_rendered_components = 0;
    state->adaptive_retained_energy = 0.0f;
    state->adaptive_ready = false;
    state->full_file_mode = FULL_FILE_MODE_FIXED_GLOBAL;
}

static size_t global_model_estimated_bytes(
    const ImportedAnalysisState *state,
    GlobalFourierChannelMode channel_mode) {
    if (state == NULL || state->audio.mono.count == 0U) {
        return 0U;
    }
    int maximum_components =
        global_fourier_available_component_count(
            state->audio.mono.count);
    unsigned int channel_count =
        channel_mode == GLOBAL_FOURIER_CHANNEL_SOURCE
            ? state->reconstruction_channels
            : 1U;
    return global_fourier_estimated_multichannel_analysis_bytes(
        state->audio.mono.count,
        maximum_components,
        channel_count);
}

static const char *global_channel_mode_label(
    const ImportedAnalysisState *state) {
    return state != NULL &&
                   state->global_reconstruction_channels > 1U
               ? "Stereo FFT"
               : "Mono FFT";
}

static const char *global_bin_scope(
    const ImportedAnalysisState *state) {
    return state != NULL &&
                   state->global_reconstruction_channels > 1U
               ? "padded FFT bins per channel"
               : "padded FFT bins";
}

static bool begin_global_model_analysis(
    ImportedAnalysisState *state,
    GlobalFourierChannelMode requested_mode) {
    if (state == NULL || state->audio.mono.samples == NULL ||
        state->audio.mono.count == 0U ||
        state->reconstruction_channels == 0U) {
        return false;
    }

    if (state->reconstruction_channels == 1U) {
        requested_mode = GLOBAL_FOURIER_CHANNEL_MONO;
    }
    unsigned int channel_count =
        requested_mode == GLOBAL_FOURIER_CHANNEL_SOURCE
            ? state->reconstruction_channels
            : 1U;
    int available_components =
        global_fourier_available_component_count(
            state->audio.mono.count);
    size_t estimated_bytes =
        global_fourier_estimated_multichannel_analysis_bytes(
            state->audio.mono.count,
            available_components,
            channel_count);

    background_task_cancel_and_join(&state->global_task);
    state->global_background_work = GLOBAL_BACKGROUND_IDLE;
    audio_clip_unload(&state->global_clip);
    interleaved_buffer_free(&state->global_reconstruction);
    global_fourier_job_free(&state->global_fourier_job);
    global_fourier_job_free(
        &state->global_fourier_job_right);
    state->global_ready = false;
    state->global_rendered_components = 0;
    state->global_retained_energy = 0.0f;
    state->global_channel_mode = requested_mode;
    state->global_reconstruction_channels = channel_count;

    if (available_components <= 0 || estimated_bytes == 0U) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not determine the whole-file FFT memory requirement.");
        return false;
    }
    if (estimated_bytes >
        state->global_model_memory_limit_bytes) {
        double estimated_megabytes =
            (double)estimated_bytes / (1024.0 * 1024.0);
        double limit_megabytes =
            (double)state->global_model_memory_limit_bytes /
            (1024.0 * 1024.0);
        if (channel_count > 1U) {
            snprintf(
                state->global_status,
                sizeof(state->global_status),
                "Stereo %.0f MB > %.0f MB limit. See Settings.",
                estimated_megabytes,
                limit_megabytes);
        } else {
            snprintf(
                state->global_status,
                sizeof(state->global_status),
                "Mono %.0f MB > %.0f MB limit. See Settings.",
                estimated_megabytes,
                limit_megabytes);
        }
        return false;
    }

    bool started = true;
    if (requested_mode == GLOBAL_FOURIER_CHANNEL_MONO) {
        started = global_fourier_job_begin_analysis(
            &state->global_fourier_job,
            &state->audio.mono,
            available_components);
    } else {
        for (unsigned int channel = 0U;
             channel < channel_count;
             channel++) {
            GlobalFourierJob *job =
                global_job_for_channel(state, channel);
            started =
                global_fourier_job_begin_analysis_strided(
                    job,
                    state->audio.interleaved.samples + channel,
                    state->audio.interleaved.frame_count,
                    state->audio.interleaved.channel_count,
                    state->audio.interleaved.sample_rate,
                    available_components);
            if (!started) break;
        }
    }
    if (!started) {
        global_fourier_job_free(&state->global_fourier_job);
        global_fourier_job_free(
            &state->global_fourier_job_right);
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not allocate the %s model.",
                 global_channel_mode_label(state));
        return false;
    }

    state->global_background_work =
        GLOBAL_BACKGROUND_ANALYSIS;
    if (!background_task_start(&state->global_task,
                               global_background_run,
                               state)) {
        state->global_background_work =
            GLOBAL_BACKGROUND_IDLE;
        global_fourier_job_free(&state->global_fourier_job);
        global_fourier_job_free(
            &state->global_fourier_job_right);
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not start the background Fourier worker.");
        return false;
    }
    snprintf(
        state->global_status,
        sizeof(state->global_status),
        channel_count > 1U
            ? "Ranking %d one-sided bins per channel in Stereo FFT..."
            : "Ranking %d one-sided bins in Mono FFT...",
        available_components);
    return true;
}

static bool start_full_file_analysis(ImportedAnalysisState *state) {
    if (state == NULL || state->audio.mono.samples == NULL ||
        state->audio.mono.count == 0U ||
        state->audio.interleaved.samples == NULL ||
        state->audio.interleaved.frame_count == 0U ||
        state->audio.interleaved.channel_count == 0U ||
        state->audio.interleaved.channel_count >
            RECONSTRUCTION_CHANNEL_LIMIT) {
        return false;
    }

    if (state->audio.duration_seconds > STFT_MAX_DURATION_SECONDS) {
        background_task_cancel_and_join(
            &state->spectrogram_task);
        background_task_cancel_and_join(&state->global_task);
        background_task_cancel_and_join(
            &state->adaptive_task);
        global_fourier_job_free(&state->global_fourier_job);
        global_fourier_job_free(
            &state->global_fourier_job_right);
        stft_reconstruction_job_free(&state->stft_job);
        stft_reconstruction_job_free(&state->adaptive_stft_job);
        stft_reconstruction_job_free(
            &state->adaptive_stft_job_right);
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "This file is longer than the 10-minute reconstruction safety limit.");
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "This file is longer than the 10-minute reconstruction safety limit.");
        return false;
    }

    background_task_cancel_and_join(&state->spectrogram_task);
    background_task_cancel_and_join(&state->global_task);
    background_task_cancel_and_join(&state->adaptive_task);
    state->global_background_work = GLOBAL_BACKGROUND_IDLE;
    audio_clip_unload(&state->global_clip);
    audio_clip_unload(&state->adaptive_clip);
    interleaved_buffer_free(&state->global_reconstruction);
    interleaved_buffer_free(&state->adaptive_reconstruction);
    reconstruction_cache_free(&state->reconstruction_cache);
    reconstruction_cache_init(
        &state->reconstruction_cache,
        RECONSTRUCTION_CACHE_BYTE_LIMIT);
    global_fourier_job_free(&state->global_fourier_job);
    global_fourier_job_free(
        &state->global_fourier_job_right);
    stft_reconstruction_job_free(&state->stft_job);
    stft_reconstruction_job_free(&state->adaptive_stft_job);
    stft_reconstruction_job_free(
        &state->adaptive_stft_job_right);
    state->reconstruction_channels =
        state->audio.interleaved.channel_count;
    state->global_ready = false;
    state->global_rendered_components = 0;
    state->global_retained_energy = 0.0f;
    state->global_selected_components = GLOBAL_DEFAULT_TOP_COMPONENTS;
    state->global_bin_budget_components =
        GLOBAL_DEFAULT_TOP_COMPONENTS;
    state->global_selected_energy_target =
        GLOBAL_DEFAULT_ENERGY_TARGET;
    state->global_selection_mode =
        GLOBAL_SELECTION_PADDED_FFT_BINS;
    state->adaptive_ready = false;
    state->adaptive_rendered_components = 0;
    state->adaptive_retained_energy = 0.0f;
    state->adaptive_selected_components = STFT_DEFAULT_TOP_COMPONENTS;
    state->full_file_mode = FULL_FILE_MODE_FIXED_GLOBAL;

    float maximum_frequency =
        fminf(STFT_MAXIMUM_FREQUENCY, (float)state->audio.mono.sample_rate * 0.5f);
    int available_global_components =
        global_fourier_available_component_count(
            state->audio.mono.count);
    if (available_global_components <= 0) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not determine the complete whole-file frequency range.");
        return false;
    }
    bool spectrogram_started =
        stft_reconstruction_job_begin(&state->stft_job,
                                      &state->audio.mono,
                                      STFT_WINDOW_SIZE,
                                      STFT_HOP_SIZE,
                                      0,
                                      true,
                                      STFT_SPECTROGRAM_TIME_BINS,
                                      STFT_SPECTROGRAM_FREQUENCY_BINS,
                                      maximum_frequency);
    state->stft_frame_count =
        spectrogram_started ? state->stft_job.frame_count : 0U;
    if (spectrogram_started &&
        !background_task_start(&state->spectrogram_task,
                               spectrogram_background_run,
                               state)) {
        stft_reconstruction_job_free(&state->stft_job);
        state->stft_frame_count = 0U;
    }

    unsigned int recommended_channels =
        global_fourier_recommended_channel_count(
            state->audio.mono.count,
            available_global_components,
            state->reconstruction_channels,
            state->global_model_memory_limit_bytes);
    GlobalFourierChannelMode initial_channel_mode =
        recommended_channels > 1U
            ? GLOBAL_FOURIER_CHANNEL_SOURCE
            : GLOBAL_FOURIER_CHANNEL_MONO;
    begin_global_model_analysis(
        state, initial_channel_mode);
    snprintf(state->adaptive_status,
             sizeof(state->adaptive_status),
             "Choose Time-varying STFT to build a cached per-frame reconstruction.");
    return true;
}

static void complete_spectrogram_visualization(ImportedAnalysisState *state) {
    size_t rendered_frames = state->stft_job.frame_count;
    SpectrogramData spectrogram =
        stft_reconstruction_job_take_spectrogram(&state->stft_job);
    if (spectrogram.db_values != NULL) {
        if (state->spectrogram_texture_loaded) {
            UnloadTexture(state->spectrogram_texture);
        }
        state->spectrogram_texture = (Texture2D){0};
        state->spectrogram_texture_loaded = false;
        spectrogram_data_free(&state->spectrogram);
        state->spectrogram = spectrogram;
        state->spectrogram_texture_loaded =
            create_spectrogram_texture(&state->spectrogram, &state->spectrogram_texture);
    }
    state->stft_frame_count = rendered_frames;
    stft_reconstruction_job_free(&state->stft_job);
}

static ReconstructionCacheKey reconstruction_key(
    ReconstructionCacheMode mode,
    int component_count,
    unsigned int channel_count) {
    ReconstructionCacheKey key = {
        .mode = mode,
        .component_count = component_count,
        .channel_count = channel_count,
    };
    return key;
}

static void cache_current_global_reconstruction(
    ImportedAnalysisState *state) {
    if (state == NULL ||
        state->global_reconstruction.samples == NULL ||
        state->global_rendered_components <= 0) {
        return;
    }
    audio_clip_unload(&state->global_clip);
    ReconstructionCacheKey key = reconstruction_key(
        RECONSTRUCTION_CACHE_GLOBAL,
        state->global_rendered_components,
        state->global_reconstruction_channels);
    if (!reconstruction_cache_store_move(
            &state->reconstruction_cache,
            key,
            &state->global_reconstruction,
            state->global_retained_energy)) {
        interleaved_buffer_free(
            &state->global_reconstruction);
    }
    state->global_ready = false;
}

static bool restore_global_reconstruction_from_cache(
    ImportedAnalysisState *state,
    int component_count,
    bool audio_ready) {
    if (state == NULL) return false;
    ReconstructionCacheKey key = reconstruction_key(
        RECONSTRUCTION_CACHE_GLOBAL,
        component_count,
        state->global_reconstruction_channels);
    float retained_energy = 0.0f;
    if (!reconstruction_cache_take(
            &state->reconstruction_cache,
            key,
            &state->global_reconstruction,
            &retained_energy)) {
        return false;
    }
    bool playback_ready =
        !audio_ready ||
        audio_clip_set_interleaved(
            &state->global_clip,
            &state->global_reconstruction);
    state->global_selected_components = component_count;
    state->global_rendered_components = component_count;
    state->global_retained_energy = retained_energy;
    state->global_ready = true;
    snprintf(state->global_status,
             sizeof(state->global_status),
             playback_ready
                 ? "Restored %d %s instantly from cache."
                 : "Restored %d %s from cache; playback is unavailable.",
             component_count,
             global_bin_scope(state));
    return true;
}

static void cache_current_adaptive_reconstruction(
    ImportedAnalysisState *state) {
    if (state == NULL ||
        state->adaptive_reconstruction.samples == NULL ||
        state->adaptive_rendered_components <= 0) {
        return;
    }
    audio_clip_unload(&state->adaptive_clip);
    ReconstructionCacheKey key = reconstruction_key(
        RECONSTRUCTION_CACHE_STFT,
        state->adaptive_rendered_components,
        state->reconstruction_channels);
    if (!reconstruction_cache_store_move(
            &state->reconstruction_cache,
            key,
            &state->adaptive_reconstruction,
            state->adaptive_retained_energy)) {
        interleaved_buffer_free(
            &state->adaptive_reconstruction);
    }
    state->adaptive_ready = false;
}

static bool restore_adaptive_reconstruction_from_cache(
    ImportedAnalysisState *state,
    int component_count,
    bool audio_ready) {
    if (state == NULL) return false;
    ReconstructionCacheKey key = reconstruction_key(
        RECONSTRUCTION_CACHE_STFT,
        component_count,
        state->reconstruction_channels);
    float retained_energy = 0.0f;
    if (!reconstruction_cache_take(
            &state->reconstruction_cache,
            key,
            &state->adaptive_reconstruction,
            &retained_energy)) {
        return false;
    }
    bool playback_ready =
        !audio_ready ||
        audio_clip_set_interleaved(
            &state->adaptive_clip,
            &state->adaptive_reconstruction);
    state->adaptive_selected_components = component_count;
    state->adaptive_rendered_components = component_count;
    state->adaptive_retained_energy = retained_energy;
    state->adaptive_ready = true;
    snprintf(state->adaptive_status,
             sizeof(state->adaptive_status),
             playback_ready
                 ? "Restored time-varying Top-%d per channel instantly from cache."
                 : "Restored time-varying Top-%d per channel from cache; playback is unavailable.",
             component_count);
    return true;
}

static bool start_global_reconstruction(ImportedAnalysisState *state,
                                        int top_components,
                                        bool audio_ready) {
    if (state == NULL) return false;
    if (top_components < 1) top_components = 1;
    int maximum_components =
        state->global_fourier_job.maximum_component_count;
    if (maximum_components > 0 &&
        top_components > maximum_components) {
        top_components = maximum_components;
    }
    state->global_selected_components = top_components;
    if (state->global_background_work ==
            GLOBAL_BACKGROUND_ANALYSIS &&
        background_task_has_work(&state->global_task)) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "%d of %d %s are queued until background ranking is ready.",
                 top_components,
                 maximum_components,
                 global_bin_scope(state));
        return true;
    }
    if (background_task_has_work(&state->global_task)) {
        background_task_cancel_and_join(&state->global_task);
        state->global_background_work =
            GLOBAL_BACKGROUND_IDLE;
    }
    if (!global_analysis_ready(state)) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "The reusable whole-file Fourier model is not ready.");
        return false;
    }
    if (state->global_ready &&
        state->global_rendered_components == top_components &&
        state->global_reconstruction.samples != NULL) {
        if (state->global_selection_mode ==
            GLOBAL_SELECTION_PADDED_FFT_BINS) {
            snprintf(state->global_status,
                     sizeof(state->global_status),
                     "%d of %d %s (%.2f%%) retain %.3f%% combined energy.",
                     top_components,
                     maximum_components,
                     global_bin_scope(state),
                     maximum_components > 0
                         ? (float)top_components * 100.0f /
                               (float)maximum_components
                         : 0.0f,
                     state->global_retained_energy * 100.0f);
        }
        return true;
    }

    cache_current_global_reconstruction(state);
    state->global_rendered_components = 0;
    state->global_retained_energy = 0.0f;
    if (restore_global_reconstruction_from_cache(
            state, top_components, audio_ready)) {
        return true;
    }

    bool jobs_started = true;
    for (unsigned int channel = 0U;
         channel < state->global_reconstruction_channels;
         channel++) {
        GlobalFourierJob *job =
            global_job_for_channel(state, channel);
        if (job == NULL ||
            !global_fourier_job_begin_reconstruction(
                job, top_components)) {
            jobs_started = false;
            break;
        }
    }
    if (!jobs_started) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not allocate the %d-bin %s reconstruction.",
                 top_components,
                 global_channel_mode_label(state));
        return false;
    }
    state->global_background_work =
        GLOBAL_BACKGROUND_RECONSTRUCTION;
    if (!background_task_start(&state->global_task,
                               global_background_run,
                               state)) {
        state->global_background_work =
            GLOBAL_BACKGROUND_IDLE;
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not start the background reconstruction worker.");
        return false;
    }
    snprintf(state->global_status,
             sizeof(state->global_status),
             "Reconstructing %d of %d %s in the background (%.2f%%)...",
             top_components,
             maximum_components,
             global_bin_scope(state),
             maximum_components > 0
                 ? (float)top_components * 100.0f /
                       (float)maximum_components
                 : 0.0f);
    return true;
}

static bool start_global_energy_reconstruction(
    ImportedAnalysisState *state,
    float retained_energy_target,
    bool audio_ready) {
    if (state == NULL) return false;
    if (retained_energy_target < 0.0001f) {
        retained_energy_target = 0.0001f;
    }
    if (retained_energy_target > 1.0f) {
        retained_energy_target = 1.0f;
    }
    state->global_selected_energy_target =
        retained_energy_target;

    if (state->global_background_work ==
            GLOBAL_BACKGROUND_ANALYSIS &&
        background_task_has_work(&state->global_task)) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "%.2f%% spectral energy is queued until the padded FFT ranking is ready.",
                 retained_energy_target * 100.0f);
        return true;
    }
    if (background_task_has_work(&state->global_task)) {
        background_task_cancel_and_join(&state->global_task);
        state->global_background_work =
            GLOBAL_BACKGROUND_IDLE;
    }
    if (!global_analysis_ready(state)) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "The reusable whole-file Fourier model is not ready.");
        return false;
    }

    int resolved_components = 0;
    for (unsigned int channel = 0U;
         channel < state->global_reconstruction_channels;
         channel++) {
        GlobalFourierJob *job =
            global_job_for_channel(state, channel);
        int channel_components =
            global_fourier_job_component_count_for_energy(
                job, retained_energy_target);
        if (channel_components > resolved_components) {
            resolved_components = channel_components;
        }
    }
    if (resolved_components <= 0) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Could not resolve the requested spectral-energy target.");
        return false;
    }
    if (!start_global_reconstruction(
            state, resolved_components, audio_ready)) {
        return false;
    }
    if (state->global_ready) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "%.2f%% energy target uses %d %s and retains %.3f%% combined energy.",
                 retained_energy_target * 100.0f,
                 resolved_components,
                 global_bin_scope(state),
                 state->global_retained_energy * 100.0f);
    } else {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "Targeting %.2f%% spectral energy with %d %s...",
                 retained_energy_target * 100.0f,
                 resolved_components,
                 global_bin_scope(state));
    }
    return true;
}

static void complete_global_reconstruction(ImportedAnalysisState *state,
                                           bool audio_ready) {
    int rendered_components =
        state->global_fourier_job.rendered_component_count;
    SampleBuffer channels[RECONSTRUCTION_CHANNEL_LIMIT] = {0};
    bool output_ready = rendered_components > 0;
    for (unsigned int channel = 0U;
         channel < state->global_reconstruction_channels;
         channel++) {
        GlobalFourierJob *job =
            global_job_for_channel(state, channel);
        channels[channel] =
            global_fourier_job_take_output(job);
        if (channels[channel].samples == NULL) {
            output_ready = false;
        }
    }
    InterleavedBuffer reconstruction = {0};
    if (output_ready) {
        output_ready = interleave_sample_buffers(
            channels,
            state->global_reconstruction_channels,
            &reconstruction);
    }
    for (unsigned int channel = 0U;
         channel < state->global_reconstruction_channels;
         channel++) {
        sample_buffer_free(&channels[channel]);
    }
    if (!output_ready || reconstruction.samples == NULL) {
        interleaved_buffer_free(&reconstruction);
        state->global_ready = false;
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "The %s reconstruction did not produce audio.",
                 global_channel_mode_label(state));
        return;
    }

    audio_clip_unload(&state->global_clip);
    interleaved_buffer_free(&state->global_reconstruction);
    state->global_reconstruction = reconstruction;
    bool playback_ready =
        !audio_ready ||
        audio_clip_set_interleaved(
            &state->global_clip,
            &state->global_reconstruction);
    state->global_rendered_components = rendered_components;
    state->global_selected_components = rendered_components;
    state->global_retained_energy =
        combined_global_retained_energy(
            state, rendered_components);
    state->global_ready = true;
    if (audio_ready && !playback_ready) {
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 "The %d-bin %s reconstruction is ready for export, but playback could not be loaded.",
                 rendered_components,
                 global_channel_mode_label(state));
    } else {
        int maximum_components =
            state->global_fourier_job.maximum_component_count;
        if (state->global_selection_mode ==
            GLOBAL_SELECTION_SPECTRAL_ENERGY) {
            snprintf(state->global_status,
                     sizeof(state->global_status),
                     "%.2f%% energy target resolved to %d %s; retained energy %.3f%%.",
                     state->global_selected_energy_target *
                         100.0f,
                     rendered_components,
                     global_bin_scope(state),
                     state->global_retained_energy * 100.0f);
        } else {
            snprintf(state->global_status,
                     sizeof(state->global_status),
                     "%d of %d %s (%.2f%%) retain %.3f%% of spectral energy.",
                     rendered_components,
                     maximum_components,
                     global_bin_scope(state),
                     maximum_components > 0
                         ? (float)rendered_components *
                               100.0f /
                               (float)maximum_components
                         : 0.0f,
                     state->global_retained_energy * 100.0f);
        }
    }
}

static bool start_adaptive_reconstruction(ImportedAnalysisState *state,
                                          int top_components,
                                          bool audio_ready) {
    if (state == NULL ||
        state->audio.interleaved.samples == NULL ||
        state->audio.interleaved.frame_count == 0U ||
        state->reconstruction_channels == 0U) {
        return false;
    }
    if (top_components < 1) top_components = 1;
    if (top_components > STFT_MAX_TOP_COMPONENTS) {
        top_components = STFT_MAX_TOP_COMPONENTS;
    }

    if (background_task_has_work(&state->adaptive_task)) {
        background_task_cancel_and_join(&state->adaptive_task);
    }
    if (state->adaptive_ready &&
        state->adaptive_rendered_components == top_components &&
        state->adaptive_reconstruction.samples != NULL) {
        return true;
    }

    cache_current_adaptive_reconstruction(state);
    stft_reconstruction_job_free(&state->adaptive_stft_job);
    stft_reconstruction_job_free(
        &state->adaptive_stft_job_right);
    state->adaptive_selected_components = top_components;
    state->adaptive_rendered_components = 0;
    state->adaptive_retained_energy = 0.0f;
    state->adaptive_ready = false;
    if (restore_adaptive_reconstruction_from_cache(
            state, top_components, audio_ready)) {
        return true;
    }

    bool jobs_started = true;
    for (unsigned int channel = 0U;
         channel < state->reconstruction_channels;
         channel++) {
        StftReconstructionJob *job =
            adaptive_job_for_channel(state, channel);
        jobs_started =
            stft_reconstruction_job_begin_strided(
                job,
                state->audio.interleaved.samples + channel,
                state->audio.interleaved.frame_count,
                state->audio.interleaved.channel_count,
                state->audio.interleaved.sample_rate,
                STFT_WINDOW_SIZE,
                STFT_HOP_SIZE,
                top_components,
                false,
                0U,
                0U,
                0.0f);
        if (!jobs_started) break;
    }
    if (!jobs_started) {
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "Could not allocate the time-varying Top-%d-per-channel reconstruction.",
                 top_components);
        return false;
    }
    if (!background_task_start(&state->adaptive_task,
                               adaptive_background_run,
                               state)) {
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "Could not start the background STFT worker.");
        return false;
    }
    snprintf(state->adaptive_status,
             sizeof(state->adaptive_status),
             "Selecting %d components per channel in every STFT frame in the background...",
             top_components);
    return true;
}

static void complete_adaptive_reconstruction(ImportedAnalysisState *state,
                                             bool audio_ready) {
    int rendered_components =
        state->adaptive_stft_job.top_component_count;
    float retained_energy =
        combined_adaptive_retained_energy(state);
    SampleBuffer channels[RECONSTRUCTION_CHANNEL_LIMIT] = {0};
    bool output_ready = rendered_components > 0;
    for (unsigned int channel = 0U;
         channel < state->reconstruction_channels;
         channel++) {
        StftReconstructionJob *job =
            adaptive_job_for_channel(state, channel);
        channels[channel] =
            stft_reconstruction_job_take_output(job);
        if (channels[channel].samples == NULL) {
            output_ready = false;
        }
    }
    InterleavedBuffer reconstruction = {0};
    if (output_ready) {
        output_ready = interleave_sample_buffers(
            channels,
            state->reconstruction_channels,
            &reconstruction);
    }
    for (unsigned int channel = 0U;
         channel < state->reconstruction_channels;
         channel++) {
        sample_buffer_free(&channels[channel]);
    }
    if (!output_ready || reconstruction.samples == NULL) {
        interleaved_buffer_free(&reconstruction);
        state->adaptive_ready = false;
        stft_reconstruction_job_free(&state->adaptive_stft_job);
        stft_reconstruction_job_free(
            &state->adaptive_stft_job_right);
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "The stereo-aware time-varying reconstruction did not produce audio.");
        return;
    }

    audio_clip_unload(&state->adaptive_clip);
    interleaved_buffer_free(&state->adaptive_reconstruction);
    state->adaptive_reconstruction = reconstruction;
    bool playback_ready =
        !audio_ready ||
        audio_clip_set_interleaved(
            &state->adaptive_clip,
            &state->adaptive_reconstruction);
    state->adaptive_selected_components = rendered_components;
    state->adaptive_rendered_components = rendered_components;
    state->adaptive_retained_energy = retained_energy;
    state->adaptive_ready = true;
    stft_reconstruction_job_free(&state->adaptive_stft_job);
    stft_reconstruction_job_free(
        &state->adaptive_stft_job_right);
    if (audio_ready && !playback_ready) {
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "Time-varying Top-%d per channel is ready for export, but playback could not be loaded.",
                 rendered_components);
    } else {
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "Time-varying Top-%d per channel ready: %.1f%% combined per-frame energy retained.",
                 rendered_components,
                 retained_energy * 100.0f);
    }
}

static bool active_full_file_ready(const ImportedAnalysisState *state) {
    return state != NULL &&
           (state->full_file_mode == FULL_FILE_MODE_FIXED_GLOBAL
                ? state->global_ready
                : state->adaptive_ready);
}

static bool active_full_file_processing(
    const ImportedAnalysisState *state) {
    return state != NULL &&
           (state->full_file_mode == FULL_FILE_MODE_FIXED_GLOBAL
                ? background_task_has_work(&state->global_task)
                : background_task_has_work(&state->adaptive_task));
}

static AudioClip *active_full_file_clip(ImportedAnalysisState *state) {
    if (state == NULL) return NULL;
    return state->full_file_mode == FULL_FILE_MODE_FIXED_GLOBAL
               ? &state->global_clip
               : &state->adaptive_clip;
}

static InterleavedBuffer *active_full_file_buffer(
    ImportedAnalysisState *state) {
    if (state == NULL) return NULL;
    return state->full_file_mode == FULL_FILE_MODE_FIXED_GLOBAL
               ? &state->global_reconstruction
               : &state->adaptive_reconstruction;
}

static char *active_full_file_status(ImportedAnalysisState *state) {
    if (state == NULL) return NULL;
    return state->full_file_mode == FULL_FILE_MODE_FIXED_GLOBAL
               ? state->global_status
               : state->adaptive_status;
}

static int active_full_file_rendered_components(
    const ImportedAnalysisState *state) {
    if (state == NULL) return 0;
    return state->full_file_mode == FULL_FILE_MODE_FIXED_GLOBAL
               ? state->global_rendered_components
               : state->adaptive_rendered_components;
}

static void restore_full_file_idle_status(
    ImportedAnalysisState *state,
    FullFileReconstructionMode mode) {
    if (state == NULL) return;

    if (mode == FULL_FILE_MODE_FIXED_GLOBAL) {
        if (state->global_ready) {
            int maximum_components =
                state->global_fourier_job.maximum_component_count;
            if (state->global_selection_mode ==
                GLOBAL_SELECTION_SPECTRAL_ENERGY) {
                snprintf(state->global_status,
                         sizeof(state->global_status),
                         "%.2f%% energy target uses %d %s and retains %.3f%% combined energy.",
                         state->global_selected_energy_target *
                             100.0f,
                         state->global_rendered_components,
                         global_bin_scope(state),
                         state->global_retained_energy *
                             100.0f);
            } else {
                snprintf(state->global_status,
                         sizeof(state->global_status),
                         "%d of %d %s (%.2f%%) retain %.3f%% combined energy.",
                         state->global_rendered_components,
                         maximum_components,
                         global_bin_scope(state),
                         maximum_components > 0
                             ? (float)state
                                       ->global_rendered_components *
                                   100.0f /
                                   (float)maximum_components
                             : 0.0f,
                         state->global_retained_energy *
                             100.0f);
            }
        }
        return;
    }

    if (state->adaptive_ready) {
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "Time-varying Top-%d per channel ready: %.1f%% combined per-frame energy retained.",
                 state->adaptive_rendered_components,
                 state->adaptive_retained_energy * 100.0f);
    }
}

static void advance_full_file_analysis(ImportedAnalysisState *state,
                                       bool audio_ready) {
    if (state == NULL) return;
    if (background_task_is_complete(
            &state->spectrogram_task)) {
        background_task_join(&state->spectrogram_task);
        if (state->stft_job.complete) {
            complete_spectrogram_visualization(state);
        } else {
            stft_reconstruction_job_free(&state->stft_job);
        }
    }

    if (background_task_is_complete(&state->global_task)) {
        GlobalBackgroundWork completed_work =
            state->global_background_work;
        background_task_join(&state->global_task);
        state->global_background_work =
            GLOBAL_BACKGROUND_IDLE;
        if (completed_work == GLOBAL_BACKGROUND_ANALYSIS) {
            if (!global_analysis_ready(state)) {
                snprintf(state->global_status,
                         sizeof(state->global_status),
                         "Background whole-file Fourier analysis did not complete.");
            } else if (state->global_selection_mode ==
                       GLOBAL_SELECTION_SPECTRAL_ENERGY) {
                start_global_energy_reconstruction(
                    state,
                    state->global_selected_energy_target,
                    audio_ready);
            } else {
                start_global_reconstruction(
                    state,
                    state->global_bin_budget_components,
                    audio_ready);
            }
        } else if (
            completed_work ==
            GLOBAL_BACKGROUND_RECONSTRUCTION) {
            if (global_reconstruction_ready(state)) {
                complete_global_reconstruction(
                    state, audio_ready);
            } else {
                snprintf(state->global_status,
                         sizeof(state->global_status),
                         "Background whole-file reconstruction did not complete.");
            }
        }
    }

    if (background_task_is_complete(&state->adaptive_task)) {
        background_task_join(&state->adaptive_task);
        bool complete = state->adaptive_stft_job.complete;
        if (state->reconstruction_channels > 1U) {
            complete =
                complete &&
                state->adaptive_stft_job_right.complete;
        }
        if (complete) {
            complete_adaptive_reconstruction(
                state, audio_ready);
        } else {
            snprintf(state->adaptive_status,
                     sizeof(state->adaptive_status),
                     "Background time-varying reconstruction did not complete.");
        }
    }

    if (background_task_has_work(&state->global_task)) {
        int percent =
            (int)(background_task_progress(
                      &state->global_task) *
                      100.0f +
                  0.5f);
        snprintf(state->global_status,
                 sizeof(state->global_status),
                 state->global_background_work ==
                         GLOBAL_BACKGROUND_ANALYSIS
                     ? "Building the %s model in the background... %d%%"
                     : "Reconstructing %s in the background... %d%%",
                 state->global_background_work ==
                         GLOBAL_BACKGROUND_ANALYSIS
                     ? global_channel_mode_label(state)
                     : global_bin_scope(state),
                 percent);
    }
    if (background_task_has_work(&state->adaptive_task)) {
        int percent =
            (int)(background_task_progress(
                      &state->adaptive_task) *
                      100.0f +
                  0.5f);
        snprintf(state->adaptive_status,
                 sizeof(state->adaptive_status),
                 "Building time-varying Top-%d per channel in the background... %d%%",
                 state->adaptive_stft_job.top_component_count,
                 percent);
    }
}

static void imported_reconstructions_unload(ImportedAnalysisState *state) {
    audio_clip_unload(&state->harmonic_clip);
    audio_clip_unload(&state->frame_original_clip);
    audio_clip_unload(&state->fourier_clip);
    sample_buffer_free(&state->harmonic_resynthesis);
    sample_buffer_free(&state->fourier_reconstruction);
    fourier_frame_analysis_free(&state->fourier_analysis);
    memset(state->harmonics, 0, sizeof(state->harmonics));
    state->harmonic_count = 0;
    state->detected_harmonic_count = 0;
    state->fourier_top_components = 0;
    state->fourier_rendered_components = 0;
    state->harmonic_ready = false;
    state->fourier_ready = false;
}

static void imported_analysis_unload(ImportedAnalysisState *state) {
    audio_clip_unload(&state->full_clip);
    audio_clip_unload(&state->region_clip);
    imported_reconstructions_unload(state);
    imported_full_file_unload(state);
    spectrum_free(&state->spectrum);
    imported_audio_unload(&state->audio);
    state->analyzed = false;
}

static void stop_imported_playback(ImportedAnalysisState *state) {
    audio_clip_stop(&state->full_clip);
    audio_clip_stop(&state->region_clip);
    audio_clip_stop(&state->harmonic_clip);
    audio_clip_stop(&state->frame_original_clip);
    audio_clip_stop(&state->fourier_clip);
    audio_clip_stop(&state->global_clip);
    audio_clip_stop(&state->adaptive_clip);
}

static AudioClip *playback_target_clip(PlaybackTarget target,
                                       AudioClip *synth_clip,
                                       ImportedAnalysisState *state) {
    if (state == NULL) {
        return target == PLAYBACK_SYNTH ? synth_clip : NULL;
    }
    switch (target) {
        case PLAYBACK_SYNTH: return synth_clip;
        case PLAYBACK_ANALYSIS_FULL:
        case PLAYBACK_SPECTROGRAM_ORIGINAL: return &state->full_clip;
        case PLAYBACK_ANALYSIS_REGION:
        case PLAYBACK_LAB_ORIGINAL: return &state->region_clip;
        case PLAYBACK_LAB_HARMONIC: return &state->harmonic_clip;
        case PLAYBACK_LAB_FRAME: return &state->frame_original_clip;
        case PLAYBACK_LAB_FOURIER: return &state->fourier_clip;
        case PLAYBACK_SPECTROGRAM_RECONSTRUCTION:
            return active_full_file_clip(state);
        default: return NULL;
    }
}

static void stop_all_playback(AudioClip *synth_clip,
                              ImportedAnalysisState *state) {
    audio_clip_stop(synth_clip);
    stop_imported_playback(state);
}

static bool start_playback_target(PlaybackTarget target,
                                  AudioClip *synth_clip,
                                  ImportedAnalysisState *state) {
    AudioClip *clip = playback_target_clip(target, synth_clip, state);
    if (clip == NULL || !clip->loaded) {
        return false;
    }
    stop_all_playback(synth_clip, state);
    return audio_clip_play(clip);
}

static bool toggle_playback_target(PlaybackTarget target,
                                   AudioClip *synth_clip,
                                   ImportedAnalysisState *state) {
    AudioClip *clip = playback_target_clip(target, synth_clip, state);
    if (clip == NULL || !clip->loaded) {
        return false;
    }
    if (audio_clip_is_playing(clip)) {
        return audio_clip_pause(clip);
    }
    if (audio_clip_is_paused(clip)) {
        return audio_clip_resume(clip);
    }
    return start_playback_target(target, synth_clip, state);
}

static TransportPlayerView playback_player_view(PlaybackTarget target,
                                                 const char *title,
                                                 AudioClip *synth_clip,
                                                 ImportedAnalysisState *state) {
    AudioClip *clip = playback_target_clip(target, synth_clip, state);
    TransportPlayerView view = {
        .available = clip != NULL && clip->loaded,
        .playing = audio_clip_is_playing(clip),
        .paused = audio_clip_is_paused(clip),
        .title = title,
        .position_seconds = audio_clip_position_seconds(clip),
        .duration_seconds = audio_clip_duration_seconds(clip),
    };
    return view;
}

static bool export_samples_with_dialog(void *window_handle,
                                       const char *suggested_name,
                                       const SampleBuffer *buffer,
                                       char *status,
                                       size_t status_size) {
    char export_path[IMPORT_PATH_CAPACITY] = {0};
    if (!windows_app_choose_wav_save_path(
            window_handle, suggested_name, export_path, sizeof(export_path))) {
        snprintf(status, status_size, "Export canceled.");
        return false;
    }
    if (!export_samples_to_wav(export_path, buffer)) {
        snprintf(status,
                 status_size,
                 "Could not write the WAV to the selected location.");
        return false;
    }

    snprintf(status, status_size, "Saved %s", GetFileName(export_path));
    return true;
}

static bool export_interleaved_with_dialog(
    void *window_handle,
    const char *suggested_name,
    const InterleavedBuffer *buffer,
    char *status,
    size_t status_size) {
    char export_path[IMPORT_PATH_CAPACITY] = {0};
    if (!windows_app_choose_wav_save_path(
            window_handle,
            suggested_name,
            export_path,
            sizeof(export_path))) {
        snprintf(status, status_size, "Export canceled.");
        return false;
    }
    if (!export_interleaved_to_wav(export_path, buffer)) {
        snprintf(status,
                 status_size,
                 "Could not write the WAV to the selected location.");
        return false;
    }

    snprintf(status, status_size, "Saved %s",
             GetFileName(export_path));
    return true;
}

static SampleBuffer imported_analysis_region(const ImportedAnalysisState *state) {
    SampleBuffer region = {0};
    if (state == NULL || state->audio.mono.samples == NULL || state->audio.mono.count == 0 ||
        state->audio.mono.sample_rate == 0) {
        return region;
    }

    const float duration = state->audio.duration_seconds;
    const float start_seconds = fminf(fmaxf(state->region_start_seconds, 0.0f), duration);
    const float remaining = duration - start_seconds;
    const float length_seconds = fminf(fmaxf(state->region_duration_seconds, 0.0f), remaining);
    size_t start = (size_t)(start_seconds * (float)state->audio.mono.sample_rate);
    size_t count = (size_t)(length_seconds * (float)state->audio.mono.sample_rate);
    if (start >= state->audio.mono.count) {
        start = state->audio.mono.count - 1;
    }
    if (count == 0) {
        count = 1;
    }
    if (count > state->audio.mono.count - start) {
        count = state->audio.mono.count - start;
    }
    region.samples = state->audio.mono.samples + start;
    region.count = count;
    region.sample_rate = state->audio.mono.sample_rate;
    return region;
}

static bool rebuild_fourier_reconstruction(ImportedAnalysisState *state,
                                           int requested_components,
                                           bool audio_ready) {
    if (state == NULL || state->fourier_analysis.component_count == 0U) {
        return false;
    }
    if (requested_components < 1) requested_components = 1;
    if ((size_t)requested_components > state->fourier_analysis.component_count) {
        requested_components = (int)state->fourier_analysis.component_count;
    }

    audio_clip_unload(&state->fourier_clip);
    sample_buffer_free(&state->fourier_reconstruction);
    state->fourier_reconstruction =
        reconstruct_fourier_frame(&state->fourier_analysis, (size_t)requested_components);
    if (state->fourier_reconstruction.samples == NULL) {
        state->fourier_ready = false;
        return false;
    }
    state->fourier_top_components = requested_components;
    state->fourier_rendered_components = requested_components;
    state->fourier_ready = true;
    if (audio_ready) {
        audio_clip_set_samples(&state->fourier_clip, &state->fourier_reconstruction);
    }
    return true;
}

static bool analyze_imported_region(ImportedAnalysisState *state, bool audio_ready) {
    SampleBuffer region = imported_analysis_region(state);
    if (region.samples == NULL || region.count == 0) {
        return false;
    }

    spectrum_free(&state->spectrum);
    imported_reconstructions_unload(state);
    state->peak_count = 0;
    state->interpolated_peak_count = 0;
    state->pitch = (PitchEstimate){0};
    state->analyzed = false;

    SampleBuffer fft_frame = region;
    if (fft_frame.count > 16384) {
        fft_frame.samples += (fft_frame.count - 16384) / 2;
        fft_frame.count = 16384;
    }
    state->spectrum =
        compute_magnitude_spectrum(fft_frame.samples, fft_frame.count, fft_frame.sample_rate);
    if (state->spectrum.count == 0) {
        snprintf(state->status, sizeof(state->status), "Could not compute the selected region's spectrum.");
        return false;
    }

    const float max_frequency = fminf(20000.0f, (float)region.sample_rate * 0.5f);
    state->peak_count =
        find_peaks(&state->spectrum, 20.0f, max_frequency, -55.0f, state->peaks, ANALYSIS_MAX_PEAKS);
    state->interpolated_peak_count =
        find_interpolated_peaks(&state->spectrum,
                                20.0f,
                                max_frequency,
                                -55.0f,
                                state->interpolated_peaks,
                                ANALYSIS_MAX_PEAKS);
    state->pitch = estimate_pitch(&region, 50.0f, fminf(2000.0f, max_frequency));

    state->analyzed_region_start_seconds = state->region_start_seconds;
    state->analyzed_region_duration_seconds = (float)region.count / (float)region.sample_rate;
    if (state->pitch.valid && state->pitch.confidence >= 0.60f) {
        float strongest_spectrum_magnitude = 0.0f;
        for (size_t bin = 0U; bin < state->spectrum.count; bin++) {
            if (state->spectrum.magnitudes[bin] > strongest_spectrum_magnitude) {
                strongest_spectrum_magnitude = state->spectrum.magnitudes[bin];
            }
        }
        float harmonic_threshold_db =
            fmaxf(-100.0f, linear_to_db(strongest_spectrum_magnitude) - 50.0f);
        state->harmonic_count = extract_harmonics(&state->spectrum,
                                                  state->pitch.frequency_hz,
                                                  EXTRACTED_HARMONIC_COUNT,
                                                  30.0f,
                                                  harmonic_threshold_db,
                                                  state->harmonics,
                                                  EXTRACTED_HARMONIC_COUNT);
        for (int index = 0; index < state->harmonic_count; index++) {
            if (state->harmonics[index].detected) {
                state->detected_harmonic_count++;
            }
        }
        state->harmonic_resynthesis = resynthesize_from_harmonics(state->pitch.frequency_hz,
                                                                  state->harmonics,
                                                                  state->harmonic_count,
                                                                  state->analyzed_region_duration_seconds,
                                                                  region.sample_rate);
        state->harmonic_ready = state->harmonic_resynthesis.samples != NULL;
        if (audio_ready && state->harmonic_ready) {
            audio_clip_set_samples(&state->harmonic_clip, &state->harmonic_resynthesis);
        }
    }

    if (analyze_fourier_frame(&region, &state->fourier_analysis)) {
        if (audio_ready) {
            audio_clip_set_samples(&state->frame_original_clip, &state->fourier_analysis.windowed_frame);
        }
        int initial_components = state->fourier_analysis.component_count < 25U
                                     ? (int)state->fourier_analysis.component_count
                                     : 25;
        rebuild_fourier_reconstruction(state, initial_components, audio_ready);
    }

    audio_clip_unload(&state->region_clip);
    if (audio_ready && !audio_clip_set_samples(&state->region_clip, &region)) {
        snprintf(state->status, sizeof(state->status), "Analysis finished, but region playback is unavailable.");
    } else {
        snprintf(state->status,
                 sizeof(state->status),
                 "Analysis ready: %d spectral peak%s.",
                 state->peak_count,
                 state->peak_count == 1 ? "" : "s");
    }
    if (state->harmonic_ready && state->fourier_ready) {
        snprintf(state->lab_status,
                 sizeof(state->lab_status),
                 "Harmonic model: %d/%d partials detected. Fourier frame: Top-%d.",
                 state->detected_harmonic_count,
                 state->harmonic_count,
                 state->fourier_top_components);
    } else if (state->fourier_ready) {
        snprintf(state->lab_status,
                 sizeof(state->lab_status),
                 "No stable pitched model for this region. Phase-preserving Top-%d frame is ready.",
                 state->fourier_top_components);
    } else {
        snprintf(state->lab_status, sizeof(state->lab_status), "Reconstruction could not be created for this region.");
    }
    state->analyzed = true;
    state->analysis_revision++;
    if (state->analysis_revision == 0U) {
        state->analysis_revision = 1U;
    }
    return true;
}

static bool load_imported_analysis(
    ImportedAnalysisState *state, const char *path, bool audio_ready, char *error, size_t error_size) {
    ImportedAudio loaded;
    imported_audio_init(&loaded);
    if (!imported_audio_load(path, &loaded, error, error_size)) {
        return false;
    }

    imported_analysis_unload(state);
    state->audio = loaded;
    snprintf(state->path, sizeof(state->path), "%s", path);
    snprintf(state->file_name, sizeof(state->file_name), "%s", GetFileName(path));
    if (g_theme != NULL) {
        theme_ensure_text_coverage(
            g_theme, state->file_name);
    }
    state->waveform_bin_count = summarize_waveform(state->audio.mono.samples,
                                                   state->audio.mono.count,
                                                   state->waveform_minimums,
                                                   state->waveform_maximums,
                                                   ANALYSIS_WAVEFORM_BINS);
    state->region_duration_seconds = fminf(1.0f, state->audio.duration_seconds);
    state->region_start_seconds =
        fmaxf(0.0f, (state->audio.duration_seconds - state->region_duration_seconds) * 0.5f);
    if (audio_ready &&
        !audio_clip_set_interleaved(
            &state->full_clip,
            &state->audio.interleaved)) {
        snprintf(state->status, sizeof(state->status), "Loaded audio, but full-file playback is unavailable.");
    } else if (state->audio.source_channels == 2U &&
               state->audio.interleaved.channel_count == 2U) {
        snprintf(state->status,
                 sizeof(state->status),
                 "Loaded stereo audio; pitch and visualization use a mono reference.");
    } else if (state->audio.source_channels > 2U) {
        snprintf(state->status,
                 sizeof(state->status),
                 "Loaded %u-channel audio and downmixed it to mono for analysis and reconstruction.",
                 state->audio.source_channels);
    } else {
        snprintf(state->status,
                 sizeof(state->status),
                 "Loaded mono audio for analysis and reconstruction.");
    }
    analyze_imported_region(state, audio_ready);
    start_full_file_analysis(state);
    return true;
}

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
    windows_app_apply_window_icon(GetWindowHandle());
    SetExitKey(KEY_NULL);
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
    ImportedAnalysisState imported;
    imported_analysis_init(&imported);
    PeakReadoutMode analysis_peak_readout_mode =
        PEAK_READOUT_INTERPOLATED;
    AudioAnalysisActions pending_analysis_actions = {0};
    HarmonicLabActions pending_lab_actions = {0};
    SpectrogramActions pending_spectrogram_actions = {0};
    SettingsActions pending_settings_actions = {0};
    PlaybackTarget analysis_playback = PLAYBACK_ANALYSIS_FULL;
    PlaybackTarget lab_playback = PLAYBACK_LAB_ORIGINAL;
    PlaybackTarget spectrogram_playback = PLAYBACK_SPECTROGRAM_ORIGINAL;
    AppPage active_page = APP_PAGE_OVERVIEW;
    AppPage help_topic = APP_PAGE_OVERVIEW;
    bool help_open = false;

    while (!WindowShouldClose()) {
#if defined(SPECTRA_RAYLIB_CUSTOM_FRAME_CONTROL)
        double frame_started_at = GetTime();
#endif

        bool toggle_fullscreen_requested = IsKeyPressed(KEY_F11);
        bool toggle_help_requested = IsKeyPressed(KEY_F1);
        if (help_open && IsKeyPressed(KEY_ESCAPE)) {
            toggle_help_requested = true;
        }

        if (IsKeyPressed(KEY_ONE)) active_page = APP_PAGE_OVERVIEW;
        if (IsKeyPressed(KEY_TWO)) active_page = APP_PAGE_SYNTH;
        if (IsKeyPressed(KEY_THREE)) active_page = APP_PAGE_ANALYSIS;
        if (IsKeyPressed(KEY_FOUR)) active_page = APP_PAGE_HARMONIC_LAB;
        if (IsKeyPressed(KEY_FIVE)) active_page = APP_PAGE_SPECTROGRAM;

        if (pending_settings_actions
                .select_global_memory_limit) {
            imported.global_model_memory_limit_bytes =
                pending_settings_actions
                    .global_memory_limit_bytes;
            bool global_model_unavailable =
                !global_analysis_ready(&imported) &&
                !imported.global_ready &&
                !background_task_has_work(
                    &imported.global_task);
            if (imported.audio.mono.samples != NULL &&
                imported.full_file_mode ==
                    FULL_FILE_MODE_FIXED_GLOBAL &&
                global_model_unavailable) {
                begin_global_model_analysis(
                    &imported,
                    imported.global_channel_mode);
            }
        }
        pending_settings_actions = (SettingsActions){0};

        if (pending_analysis_actions.select_peak_readout_mode &&
            (pending_analysis_actions.peak_readout_mode ==
                 PEAK_READOUT_INTERPOLATED ||
             pending_analysis_actions.peak_readout_mode ==
                 PEAK_READOUT_FFT_BINS)) {
            analysis_peak_readout_mode =
                pending_analysis_actions.peak_readout_mode;
        }
        if (pending_analysis_actions.choose_file) {
            char selected_path[IMPORT_PATH_CAPACITY] = {0};
            if (windows_app_choose_audio_file(GetWindowHandle(), selected_path, sizeof(selected_path))) {
                char import_error[IMPORT_TEXT_CAPACITY] = {0};
                if (!load_imported_analysis(
                        &imported, selected_path, audio_ready, import_error, sizeof(import_error))) {
                    snprintf(imported.status, sizeof(imported.status), "%s", import_error);
                } else {
                    stop_all_playback(&clip, &imported);
                    analysis_playback = PLAYBACK_ANALYSIS_FULL;
                    lab_playback = PLAYBACK_LAB_ORIGINAL;
                    spectrogram_playback = PLAYBACK_SPECTROGRAM_ORIGINAL;
                }
            }
        }
        if (pending_analysis_actions.stop) {
            stop_all_playback(&clip, &imported);
            snprintf(imported.status, sizeof(imported.status), "Playback stopped.");
        }
        if (pending_analysis_actions.play_full && imported.audio.mono.samples != NULL) {
            analysis_playback = PLAYBACK_ANALYSIS_FULL;
            if (start_playback_target(analysis_playback, &clip, &imported)) {
                snprintf(imported.status, sizeof(imported.status), "Playing the full imported file.");
            } else {
                snprintf(imported.status, sizeof(imported.status), "Full-file playback is unavailable.");
            }
        }
        if (pending_analysis_actions.play_region && imported.audio.mono.samples != NULL) {
            analysis_playback = PLAYBACK_ANALYSIS_REGION;
            if (start_playback_target(analysis_playback, &clip, &imported)) {
                snprintf(imported.status, sizeof(imported.status), "Playing the analyzed region.");
            } else {
                snprintf(imported.status, sizeof(imported.status), "Analyze the selected region before playing it.");
            }
        }
        if (pending_analysis_actions.toggle_play_pause) {
            AudioClip *selected =
                playback_target_clip(analysis_playback, &clip, &imported);
            bool was_playing = audio_clip_is_playing(selected);
            bool was_paused = audio_clip_is_paused(selected);
            if (toggle_playback_target(analysis_playback, &clip, &imported)) {
                snprintf(imported.status,
                         sizeof(imported.status),
                         was_playing ? "Playback paused."
                                     : (was_paused ? "Playback resumed."
                                                   : "Playback started."));
            } else {
                snprintf(imported.status,
                         sizeof(imported.status),
                         "The selected audio is not ready for playback.");
            }
        }
        if (pending_analysis_actions.analyze_region && imported.audio.mono.samples != NULL) {
            audio_clip_stop(&imported.region_clip);
            analyze_imported_region(&imported, audio_ready);
        }
        pending_analysis_actions = (AudioAnalysisActions){0};

        if (pending_lab_actions.open_analysis) {
            active_page = APP_PAGE_ANALYSIS;
        }
        if (pending_lab_actions.stop) {
            stop_all_playback(&clip, &imported);
            snprintf(imported.lab_status, sizeof(imported.lab_status), "Reconstruction playback stopped.");
        }
        if (pending_lab_actions.rebuild_fourier && imported.fourier_analysis.component_count > 0U) {
            if (rebuild_fourier_reconstruction(
                    &imported, pending_lab_actions.top_component_count, audio_ready)) {
                snprintf(imported.lab_status,
                         sizeof(imported.lab_status),
                         "Rebuilt the frame from its %d strongest phase-preserving Fourier components.",
                         imported.fourier_rendered_components);
            } else {
                snprintf(imported.lab_status,
                         sizeof(imported.lab_status),
                         "Could not rebuild the selected Fourier frame.");
            }
        }
        if (pending_lab_actions.play_original && imported.analyzed) {
            lab_playback = PLAYBACK_LAB_ORIGINAL;
            if (start_playback_target(lab_playback, &clip, &imported)) {
                snprintf(imported.lab_status, sizeof(imported.lab_status), "Playing the original selected region.");
            }
        }
        if (pending_lab_actions.play_harmonic && imported.harmonic_ready) {
            lab_playback = PLAYBACK_LAB_HARMONIC;
            if (start_playback_target(lab_playback, &clip, &imported)) {
                snprintf(imported.lab_status, sizeof(imported.lab_status), "Playing the integer-harmonic resynthesis.");
            }
        }
        if (pending_lab_actions.play_frame && imported.fourier_analysis.windowed_frame.samples != NULL) {
            lab_playback = PLAYBACK_LAB_FRAME;
            if (start_playback_target(lab_playback, &clip, &imported)) {
                snprintf(imported.lab_status, sizeof(imported.lab_status), "Playing the original windowed FFT frame.");
            }
        }
        if (pending_lab_actions.play_fourier && imported.fourier_ready) {
            lab_playback = PLAYBACK_LAB_FOURIER;
            if (start_playback_target(lab_playback, &clip, &imported)) {
                snprintf(imported.lab_status,
                         sizeof(imported.lab_status),
                         "Playing the Top-%d phase-preserving Fourier reconstruction.",
                         imported.fourier_rendered_components);
            }
        }
        if (pending_lab_actions.toggle_play_pause) {
            AudioClip *selected =
                playback_target_clip(lab_playback, &clip, &imported);
            bool was_playing = audio_clip_is_playing(selected);
            bool was_paused = audio_clip_is_paused(selected);
            if (toggle_playback_target(lab_playback, &clip, &imported)) {
                snprintf(imported.lab_status,
                         sizeof(imported.lab_status),
                         was_playing
                             ? "Reconstruction paused."
                             : (was_paused
                                    ? "Reconstruction playback resumed."
                                    : "Reconstruction playback started."));
            } else {
                snprintf(imported.lab_status,
                         sizeof(imported.lab_status),
                         "The selected reconstruction is not ready.");
            }
        }
        if (pending_lab_actions.export_harmonic && imported.harmonic_ready) {
            export_samples_with_dialog(GetWindowHandle(),
                                       "spectra-harmonic-resynthesis.wav",
                                       &imported.harmonic_resynthesis,
                                       imported.lab_status,
                                       sizeof(imported.lab_status));
        }
        if (pending_lab_actions.export_fourier && imported.fourier_ready) {
            char suggested_name[96];
            snprintf(suggested_name,
                     sizeof(suggested_name),
                     "spectra-top-%d-frame.wav",
                     imported.fourier_rendered_components);
            export_samples_with_dialog(GetWindowHandle(),
                                       suggested_name,
                                       &imported.fourier_reconstruction,
                                       imported.lab_status,
                                       sizeof(imported.lab_status));
        }
        pending_lab_actions = (HarmonicLabActions){0};

        if (pending_spectrogram_actions.open_analysis) {
            active_page = APP_PAGE_ANALYSIS;
        }
        if (pending_spectrogram_actions.select_mode) {
            stop_all_playback(&clip, &imported);
            restore_full_file_idle_status(
                &imported, imported.full_file_mode);
            imported.full_file_mode =
                pending_spectrogram_actions.mode;
            spectrogram_playback = PLAYBACK_SPECTROGRAM_ORIGINAL;
            if (imported.full_file_mode ==
                    FULL_FILE_MODE_TIME_VARYING_STFT &&
                imported.audio.mono.samples != NULL &&
                !imported.adaptive_ready &&
                !background_task_has_work(
                    &imported.adaptive_task)) {
                start_adaptive_reconstruction(
                    &imported,
                    imported.adaptive_selected_components,
                    audio_ready);
            }
            restore_full_file_idle_status(
                &imported, imported.full_file_mode);
        }
        if (pending_spectrogram_actions
                .select_global_channel_mode &&
            imported.audio.mono.samples != NULL) {
            stop_all_playback(&clip, &imported);
            cache_current_global_reconstruction(&imported);
            begin_global_model_analysis(
                &imported,
                pending_spectrogram_actions
                    .global_channel_mode);
            imported.full_file_mode =
                FULL_FILE_MODE_FIXED_GLOBAL;
            spectrogram_playback =
                PLAYBACK_SPECTROGRAM_ORIGINAL;
        }
        char *full_file_status =
            active_full_file_status(&imported);
        if (pending_spectrogram_actions
                .select_global_selection_mode &&
            imported.full_file_mode ==
                FULL_FILE_MODE_FIXED_GLOBAL) {
            stop_all_playback(&clip, &imported);
            imported.global_selection_mode =
                pending_spectrogram_actions
                    .global_selection_mode;
            if (imported.audio.mono.samples != NULL) {
                if (imported.global_selection_mode ==
                    GLOBAL_SELECTION_SPECTRAL_ENERGY) {
                    start_global_energy_reconstruction(
                        &imported,
                        imported
                            .global_selected_energy_target,
                        audio_ready);
                } else {
                    start_global_reconstruction(
                        &imported,
                        imported
                            .global_bin_budget_components,
                        audio_ready);
                }
            }
        }
        if (pending_spectrogram_actions.stop) {
            stop_all_playback(&clip, &imported);
            snprintf(full_file_status,
                     IMPORT_TEXT_CAPACITY,
                     "Playback stopped. The current reconstruction remains ready.");
        }
        if (pending_spectrogram_actions.select_top_components &&
            imported.audio.mono.samples != NULL) {
            if (imported.full_file_mode ==
                FULL_FILE_MODE_FIXED_GLOBAL) {
                imported.global_selection_mode =
                    GLOBAL_SELECTION_PADDED_FFT_BINS;
                imported.global_bin_budget_components =
                    pending_spectrogram_actions
                        .top_component_count;
                start_global_reconstruction(
                    &imported,
                    pending_spectrogram_actions.top_component_count,
                    audio_ready);
            } else {
                start_adaptive_reconstruction(
                    &imported,
                    pending_spectrogram_actions.top_component_count,
                    audio_ready);
            }
        }
        if (pending_spectrogram_actions
                .select_energy_target &&
            imported.audio.mono.samples != NULL &&
            imported.full_file_mode ==
                FULL_FILE_MODE_FIXED_GLOBAL) {
            imported.global_selection_mode =
                GLOBAL_SELECTION_SPECTRAL_ENERGY;
            start_global_energy_reconstruction(
                &imported,
                pending_spectrogram_actions.energy_target,
                audio_ready);
        }
        if (pending_spectrogram_actions.play_original &&
            imported.audio.mono.samples != NULL) {
            spectrogram_playback = PLAYBACK_SPECTROGRAM_ORIGINAL;
            if (start_playback_target(spectrogram_playback, &clip, &imported)) {
                snprintf(full_file_status,
                         IMPORT_TEXT_CAPACITY,
                         "Playing the original full imported file.");
            } else {
                snprintf(full_file_status,
                         IMPORT_TEXT_CAPACITY,
                         "Original full-file playback is unavailable.");
            }
        }
        if (pending_spectrogram_actions.play_reconstruction &&
            active_full_file_ready(&imported)) {
            spectrogram_playback = PLAYBACK_SPECTROGRAM_RECONSTRUCTION;
            if (start_playback_target(spectrogram_playback, &clip, &imported)) {
                if (imported.full_file_mode ==
                    FULL_FILE_MODE_FIXED_GLOBAL) {
                    snprintf(full_file_status,
                             IMPORT_TEXT_CAPACITY,
                             "Playing %d %s, retaining %.3f%% spectral energy.",
                             imported.global_rendered_components,
                             global_bin_scope(&imported),
                             imported.global_retained_energy *
                                 100.0f);
                } else {
                    snprintf(full_file_status,
                             IMPORT_TEXT_CAPACITY,
                             "Playing Top-%d selected per channel in every STFT frame.",
                             imported.adaptive_rendered_components);
                }
            } else {
                snprintf(full_file_status,
                         IMPORT_TEXT_CAPACITY,
                         "Top-%d playback is unavailable.",
                         active_full_file_rendered_components(
                             &imported));
            }
        }
        if (pending_spectrogram_actions.toggle_play_pause) {
            AudioClip *selected = playback_target_clip(
                spectrogram_playback, &clip, &imported);
            bool was_playing = audio_clip_is_playing(selected);
            bool was_paused = audio_clip_is_paused(selected);
            if (toggle_playback_target(
                    spectrogram_playback, &clip, &imported)) {
                snprintf(full_file_status,
                         IMPORT_TEXT_CAPACITY,
                         was_playing
                             ? "Playback paused."
                             : (was_paused ? "Playback resumed."
                                           : "Playback started."));
            } else {
                snprintf(full_file_status,
                         IMPORT_TEXT_CAPACITY,
                         "The selected full-file audio is not ready.");
            }
        }
        if (pending_spectrogram_actions.export_reconstruction &&
            active_full_file_ready(&imported)) {
            char suggested_name[96];
            snprintf(suggested_name,
                     sizeof(suggested_name),
                     imported.full_file_mode ==
                             FULL_FILE_MODE_FIXED_GLOBAL
                         ? "spectra-fixed-top-%d-full.wav"
                         : "spectra-time-varying-top-%d-full.wav",
                     active_full_file_rendered_components(
                         &imported));
            export_interleaved_with_dialog(
                GetWindowHandle(),
                suggested_name,
                active_full_file_buffer(&imported),
                full_file_status,
                IMPORT_TEXT_CAPACITY);
        }
        pending_spectrogram_actions = (SpectrogramActions){0};

        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            const char *selected_path = NULL;
            for (unsigned int i = 0; i < dropped.count; i++) {
                if (audio_file_is_supported(dropped.paths[i])) {
                    selected_path = dropped.paths[i];
                    break;
                }
            }
            if (selected_path != NULL) {
                char import_error[IMPORT_TEXT_CAPACITY] = {0};
                if (!load_imported_analysis(
                        &imported, selected_path, audio_ready, import_error, sizeof(import_error))) {
                    snprintf(imported.status, sizeof(imported.status), "%s", import_error);
                } else {
                    stop_all_playback(&clip, &imported);
                    analysis_playback = PLAYBACK_ANALYSIS_FULL;
                    lab_playback = PLAYBACK_LAB_ORIGINAL;
                    spectrogram_playback = PLAYBACK_SPECTROGRAM_ORIGINAL;
                }
                active_page = APP_PAGE_ANALYSIS;
            } else {
                snprintf(imported.status,
                         sizeof(imported.status),
                         "Unsupported file. Choose WAV, MP3, OGG, or FLAC.");
                active_page = APP_PAGE_ANALYSIS;
            }
            UnloadDroppedFiles(dropped);
        }

        advance_full_file_analysis(&imported, audio_ready);

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

        AppPage page_before_shell = active_page;
        AppShellFrame shell_frame =
            draw_app_shell(&theme, active_page, audio_ready, help_open);
        active_page = shell_frame.page;
        if (shell_frame.toggle_fullscreen) toggle_fullscreen_requested = true;
        if (help_open && active_page != page_before_shell) {
            help_topic = active_page;
        }
        if (shell_frame.toggle_help) {
            toggle_help_requested = true;
        }
        if (toggle_help_requested) {
            help_open = !help_open;
            if (help_open) {
                help_topic = active_page;
            }
        }

        if (help_open) {
            HelpCenterActions help_actions =
                draw_help_center(&theme, shell_frame.workspace, help_topic);
            help_topic = help_actions.topic;
            if (help_actions.close) {
                help_open = false;
            }
        } else if (active_page == APP_PAGE_OVERVIEW) {
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
            float action_width = (controls_width - 8.0f) * 0.5f;
            Rectangle play_button = {
                controls_x, synth_panel.y + 68.0f, action_width, 44.0f};
            Rectangle export_button = {
                play_button.x + play_button.width + 8.0f,
                play_button.y,
                action_width,
                44.0f};
            if (draw_button(&theme, play_button, "Play from start", theme.accent) ||
                IsKeyPressed(KEY_SPACE)) {
                if (!audio_ready) {
                    snprintf(status, sizeof(status), "No audio device detected. Check Windows audio output.");
                } else if (start_playback_target(
                               PLAYBACK_SYNTH, &clip, &imported)) {
                    snprintf(status, sizeof(status), "Playing %.1f Hz %s tone.", fundamental, PRESETS[selected_preset].name);
                } else {
                    snprintf(status, sizeof(status), "Playback buffer was not ready. Adjust a control and try again.");
                }
            }
            if (draw_button(&theme, export_button, "Export WAV", theme.blue)) {
                export_samples_with_dialog(GetWindowHandle(),
                                           "spectra-tone.wav",
                                           &generated,
                                           status,
                                           sizeof(status));
            }

            TransportPlayerView synth_player =
                playback_player_view(PLAYBACK_SYNTH,
                                     TextFormat("%.1f Hz %s tone",
                                                fundamental,
                                                PRESETS[selected_preset].name),
                                     &clip,
                                     &imported);
            TransportPlayerActions synth_player_actions =
                draw_transport_player(&theme,
                                      (Rectangle){controls_x,
                                                  synth_panel.y + 124.0f,
                                                  controls_width,
                                                  64.0f},
                                      &synth_player);
            if (synth_player_actions.toggle_play_pause) {
                bool was_playing = audio_clip_is_playing(&clip);
                bool was_paused = audio_clip_is_paused(&clip);
                if (toggle_playback_target(PLAYBACK_SYNTH, &clip, &imported)) {
                    snprintf(status,
                             sizeof(status),
                             was_playing ? "Tone paused."
                                         : (was_paused ? "Tone playback resumed."
                                                       : "Playing tone from the beginning."));
                }
            }
            if (synth_player_actions.stop) {
                stop_all_playback(&clip, &imported);
                snprintf(status, sizeof(status), "Stopped playback.");
            }

            DrawText(status, (int)controls_x, (int)synth_panel.y + 202, 12, theme.muted_text);
            float new_fundamental =
                draw_slider(&theme, (Rectangle){controls_x, synth_panel.y + 236.0f, controls_width, 42},
                            "Fundamental frequency (Hz)", fundamental, 40.0f, 1200.0f);
            float new_duration =
                draw_slider(&theme, (Rectangle){controls_x, synth_panel.y + 296.0f, controls_width, 42},
                            "Duration (seconds)", duration_seconds, 0.2f, 3.0f);
            float new_gain = draw_slider(&theme, (Rectangle){controls_x, synth_panel.y + 356.0f, controls_width, 42},
                                         "Master gain", master_gain, 0.05f, 1.0f);
            if (fabsf(new_fundamental - fundamental) > 0.01f || fabsf(new_duration - duration_seconds) > 0.001f ||
                fabsf(new_gain - master_gain) > 0.001f) {
                fundamental = new_fundamental;
                duration_seconds = new_duration;
                master_gain = new_gain;
                dirty = true;
            }

            theme_draw_heading(&theme, "Presets", controls_x, synth_panel.y + 416.0f, 16.0f, theme.text);
            float preset_width = (controls_width - 16.0f) / 3.0f;
            for (int i = 0; i < PRESET_COUNT; i++) {
                Rectangle bounds = {controls_x + (float)(i % 3) * (preset_width + 8.0f),
                                    synth_panel.y + 452.0f + (float)(i / 3) * 44.0f, preset_width, 36.0f};
                Color accent = selected_preset == (PresetId)i ? theme.accent : theme.muted_text;
                if (draw_button(&theme, bounds, PRESETS[i].name, accent)) {
                    selected_preset = (PresetId)i;
                    apply_preset(selected_preset, harmonics);
                    dirty = true;
                    snprintf(status, sizeof(status), "Loaded %s: %s.", PRESETS[i].name, PRESETS[i].description);
                }
            }

            theme_draw_heading(&theme, "Harmonics", controls_x, synth_panel.y + 544.0f, 16.0f, theme.text);
            float harmonic_width = (controls_width - 30.0f) / 4.0f;
            float harmonic_row_gap =
                fmaxf(34.0f, fminf(46.0f, (synth_panel.height - 626.0f) / 3.0f));
            for (int i = 0; i < HARMONIC_COUNT; i++) {
                int column = i % 4;
                int row = i / 4;
                char label[32];
                snprintf(label, sizeof(label), "H%d", i + 1);
                Rectangle slider = {controls_x + (float)column * (harmonic_width + 10.0f),
                                    synth_panel.y + 582.0f + (float)row * harmonic_row_gap, harmonic_width, 34.0f};
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
            const char *analysis_player_title =
                analysis_playback == PLAYBACK_ANALYSIS_REGION
                    ? "Analyzed region"
                    : "Original full file";
            bool use_interpolated_peaks =
                analysis_peak_readout_mode ==
                PEAK_READOUT_INTERPOLATED;
            AudioAnalysisView analysis_view = {
                .loaded = imported.audio.mono.samples != NULL,
                .analyzed = imported.analyzed,
                .playing_full = audio_clip_is_playing(&imported.full_clip),
                .playing_region = audio_clip_is_playing(&imported.region_clip),
                .player = playback_player_view(analysis_playback,
                                               analysis_player_title,
                                               &clip,
                                               &imported),
                .file_name = imported.file_name,
                .status = imported.status,
                .source_channels = imported.audio.source_channels,
                .sample_rate = imported.audio.mono.sample_rate,
                .duration_seconds = imported.audio.duration_seconds,
                .region_start_seconds = imported.region_start_seconds,
                .region_duration_seconds = imported.region_duration_seconds,
                .samples = &imported.audio.mono,
                .waveform_minimums = imported.waveform_minimums,
                .waveform_maximums = imported.waveform_maximums,
                .waveform_bin_count = imported.waveform_bin_count,
                .spectrum = &imported.spectrum,
                .peaks = use_interpolated_peaks
                             ? imported.interpolated_peaks
                             : imported.peaks,
                .peak_count = use_interpolated_peaks
                                  ? imported.interpolated_peak_count
                                  : imported.peak_count,
                .pitch = &imported.pitch,
                .analysis_revision = imported.analysis_revision,
                .peak_readout_mode =
                    analysis_peak_readout_mode,
            };
            pending_analysis_actions = draw_analysis_page(&theme, shell_frame.workspace, &analysis_view);
            if (analysis_view.loaded) {
                imported.region_start_seconds = fminf(fmaxf(pending_analysis_actions.region_start_seconds, 0.0f),
                                                       imported.audio.duration_seconds);
                float remaining = imported.audio.duration_seconds - imported.region_start_seconds;
                imported.region_duration_seconds = fminf(
                    fmaxf(pending_analysis_actions.region_duration_seconds, fminf(0.01f, remaining)), remaining);
            }
        } else if (active_page == APP_PAGE_HARMONIC_LAB) {
            int maximum_components = imported.fourier_analysis.component_count > 1000U
                                         ? 1000
                                         : (int)imported.fourier_analysis.component_count;
            char lab_player_title[96];
            switch (lab_playback) {
                case PLAYBACK_LAB_HARMONIC:
                    snprintf(lab_player_title,
                             sizeof(lab_player_title),
                             "Integer-harmonic resynthesis");
                    break;
                case PLAYBACK_LAB_FRAME:
                    snprintf(lab_player_title,
                             sizeof(lab_player_title),
                             "Original windowed FFT frame");
                    break;
                case PLAYBACK_LAB_FOURIER:
                    snprintf(lab_player_title,
                             sizeof(lab_player_title),
                             "Top-%d Fourier frame",
                             imported.fourier_rendered_components);
                    break;
                default:
                    snprintf(lab_player_title,
                             sizeof(lab_player_title),
                             "Original selected region");
                    break;
            }
            HarmonicLabView lab_view = {
                .source_loaded = imported.audio.mono.samples != NULL,
                .analyzed = imported.analyzed,
                .harmonic_ready = imported.harmonic_ready,
                .fourier_ready = imported.fourier_ready,
                .playing_original = audio_clip_is_playing(&imported.region_clip),
                .playing_harmonic = audio_clip_is_playing(&imported.harmonic_clip),
                .playing_frame = audio_clip_is_playing(&imported.frame_original_clip),
                .playing_fourier = audio_clip_is_playing(&imported.fourier_clip),
                .player = playback_player_view(lab_playback,
                                               lab_player_title,
                                               &clip,
                                               &imported),
                .file_name = imported.file_name,
                .status = imported.lab_status,
                .region_start_seconds = imported.analyzed_region_start_seconds,
                .region_duration_seconds = imported.analyzed_region_duration_seconds,
                .pitch = &imported.pitch,
                .harmonics = imported.harmonics,
                .harmonic_count = imported.harmonic_count,
                .detected_harmonic_count = imported.detected_harmonic_count,
                .fourier_analysis = &imported.fourier_analysis,
                .top_component_count = imported.fourier_top_components,
                .rendered_component_count = imported.fourier_rendered_components,
                .maximum_component_count = maximum_components,
            };
            pending_lab_actions = draw_harmonic_lab_page(&theme, shell_frame.workspace, &lab_view);
            if (maximum_components > 0) {
                imported.fourier_top_components = pending_lab_actions.top_component_count;
                if (imported.fourier_top_components < 1) imported.fourier_top_components = 1;
                if (imported.fourier_top_components > maximum_components) {
                    imported.fourier_top_components = maximum_components;
                }
            }
        } else if (active_page == APP_PAGE_SPECTROGRAM) {
            float maximum_frequency =
                imported.spectrogram.maximum_frequency > 0.0f
                    ? imported.spectrogram.maximum_frequency
                    : fminf(STFT_MAXIMUM_FREQUENCY,
                            (float)imported.audio.mono.sample_rate * 0.5f);
            char spectrogram_player_title[96];
            if (spectrogram_playback ==
                PLAYBACK_SPECTROGRAM_RECONSTRUCTION) {
                if (imported.full_file_mode ==
                    FULL_FILE_MODE_FIXED_GLOBAL) {
                    if (imported.global_selection_mode ==
                        GLOBAL_SELECTION_SPECTRAL_ENERGY) {
                        snprintf(
                            spectrogram_player_title,
                            sizeof(spectrogram_player_title),
                            imported
                                        .global_reconstruction_channels >
                                    1U
                                ? "%.2f%% energy / %d bins per channel"
                                : "%.2f%% energy / %d bins",
                            imported
                                    .global_selected_energy_target *
                                100.0f,
                            imported
                                .global_rendered_components);
                    } else {
                        snprintf(
                            spectrogram_player_title,
                            sizeof(spectrogram_player_title),
                            imported
                                        .global_reconstruction_channels >
                                    1U
                                ? "%d FFT bins per channel"
                                : "%d FFT bins",
                            imported
                                .global_rendered_components);
                    }
                } else {
                    snprintf(
                        spectrogram_player_title,
                        sizeof(spectrogram_player_title),
                        "Time-varying Top-%d per channel",
                        active_full_file_rendered_components(
                            &imported));
                }
            } else {
                snprintf(spectrogram_player_title,
                         sizeof(spectrogram_player_title),
                         "Original full file");
            }
            bool fixed_global_mode =
                imported.full_file_mode ==
                FULL_FILE_MODE_FIXED_GLOBAL;
            bool full_file_processing =
                active_full_file_processing(&imported);
            SpectrogramView spectrogram_view = {
                .source_loaded = imported.audio.mono.samples != NULL,
                .processing = full_file_processing,
                .ready = active_full_file_ready(&imported),
                .playing_original = audio_clip_is_playing(&imported.full_clip),
                .playing_reconstruction =
                    audio_clip_is_playing(
                        active_full_file_clip(&imported)),
                .player = playback_player_view(spectrogram_playback,
                                               spectrogram_player_title,
                                               &clip,
                                               &imported),
                .file_name = imported.file_name,
                .status = active_full_file_status(&imported),
                .sample_rate = imported.audio.mono.sample_rate,
                .reconstruction_channels =
                    imported.reconstruction_channels,
                .source_reconstruction_channels =
                    imported.reconstruction_channels,
                .global_reconstruction_channels =
                    imported.global_reconstruction_channels,
                .duration_seconds = imported.audio.duration_seconds,
                .window_size = STFT_WINDOW_SIZE,
                .hop_size = STFT_HOP_SIZE,
                .frame_count = imported.stft_frame_count,
                .source_sample_count =
                    imported.audio.mono.count,
                .transform_size =
                    fixed_global_mode
                        ? imported.global_fourier_job.fft_size
                        : STFT_WINDOW_SIZE,
                .frequency_resolution =
                    fixed_global_mode
                        ? global_fourier_job_frequency_resolution(
                              &imported.global_fourier_job)
                        : (imported.audio.mono.sample_rate > 0U
                               ? (float)imported.audio.mono.sample_rate /
                                     (float)STFT_WINDOW_SIZE
                               : 0.0f),
                .selected_top_components =
                    fixed_global_mode
                        ? (imported.global_selection_mode ==
                                   GLOBAL_SELECTION_SPECTRAL_ENERGY
                               ? imported
                                     .global_selected_components
                               : imported
                                     .global_bin_budget_components)
                        : imported.adaptive_selected_components,
                .rendered_top_components =
                    fixed_global_mode
                        ? imported.global_rendered_components
                        : imported.adaptive_rendered_components,
                .progress =
                    full_file_processing
                        ? (fixed_global_mode
                               ? background_task_progress(
                                     &imported.global_task)
                               : background_task_progress(
                                     &imported.adaptive_task))
                        : (active_full_file_ready(&imported)
                               ? 1.0f
                               : 0.0f),
                .retained_energy =
                    fixed_global_mode
                        ? imported.global_retained_energy
                        : imported.adaptive_retained_energy,
                .maximum_top_components =
                    fixed_global_mode
                        ? imported.global_fourier_job
                              .maximum_component_count
                        : STFT_MAX_TOP_COMPONENTS,
                .mode = imported.full_file_mode,
                .global_channel_mode =
                    imported.global_channel_mode,
                .global_selection_mode =
                    imported.global_selection_mode,
                .selected_energy_target =
                    imported.global_selected_energy_target,
                .global_estimated_mono_bytes =
                    global_model_estimated_bytes(
                        &imported,
                        GLOBAL_FOURIER_CHANNEL_MONO),
                .global_estimated_source_bytes =
                    global_model_estimated_bytes(
                        &imported,
                        GLOBAL_FOURIER_CHANNEL_SOURCE),
                .global_memory_limit_bytes =
                    imported.global_model_memory_limit_bytes,
                .maximum_frequency = maximum_frequency,
                .spectrogram_texture = imported.spectrogram_texture_loaded
                                           ? &imported.spectrogram_texture
                                           : NULL,
            };
            pending_spectrogram_actions =
                draw_spectrogram_page(&theme, shell_frame.workspace, &spectrogram_view);
        } else if (active_page == APP_PAGE_SETTINGS) {
            SettingsView settings_view = {
                .global_memory_limit_bytes =
                    imported.global_model_memory_limit_bytes,
            };
            pending_settings_actions =
                draw_settings_page(
                    &theme,
                    shell_frame.workspace,
                    audio_ready,
                    &settings_view);
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
    imported_analysis_unload(&imported);
    spectrum_free(&spectrum);
    sample_buffer_free(&generated);
    CloseAudioDevice();
    theme_unload(&theme);
    CloseWindow();
    return 0;
}





