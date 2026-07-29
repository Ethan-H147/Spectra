#ifndef SPECTRA_APP_SHELL_H
#define SPECTRA_APP_SHELL_H

#include "dsp/dsp_types.h"
#include "dsp/fourier_reconstruction.h"
#include "dsp/harmonic_analysis.h"
#include "dsp/pitch_detection.h"
#include "raylib.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include <stdbool.h>

typedef enum {
    APP_PAGE_OVERVIEW = 0,
    APP_PAGE_SYNTH,
    APP_PAGE_ANALYSIS,
    APP_PAGE_HARMONIC_LAB,
    APP_PAGE_SPECTROGRAM,
    APP_PAGE_SETTINGS,
    APP_PAGE_COUNT
} AppPage;

typedef struct {
    AppPage page;
    Rectangle workspace;
    bool toggle_fullscreen;
    bool toggle_help;
} AppShellFrame;

typedef enum {
    PEAK_READOUT_INTERPOLATED = 0,
    PEAK_READOUT_FFT_BINS
} PeakReadoutMode;

typedef struct {
    bool loaded;
    bool analyzed;
    bool playing_full;
    bool playing_region;
    TransportPlayerView player;
    const char *file_name;
    const char *status;
    unsigned int source_channels;
    unsigned int sample_rate;
    float duration_seconds;
    float region_start_seconds;
    float region_duration_seconds;
    const SampleBuffer *samples;
    const float *waveform_minimums;
    const float *waveform_maximums;
    size_t waveform_bin_count;
    const Spectrum *spectrum;
    const Peak *peaks;
    int peak_count;
    const PitchEstimate *pitch;
    unsigned int analysis_revision;
    PeakReadoutMode peak_readout_mode;
} AudioAnalysisView;

typedef struct {
    bool choose_file;
    bool play_full;
    bool play_region;
    bool toggle_play_pause;
    bool stop;
    bool analyze_region;
    bool select_peak_readout_mode;
    PeakReadoutMode peak_readout_mode;
    float region_start_seconds;
    float region_duration_seconds;
} AudioAnalysisActions;

typedef struct {
    bool source_loaded;
    bool analyzed;
    bool harmonic_ready;
    bool fourier_ready;
    bool playing_original;
    bool playing_harmonic;
    bool playing_frame;
    bool playing_fourier;
    TransportPlayerView player;
    const char *file_name;
    const char *status;
    float region_start_seconds;
    float region_duration_seconds;
    const PitchEstimate *pitch;
    const ExtractedHarmonic *harmonics;
    int harmonic_count;
    int detected_harmonic_count;
    const FourierFrameAnalysis *fourier_analysis;
    int top_component_count;
    int rendered_component_count;
    int maximum_component_count;
} HarmonicLabView;

typedef struct {
    bool open_analysis;
    bool play_original;
    bool play_harmonic;
    bool play_frame;
    bool play_fourier;
    bool toggle_play_pause;
    bool stop;
    bool export_harmonic;
    bool export_fourier;
    bool rebuild_fourier;
    int top_component_count;
} HarmonicLabActions;

typedef enum {
    FULL_FILE_MODE_FIXED_GLOBAL = 0,
    FULL_FILE_MODE_TIME_VARYING_STFT
} FullFileReconstructionMode;

typedef enum {
    GLOBAL_SELECTION_PADDED_FFT_BINS = 0,
    GLOBAL_SELECTION_SPECTRAL_ENERGY
} GlobalFourierSelectionMode;

typedef enum {
    GLOBAL_FOURIER_CHANNEL_MONO = 0,
    GLOBAL_FOURIER_CHANNEL_SOURCE
} GlobalFourierChannelMode;

typedef struct {
    bool source_loaded;
    bool processing;
    bool ready;
    bool playing_original;
    bool playing_reconstruction;
    TransportPlayerView player;
    const char *file_name;
    const char *status;
    unsigned int sample_rate;
    unsigned int reconstruction_channels;
    unsigned int source_reconstruction_channels;
    unsigned int global_reconstruction_channels;
    float duration_seconds;
    unsigned int window_size;
    unsigned int hop_size;
    size_t frame_count;
    size_t source_sample_count;
    size_t transform_size;
    float frequency_resolution;
    int selected_top_components;
    int rendered_top_components;
    float progress;
    float retained_energy;
    int maximum_top_components;
    FullFileReconstructionMode mode;
    GlobalFourierChannelMode global_channel_mode;
    GlobalFourierSelectionMode global_selection_mode;
    float selected_energy_target;
    size_t global_estimated_mono_bytes;
    size_t global_estimated_source_bytes;
    size_t global_memory_limit_bytes;
    float maximum_frequency;
    const Texture2D *spectrogram_texture;
} SpectrogramView;

typedef struct {
    bool open_analysis;
    bool play_original;
    bool play_reconstruction;
    bool toggle_play_pause;
    bool stop;
    bool export_reconstruction;
    bool select_mode;
    FullFileReconstructionMode mode;
    bool select_global_channel_mode;
    GlobalFourierChannelMode global_channel_mode;
    bool select_global_selection_mode;
    GlobalFourierSelectionMode global_selection_mode;
    bool select_top_components;
    int top_component_count;
    bool select_energy_target;
    float energy_target;
} SpectrogramActions;

typedef struct {
    size_t global_memory_limit_bytes;
} SettingsView;

typedef struct {
    bool select_global_memory_limit;
    size_t global_memory_limit_bytes;
} SettingsActions;

AppShellFrame draw_app_shell(const AppTheme *theme,
                             AppPage active_page,
                             bool audio_ready,
                             bool help_open);
AppPage draw_overview_page(const AppTheme *theme, Rectangle workspace);
AudioAnalysisActions draw_analysis_page(const AppTheme *theme,
                                        Rectangle workspace,
                                        const AudioAnalysisView *view);
HarmonicLabActions draw_harmonic_lab_page(const AppTheme *theme,
                                          Rectangle workspace,
                                          const HarmonicLabView *view);
SpectrogramActions draw_spectrogram_page(const AppTheme *theme,
                                         Rectangle workspace,
                                         const SpectrogramView *view);
SettingsActions draw_settings_page(AppTheme *theme,
                                   Rectangle workspace,
                                   bool audio_ready,
                                   const SettingsView *view);

void shell_draw_card(const AppTheme *theme, Rectangle bounds, const char *title, const char *subtitle);
void shell_draw_badge(const AppTheme *theme, Rectangle bounds, const char *text, Color color);

#endif
