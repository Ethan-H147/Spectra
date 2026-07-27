#ifndef SPECTRA_THEME_H
#define SPECTRA_THEME_H

#include "raylib.h"

#include <stdbool.h>

typedef struct {
    Font font;
    Font bold_font;
    Font fallback_font;
    bool owns_font;
    bool owns_bold_font;
    bool owns_fallback_font;
    float text_scale;
    Color background;
    Color panel;
    Color panel_border;
    Color text;
    Color muted_text;
    Color accent;
    Color blue;
    Color danger;
} AppTheme;

void theme_init(AppTheme *theme);
bool theme_ensure_text_coverage(AppTheme *theme,
                                const char *utf8_text);
void theme_unload(AppTheme *theme);
void theme_draw_text(const AppTheme *theme, const char *text, float x, float y, float size, Color color);
void theme_draw_heading(const AppTheme *theme, const char *text, float x, float y, float size, Color color);
float theme_scaled_size(const AppTheme *theme, float size);
int theme_measure_text(const AppTheme *theme, const char *text, float size);
int theme_measure_heading(const AppTheme *theme, const char *text, float size);

#endif
