#ifndef SPECTRA_THEME_H
#define SPECTRA_THEME_H

#include "raylib.h"

#include <stdbool.h>

typedef struct {
    Font font;
    bool owns_font;
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
void theme_unload(AppTheme *theme);
void theme_draw_text(const AppTheme *theme, const char *text, float x, float y, float size, Color color);
int theme_measure_text(const AppTheme *theme, const char *text, float size);

#endif
