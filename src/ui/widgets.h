#ifndef SPECTRA_WIDGETS_H
#define SPECTRA_WIDGETS_H

#include "raylib.h"
#include "ui/theme.h"

#include <stdbool.h>

typedef struct {
    bool available;
    bool playing;
    bool paused;
    const char *title;
    float position_seconds;
    float duration_seconds;
} TransportPlayerView;

typedef struct {
    bool toggle_play_pause;
    bool stop;
} TransportPlayerActions;

bool draw_button(const AppTheme *theme, Rectangle bounds, const char *text, Color accent);
bool draw_choice_button(const AppTheme *theme,
                        Rectangle bounds,
                        const char *text,
                        bool selected);
TransportPlayerActions draw_transport_player(const AppTheme *theme,
                                              Rectangle bounds,
                                              const TransportPlayerView *view);
float draw_slider(const AppTheme *theme, Rectangle bounds, const char *label, float value, float min_value, float max_value);
void draw_panel(const AppTheme *theme, Rectangle bounds, const char *title);

#endif
