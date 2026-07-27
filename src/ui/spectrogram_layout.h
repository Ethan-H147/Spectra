#ifndef SPECTRA_UI_SPECTROGRAM_LAYOUT_H
#define SPECTRA_UI_SPECTROGRAM_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    float x;
    float y;
    float width;
    float height;
} SpectrogramLayoutRect;

typedef struct {
    SpectrogramLayoutRect workspace;
    SpectrogramLayoutRect inspector;
    SpectrogramLayoutRect inspector_content;
    SpectrogramLayoutRect canvas;

    SpectrogramLayoutRect mode_control;
    SpectrogramLayoutRect choose_audio_button;
    SpectrogramLayoutRect original_button;
    SpectrogramLayoutRect reconstruction_button;
    SpectrogramLayoutRect export_button;
    SpectrogramLayoutRect player;
    SpectrogramLayoutRect fixed_basis_control;
    SpectrogramLayoutRect fixed_presets;
    SpectrogramLayoutRect fixed_custom_input;
    SpectrogramLayoutRect adaptive_presets;
    SpectrogramLayoutRect adaptive_custom_input;
    SpectrogramLayoutRect progress_track;

    SpectrogramLayoutRect canvas_title;
    SpectrogramLayoutRect legend_bar;
    SpectrogramLayoutRect legend_labels;
    SpectrogramLayoutRect stats_panel;
    SpectrogramLayoutRect stats_content;
    SpectrogramLayoutRect plot;
} SpectrogramPageLayout;

SpectrogramPageLayout spectrogram_page_layout(float x,
                                              float y,
                                              float width,
                                              float height);

bool spectrogram_page_layout_validate(
    const SpectrogramPageLayout *layout,
    bool fixed_mode,
    char *message,
    size_t message_size);

#endif
