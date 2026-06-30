#ifndef SPECTRA_WIDGETS_H
#define SPECTRA_WIDGETS_H

#include "raylib.h"
#include "ui/theme.h"

#include <stdbool.h>

bool draw_button(const AppTheme *theme, Rectangle bounds, const char *text, Color accent);
float draw_slider(const AppTheme *theme, Rectangle bounds, const char *label, float value, float min_value, float max_value);
void draw_panel(const AppTheme *theme, Rectangle bounds, const char *title);

#endif
