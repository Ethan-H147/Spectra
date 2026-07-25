#ifndef SPECTRA_APP_SHELL_H
#define SPECTRA_APP_SHELL_H

#include "raylib.h"
#include "ui/theme.h"

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
} AppShellFrame;

AppShellFrame draw_app_shell(const AppTheme *theme, AppPage active_page, bool audio_ready);
AppPage draw_overview_page(const AppTheme *theme, Rectangle workspace);
void draw_analysis_page(const AppTheme *theme, Rectangle workspace);
void draw_harmonic_lab_page(const AppTheme *theme, Rectangle workspace);
void draw_spectrogram_page(const AppTheme *theme, Rectangle workspace);
void draw_settings_page(AppTheme *theme, Rectangle workspace, bool audio_ready);

void shell_draw_card(const AppTheme *theme, Rectangle bounds, const char *title, const char *subtitle);
void shell_draw_badge(const AppTheme *theme, Rectangle bounds, const char *text, Color color);

#endif
