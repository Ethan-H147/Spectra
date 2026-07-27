#include "ui/app_shell.h"

#include "ui/spectrogram_layout.h"
#include "ui/widgets.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIDEBAR_WIDTH 280.0f
#define TOPBAR_HEIGHT 88.0f
#define STATUSBAR_HEIGHT 40.0f
#define CONTENT_MARGIN 32.0f
#define UI_SPACE_1 8.0f
#define UI_SPACE_2 16.0f
#define UI_SPACE_3 24.0f
#define UI_SPACE_4 32.0f
#define CARD_BODY_OFFSET 104.0f

typedef enum {
    SHELL_ICON_HOME = 0,
    SHELL_ICON_SYNTH,
    SHELL_ICON_ANALYSIS,
    SHELL_ICON_HARMONICS,
    SHELL_ICON_SPECTROGRAM,
    SHELL_ICON_SETTINGS
} ShellIcon;

typedef struct {
    AppPage page;
    const char *label;
    const char *shortcut;
    ShellIcon icon;
} NavItem;

static const NavItem NAV_ITEMS[] = {
    {APP_PAGE_OVERVIEW, "Overview", "1", SHELL_ICON_HOME},
    {APP_PAGE_SYNTH, "Synthesizer", "2", SHELL_ICON_SYNTH},
    {APP_PAGE_ANALYSIS, "Audio Analysis", "3", SHELL_ICON_ANALYSIS},
    {APP_PAGE_HARMONIC_LAB, "Harmonic Lab", "4", SHELL_ICON_HARMONICS},
    {APP_PAGE_SPECTROGRAM, "Spectrogram", "5", SHELL_ICON_SPECTROGRAM},
};

static const char *page_title(AppPage page) {
    switch (page) {
        case APP_PAGE_OVERVIEW: return "Overview";
        case APP_PAGE_SYNTH: return "Harmonic Synthesizer";
        case APP_PAGE_ANALYSIS: return "Audio Analysis";
        case APP_PAGE_HARMONIC_LAB: return "Harmonic Lab";
        case APP_PAGE_SPECTROGRAM: return "Spectrogram";
        case APP_PAGE_SETTINGS: return "Settings";
        default: return "Spectra";
    }
}

static const char *page_context(AppPage page) {
    switch (page) {
        case APP_PAGE_OVERVIEW: return "Project / Start";
        case APP_PAGE_SYNTH: return "Generate / Additive synthesis";
        case APP_PAGE_ANALYSIS: return "Analyze / Imported audio";
        case APP_PAGE_HARMONIC_LAB: return "Transform / Resynthesis";
        case APP_PAGE_SPECTROGRAM: return "Visualize / Time-frequency";
        case APP_PAGE_SETTINGS: return "Application / Preferences";
        default: return "";
    }
}

static void draw_icon(ShellIcon icon, Vector2 center, Color color) {
    float x = center.x;
    float y = center.y;
    switch (icon) {
        case SHELL_ICON_HOME:
            DrawTriangle((Vector2){x, y - 9.0f}, (Vector2){x - 10.0f, y}, (Vector2){x + 10.0f, y}, color);
            DrawRectangleLinesEx((Rectangle){x - 7.0f, y, 14.0f, 10.0f}, 1.5f, color);
            break;
        case SHELL_ICON_SYNTH:
            DrawLineEx((Vector2){x - 9.0f, y - 7.0f}, (Vector2){x + 9.0f, y - 7.0f}, 1.5f, color);
            DrawLineEx((Vector2){x - 9.0f, y}, (Vector2){x + 9.0f, y}, 1.5f, color);
            DrawLineEx((Vector2){x - 9.0f, y + 7.0f}, (Vector2){x + 9.0f, y + 7.0f}, 1.5f, color);
            DrawCircleV((Vector2){x - 3.0f, y - 7.0f}, 3.0f, color);
            DrawCircleV((Vector2){x + 4.0f, y}, 3.0f, color);
            DrawCircleV((Vector2){x - 5.0f, y + 7.0f}, 3.0f, color);
            break;
        case SHELL_ICON_ANALYSIS:
            DrawLineEx((Vector2){x - 10.0f, y + 6.0f}, (Vector2){x - 6.0f, y - 3.0f}, 2.0f, color);
            DrawLineEx((Vector2){x - 6.0f, y - 3.0f}, (Vector2){x - 1.0f, y + 4.0f}, 2.0f, color);
            DrawLineEx((Vector2){x - 1.0f, y + 4.0f}, (Vector2){x + 4.0f, y - 8.0f}, 2.0f, color);
            DrawLineEx((Vector2){x + 4.0f, y - 8.0f}, (Vector2){x + 10.0f, y + 5.0f}, 2.0f, color);
            break;
        case SHELL_ICON_HARMONICS:
            for (int i = 0; i < 4; i++) {
                float height = 6.0f + (float)i * 3.0f;
                DrawRectangle((int)(x - 10.0f + (float)i * 6.0f), (int)(y + 9.0f - height), 3, (int)height, color);
            }
            break;
        case SHELL_ICON_SPECTROGRAM:
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 4; col++) {
                    unsigned char alpha = (unsigned char)(90 + row * 45 + col * 16);
                    DrawRectangle((int)(x - 10.0f + col * 6.0f), (int)(y - 8.0f + row * 6.0f), 4, 4,
                                  (Color){color.r, color.g, color.b, alpha});
                }
            }
            break;
        case SHELL_ICON_SETTINGS:
            DrawCircleLines((int)x, (int)y, 9.0f, color);
            DrawCircleLines((int)x, (int)y, 3.0f, color);
            DrawLineEx((Vector2){x, y - 12.0f}, (Vector2){x, y - 8.0f}, 2.0f, color);
            DrawLineEx((Vector2){x, y + 8.0f}, (Vector2){x, y + 12.0f}, 2.0f, color);
            DrawLineEx((Vector2){x - 12.0f, y}, (Vector2){x - 8.0f, y}, 2.0f, color);
            DrawLineEx((Vector2){x + 8.0f, y}, (Vector2){x + 12.0f, y}, 2.0f, color);
            break;
    }
}

static void draw_brand_mark(Rectangle bounds) {
    DrawRectangleRounded(bounds, 0.22f, 8, (Color){250, 250, 250, 255});

    Vector2 previous = {bounds.x + bounds.width * 0.14f, bounds.y + bounds.height * 0.50f};
    for (int i = 1; i <= 32; i++) {
        float t = (float)i / 32.0f;
        Vector2 current = {
            bounds.x + bounds.width * (0.14f + t * 0.72f),
            bounds.y + bounds.height * (0.50f + sinf(t * 4.0f * PI) * 0.25f),
        };
        DrawLineEx(previous, current, fmaxf(2.5f, bounds.width * 0.075f), BLACK);
        previous = current;
    }
}

static bool draw_nav_item(const AppTheme *theme, Rectangle bounds, const NavItem *item, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color fill = active ? (Color){45, 45, 48, 255} : (hovered ? (Color){37, 37, 40, 255} : BLANK);
    Color color = active ? theme->text : theme->muted_text;

    if (fill.a > 0) DrawRectangleRounded(bounds, 0.08f, 6, fill);
    if (active) DrawRectangle((int)bounds.x, (int)bounds.y + 7, 3, (int)bounds.height - 14, theme->accent);
    draw_icon(item->icon, (Vector2){bounds.x + 25.0f, bounds.y + bounds.height * 0.5f}, color);
    float label_y = bounds.y + (bounds.height - theme_scaled_size(theme, 15.0f)) * 0.5f - 1.0f;
    float shortcut_y = bounds.y + (bounds.height - theme_scaled_size(theme, 13.0f)) * 0.5f - 1.0f;
    theme_draw_text(theme, item->label, bounds.x + 48.0f, label_y, 15.0f, color);
    float shortcut_x = bounds.x + bounds.width - 24.0f;
    if (bounds.x + 48.0f + (float)theme_measure_text(theme, item->label, 15.0f) + 18.0f < shortcut_x) {
        theme_draw_text(theme, item->shortcut, shortcut_x, shortcut_y, 13.0f,
                        active ? theme->muted_text : (Color){98, 98, 103, 255});
    }
    return clicked;
}

static bool draw_help_button(const AppTheme *theme, Rectangle bounds, bool active) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool held = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    Color fill = active ? (Color){34, 70, 112, 255}
                        : (held ? (Color){38, 42, 49, 255}
                                : (hovered ? (Color){49, 53, 61, 255}
                                           : (Color){42, 43, 47, 255}));
    Color border = active ? theme->accent
                          : (hovered ? (Color){79, 83, 92, 255}
                                     : (Color){58, 60, 66, 255});

    Rectangle visual_bounds = bounds;
    if (held) {
        visual_bounds.y += 1.0f;
    }
    DrawRectangleRounded(visual_bounds, 0.16f, 8, fill);
    DrawRectangleRoundedLines(visual_bounds, 0.16f, 8, border);

    Vector2 center = {
        visual_bounds.x + visual_bounds.width * 0.5f,
        visual_bounds.y + visual_bounds.height * 0.5f,
    };
    DrawCircleLines((int)center.x, (int)center.y, 11.0f, active ? WHITE : theme->muted_text);
    int width = theme_measure_heading(theme, "?", 11.0f);
    theme_draw_heading(theme,
                       "?",
                       center.x - (float)width * 0.5f,
                       center.y - theme_scaled_size(theme, 11.0f) * 0.5f - 1.0f,
                       11.0f,
                       active ? WHITE : theme->text);
    return pressed;
}

static float fitted_text_size(const AppTheme *theme,
                              const char *text,
                              float preferred_size,
                              float minimum_size,
                              float maximum_width,
                              bool heading) {
    float size = preferred_size;
    while (size > minimum_size) {
        int measured = heading ? theme_measure_heading(theme, text, size)
                               : theme_measure_text(theme, text, size);
        if ((float)measured <= maximum_width) {
            break;
        }
        size -= 0.5f;
    }
    return fmaxf(size, minimum_size);
}

static void draw_fitted_text(const AppTheme *theme,
                             const char *text,
                             float x,
                             float y,
                             float maximum_width,
                             float preferred_size,
                             float minimum_size,
                             Color color,
                             bool heading) {
    if (text == NULL || text[0] == '\0' || maximum_width <= 0.0f) {
        return;
    }
    float size =
        fitted_text_size(theme, text, preferred_size, minimum_size, maximum_width, heading);
    float height = theme_scaled_size(theme, size) + 4.0f;
    BeginScissorMode((int)x, (int)y, (int)maximum_width, (int)ceilf(height));
    if (heading) {
        theme_draw_heading(theme, text, x, y, size, color);
    } else {
        theme_draw_text(theme, text, x, y, size, color);
    }
    EndScissorMode();
}

void shell_draw_card(const AppTheme *theme, Rectangle bounds, const char *title, const char *subtitle) {
    DrawRectangleRounded(bounds, 0.025f, 8, theme->panel);
    DrawRectangleRoundedLines(bounds, 0.025f, 8, theme->panel_border);
    draw_fitted_text(theme,
                     title,
                     bounds.x + UI_SPACE_2,
                     bounds.y + UI_SPACE_2,
                     bounds.width - UI_SPACE_4,
                     17.0f,
                     12.0f,
                     theme->text,
                     true);
    float subtitle_y =
        bounds.y + UI_SPACE_2 + theme_scaled_size(theme, 17.0f) + UI_SPACE_1;
    draw_fitted_text(theme,
                     subtitle,
                     bounds.x + UI_SPACE_2,
                     subtitle_y,
                     bounds.width - UI_SPACE_4,
                     14.0f,
                     10.5f,
                     theme->muted_text,
                     false);
}

void shell_draw_badge(const AppTheme *theme, Rectangle bounds, const char *text, Color color) {
    DrawRectangleRounded(bounds, 0.25f, 8, (Color){color.r, color.g, color.b, 32});
    int width = theme_measure_heading(theme, text, 11.0f);
    float text_y = bounds.y + (bounds.height - theme_scaled_size(theme, 11.0f)) * 0.5f - 1.0f;
    theme_draw_heading(theme, text, bounds.x + (bounds.width - (float)width) * 0.5f, text_y, 11.0f, color);
}

static void draw_disabled_button(const AppTheme *theme, Rectangle bounds, const char *text) {
    DrawRectangleRounded(bounds, 0.12f, 6, (Color){40, 40, 43, 255});
    DrawRectangleRoundedLines(bounds, 0.12f, 6, (Color){55, 55, 60, 255});
    int width = theme_measure_heading(theme, text, 14.0f);
    float text_y = bounds.y + (bounds.height - theme_scaled_size(theme, 14.0f)) * 0.5f - 1.0f;
    theme_draw_heading(theme, text, bounds.x + (bounds.width - (float)width) * 0.5f, text_y, 14.0f,
                       (Color){112, 112, 118, 255});
}

static void draw_status_bar(const AppTheme *theme, bool audio_ready, float window_width, float window_height) {
    Rectangle bounds = {0.0f, window_height - STATUSBAR_HEIGHT, window_width, STATUSBAR_HEIGHT};
    DrawRectangleRec(bounds, (Color){27, 27, 29, 255});
    DrawLine(0, (int)bounds.y, (int)window_width, (int)bounds.y, theme->panel_border);
    Color state_color = audio_ready ? (Color){70, 190, 120, 255} : (Color){229, 160, 62, 255};
    DrawCircle(16, (int)(bounds.y + bounds.height * 0.5f), 4, state_color);
    float text_y = bounds.y + (bounds.height - theme_scaled_size(theme, 12.0f)) * 0.5f - 1.0f;
    theme_draw_text(theme, audio_ready ? "Audio device ready" : "Audio device unavailable", 32.0f,
                    text_y, 12.0f, theme->muted_text);
    const char *pipeline = "44.1 kHz  |  Mono / stereo  |  FFT 16384  |  Local processing";
    int width = theme_measure_text(theme, pipeline, 12.0f);
    float pipeline_x = window_width - (float)width - UI_SPACE_2;
    if (pipeline_x > SIDEBAR_WIDTH + CONTENT_MARGIN) {
        theme_draw_text(theme, pipeline, pipeline_x, text_y, 12.0f, theme->muted_text);
    }
}

AppShellFrame draw_app_shell(const AppTheme *theme,
                             AppPage active_page,
                             bool audio_ready,
                             bool help_open) {
    float window_width = (float)GetScreenWidth();
    float window_height = (float)GetScreenHeight();
    AppShellFrame frame = {
        .page = active_page,
        .workspace = {SIDEBAR_WIDTH + CONTENT_MARGIN,
                      TOPBAR_HEIGHT + 24.0f,
                      window_width - SIDEBAR_WIDTH - CONTENT_MARGIN * 2.0f,
                      window_height - TOPBAR_HEIGHT - STATUSBAR_HEIGHT - 48.0f},
        .toggle_fullscreen = false,
        .toggle_help = false,
    };

    DrawRectangle(0, 0, (int)SIDEBAR_WIDTH, (int)(window_height - STATUSBAR_HEIGHT), (Color){25, 25, 27, 255});
    DrawLine((int)SIDEBAR_WIDTH, 0, (int)SIDEBAR_WIDTH, (int)(window_height - STATUSBAR_HEIGHT), theme->panel_border);
    DrawRectangle((int)SIDEBAR_WIDTH, 0, (int)(window_width - SIDEBAR_WIDTH), (int)TOPBAR_HEIGHT,
                  (Color){29, 29, 31, 255});
    DrawLine((int)SIDEBAR_WIDTH, (int)TOPBAR_HEIGHT, (int)window_width, (int)TOPBAR_HEIGHT, theme->panel_border);

    draw_brand_mark((Rectangle){16, 16, 48, 48});
    theme_draw_heading(theme, "Spectra", 72.0f, 10.0f, 18.0f, theme->text);
    theme_draw_text(theme, "FOURIER AUDIO LAB", 72.0f, 51.0f, 9.5f, theme->muted_text);

    theme_draw_text(theme, "WORKSPACES", 16.0f, 104.0f, 10.0f, (Color){108, 108, 112, 255});
    for (size_t i = 0; i < sizeof(NAV_ITEMS) / sizeof(NAV_ITEMS[0]); i++) {
        Rectangle nav_bounds = {8.0f, 136.0f + (float)i * 64.0f, SIDEBAR_WIDTH - 16.0f, 56.0f};
        if (draw_nav_item(theme, nav_bounds, &NAV_ITEMS[i], active_page == NAV_ITEMS[i].page)) {
            frame.page = NAV_ITEMS[i].page;
        }
    }

    NavItem settings = {APP_PAGE_SETTINGS, "Settings", "", SHELL_ICON_SETTINGS};
    float settings_y = window_height - STATUSBAR_HEIGHT - 72.0f;
    if (draw_nav_item(theme, (Rectangle){8, settings_y, SIDEBAR_WIDTH - 16.0f, 56}, &settings,
                      active_page == APP_PAGE_SETTINGS)) {
        frame.page = APP_PAGE_SETTINGS;
    }

    float topbar_x = SIDEBAR_WIDTH + CONTENT_MARGIN;
    theme_draw_heading(
        theme, help_open ? "Help Center" : page_title(active_page), topbar_x, 8.0f, 18.0f, theme->text);
    theme_draw_text(theme,
                    help_open ? "Learn / Feature guides" : page_context(active_page),
                    topbar_x,
                    51.0f,
                    11.5f,
                    theme->muted_text);

    const char *fullscreen_label = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "Exit full screen  F11" : "Full screen  F11";
    if (draw_button(theme, (Rectangle){window_width - 232.0f, 16, 208, 48}, fullscreen_label, theme->accent)) {
        frame.toggle_fullscreen = true;
    }
    if (draw_help_button(
            theme, (Rectangle){window_width - 288.0f, 16.0f, 48.0f, 48.0f}, help_open)) {
        frame.toggle_help = true;
    }

    draw_status_bar(theme, audio_ready, window_width, window_height);
    return frame;
}

static void draw_step(const AppTheme *theme, Rectangle bounds, const char *number, const char *title,
                      const char *description, bool ready) {
    DrawRectangleRounded(bounds, 0.035f, 8, theme->panel);
    DrawRectangleRoundedLines(bounds, 0.035f, 8, theme->panel_border);
    bool compact = bounds.width < 260.0f;
    float number_x = bounds.x + (compact ? 23.0f : 28.0f);
    float number_y = bounds.y + 30.0f;
    float number_radius = compact ? 15.0f : 17.0f;
    DrawCircleSector((Vector2){number_x, number_y},
                     number_radius,
                     0.0f,
                     360.0f,
                     64,
                     ready ? theme->accent : (Color){70, 70, 74, 255});
    int number_width = theme_measure_heading(theme, number, 12.0f);
    float number_text_y = number_y - theme_scaled_size(theme, 12.0f) * 0.5f - 1.0f;
    theme_draw_heading(theme, number, number_x - (float)number_width * 0.5f, number_text_y, 12.0f, WHITE);
    float title_x = number_x + number_radius + 9.0f;
    float title_y = number_y - theme_scaled_size(theme, 13.0f) * 0.5f - 1.0f;
    theme_draw_heading(theme, title, title_x, title_y, 13.0f, theme->text);
    theme_draw_text(theme, description, bounds.x + 16.0f, bounds.y + 62.0f, 11.5f, theme->muted_text);
    if (!compact) {
        if (ready) shell_draw_badge(theme, (Rectangle){bounds.x + bounds.width - 92.0f, bounds.y + 14.0f, 74.0f, 23.0f},
                                    "READY", (Color){70, 190, 120, 255});
        else shell_draw_badge(theme, (Rectangle){bounds.x + bounds.width - 104.0f, bounds.y + 14.0f, 86.0f, 23.0f},
                              "PLANNED", (Color){229, 160, 62, 255});
    }
}

AppPage draw_overview_page(const AppTheme *theme, Rectangle workspace) {
    AppPage requested_page = APP_PAGE_COUNT;
    theme_draw_heading(theme, "Build sound. See its structure.", workspace.x, workspace.y, 30.0f, theme->text);
    float subtitle_y = workspace.y + theme_scaled_size(theme, 30.0f) + 5.0f;
    theme_draw_text(theme, "A focused Fourier workspace for synthesis, analysis, and additive reconstruction.",
                    workspace.x, subtitle_y, 16.0f, theme->muted_text);

    float card_height = fminf(160.0f, fmaxf(140.0f, workspace.height * 0.19f));
    float card_width = (workspace.width - 32.0f) / 3.0f;
    float card_y = subtitle_y + theme_scaled_size(theme, 16.0f) + 16.0f;
    Rectangle synth_card = {workspace.x, card_y, card_width, card_height};
    Rectangle analyze_card = {synth_card.x + card_width + 16.0f, synth_card.y, card_width, card_height};
    Rectangle reconstruct_card = {analyze_card.x + card_width + 16.0f, synth_card.y, card_width, card_height};
    bool compact_cards = card_width < 460.0f;
    shell_draw_card(theme, synth_card, "Create a harmonic tone",
                    compact_cards ? "16 partials, ADSR, playback" : "16 partials, ADSR, playback, spectrum");
    float action_y = synth_card.y + synth_card.height - 48.0f;
    if (draw_button(theme, (Rectangle){synth_card.x + 18, action_y, 148, 36}, "Open Synth", theme->accent)) {
        requested_page = APP_PAGE_SYNTH;
    }
    shell_draw_badge(theme, (Rectangle){synth_card.x + synth_card.width - 126.0f, action_y + 4.0f, 108, 28},
                     "AVAILABLE", (Color){70, 190, 120, 255});

    shell_draw_card(theme, analyze_card, compact_cards ? "Analyze audio" : "Import and analyze audio",
                    compact_cards ? "Waveform, FFT, pitch" : "Decode, waveform, FFT, estimated pitch");
    if (draw_button(theme, (Rectangle){analyze_card.x + 18, action_y, 148, 36}, "Open Analysis", theme->accent)) {
        requested_page = APP_PAGE_ANALYSIS;
    }
    shell_draw_badge(theme,
                     (Rectangle){analyze_card.x + analyze_card.width - 88.0f, action_y + 4.0f, 70, 28},
                     "READY",
                     (Color){70, 190, 120, 255});
    shell_draw_card(theme, reconstruct_card, compact_cards ? "Reconstruct audio" : "Reconstruct a sound",
                    compact_cards ? "Extract and compare harmonics" : "Extract harmonics and compare playback");
    if (draw_button(theme,
                    (Rectangle){reconstruct_card.x + 18, action_y, 148, 36},
                    "Open Lab",
                    theme->accent)) {
        requested_page = APP_PAGE_HARMONIC_LAB;
    }
    shell_draw_badge(theme,
                     (Rectangle){reconstruct_card.x + reconstruct_card.width - 88.0f, action_y + 4.0f, 70, 28},
                     "READY",
                     (Color){70, 190, 120, 255});

    float signal_title_y = synth_card.y + synth_card.height + 24.0f;
    float step_y = signal_title_y + theme_scaled_size(theme, 19.0f) + 12.0f;
    float step_height = fminf(100.0f, fmaxf(86.0f, workspace.height * 0.12f));
    theme_draw_heading(theme, "Signal path", workspace.x, signal_title_y, 19.0f, theme->text);
    const char *titles[] = {"Source", "Synthesis", "Spectrum", "Pitch", "Harmonics", "Resynthesis"};
    const char *details[] = {"Generated tone", "ADSR + gain", "FFT + peaks", "Estimated f0", "Peak matching", "Additive model"};
    float step_width = (workspace.width - 50.0f) / 6.0f;
    for (int i = 0; i < 6; i++) {
        Rectangle step = {workspace.x + (step_width + 10.0f) * (float)i, step_y, step_width, step_height};
        draw_step(theme, step, TextFormat("%d", i + 1), titles[i], details[i], true);
        if (i < 5) {
            float arrow_y = step.y + step.height * 0.5f;
            DrawTriangle((Vector2){step.x + step.width + 8.0f, arrow_y},
                         (Vector2){step.x + step.width + 2.0f, arrow_y - 4.0f},
                         (Vector2){step.x + step.width + 2.0f, arrow_y + 4.0f}, theme->muted_text);
        }
    }

    float scope_title_y = step_y + step_height + 26.0f;
    float scope_y = scope_title_y + theme_scaled_size(theme, 19.0f) + 12.0f;
    float scope_height = fmaxf(0.0f, workspace.y + workspace.height - scope_y);
    theme_draw_heading(theme, "Project scope", workspace.x, scope_title_y, 19.0f, theme->text);
    Rectangle scope = {workspace.x, scope_y, workspace.width, scope_height};
    shell_draw_card(theme, scope, "An educational DSP instrument", "Technically real, deliberately focused");
    float body_y = scope.y + 16.0f + theme_scaled_size(theme, 17.0f) + 5.0f +
                   theme_scaled_size(theme, 14.0f) + 17.0f;
    float body_step = theme_scaled_size(theme, 14.0f) + 7.0f;
    theme_draw_text(theme, "Spectra generates and analyzes audio locally with Fourier methods and additive synthesis.",
                    scope.x + 22.0f, body_y, 14.0f, theme->muted_text);
    theme_draw_text(theme, "Every workspace maps directly to a stage in the documented signal pipeline.",
                    scope.x + 22.0f, body_y + body_step, 14.0f, theme->muted_text);
    float badge_y = scope.y + scope.height - 38.0f;
    shell_draw_badge(theme, (Rectangle){scope.x + 22, badge_y, 160, 28}, "LOCAL PROCESSING", theme->accent);
    shell_draw_badge(theme, (Rectangle){scope.x + 192, badge_y, 132, 28}, "NO AI CLAIMS", (Color){157, 113, 224, 255});
    return requested_page;
}

static bool draw_drop_zone(const AppTheme *theme, Rectangle bounds, const char *title, const char *subtitle) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    DrawRectangleRounded(bounds, 0.025f, 8, hovered ? (Color){34, 38, 44, 255} : (Color){27, 27, 30, 255});
    DrawRectangleRoundedLines(bounds, 0.025f, 8, hovered ? theme->accent : (Color){72, 72, 78, 255});
    float available_width = bounds.width - UI_SPACE_4;
    float title_size = fitted_text_size(theme, title, 17.0f, 12.0f, available_width, true);
    float subtitle_size =
        fitted_text_size(theme, subtitle, 13.0f, 10.0f, available_width, false);
    float title_height = theme_scaled_size(theme, title_size);
    float subtitle_height = theme_scaled_size(theme, subtitle_size);
    float content_height =
        32.0f + 12.0f + title_height + 4.0f + subtitle_height;
    float content_y =
        bounds.y + fmaxf(UI_SPACE_1, (bounds.height - content_height) * 0.5f);
    float center_x = bounds.x + bounds.width * 0.5f;
    float icon_y = content_y + 16.0f;
    DrawCircleLines((int)center_x, (int)icon_y, 16.0f, theme->muted_text);
    DrawLine((int)center_x, (int)icon_y - 10, (int)center_x, (int)icon_y + 10, theme->muted_text);
    DrawLine((int)center_x - 10, (int)icon_y, (int)center_x + 10, (int)icon_y, theme->muted_text);

    float title_y = content_y + 44.0f;
    int title_width = theme_measure_heading(theme, title, title_size);
    theme_draw_heading(
        theme, title, center_x - (float)title_width * 0.5f, title_y, title_size, theme->text);
    float subtitle_y = title_y + title_height + 4.0f;
    int subtitle_width = theme_measure_text(theme, subtitle, subtitle_size);
    theme_draw_text(theme,
                    subtitle,
                    center_x - (float)subtitle_width * 0.5f,
                    subtitle_y,
                    subtitle_size,
                    theme->muted_text);
    return clicked;
}

static void draw_waveform_overview(const AppTheme *theme,
                                   Rectangle bounds,
                                   const AudioAnalysisView *view) {
    DrawRectangleRec(bounds, (Color){21, 22, 25, 255});
    DrawLine((int)bounds.x,
             (int)(bounds.y + bounds.height * 0.5f),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height * 0.5f),
             (Color){61, 63, 69, 255});
    if (view == NULL || !view->loaded || view->waveform_minimums == NULL ||
        view->waveform_maximums == NULL || view->waveform_bin_count == 0U) {
        const char *message = "Import audio to view its waveform";
        int width = theme_measure_text(theme, message, 14.0f);
        theme_draw_text(theme,
                        message,
                        bounds.x + (bounds.width - (float)width) * 0.5f,
                        bounds.y + bounds.height * 0.5f - theme_scaled_size(theme, 14.0f) * 0.5f,
                        14.0f,
                        theme->muted_text);
        return;
    }

    float selection_start = view->duration_seconds > 0.0f
                                ? view->region_start_seconds / view->duration_seconds
                                : 0.0f;
    float selection_end = view->duration_seconds > 0.0f
                              ? (view->region_start_seconds + view->region_duration_seconds) /
                                    view->duration_seconds
                              : 0.0f;
    selection_start = fmaxf(0.0f, fminf(1.0f, selection_start));
    selection_end = fmaxf(selection_start, fminf(1.0f, selection_end));

    int pixel_width = (int)bounds.width;
    for (int pixel = 0; pixel < pixel_width; pixel++) {
        size_t start_bin = (size_t)pixel * view->waveform_bin_count / (size_t)pixel_width;
        size_t end_bin = (size_t)(pixel + 1) * view->waveform_bin_count / (size_t)pixel_width;
        if (end_bin <= start_bin) end_bin = start_bin + 1U;
        if (end_bin > view->waveform_bin_count) end_bin = view->waveform_bin_count;

        float minimum = view->waveform_minimums[start_bin];
        float maximum = view->waveform_maximums[start_bin];
        for (size_t bin = start_bin + 1U; bin < end_bin; bin++) {
            if (view->waveform_minimums[bin] < minimum) minimum = view->waveform_minimums[bin];
            if (view->waveform_maximums[bin] > maximum) maximum = view->waveform_maximums[bin];
        }

        float normalized_x = (float)pixel / fmaxf(1.0f, bounds.width - 1.0f);
        bool selected = normalized_x >= selection_start && normalized_x <= selection_end;
        Color color = selected ? (Color){57, 205, 190, 255} : (Color){52, 111, 111, 255};
        float y_min = bounds.y + (1.0f - minimum) * 0.5f * bounds.height;
        float y_max = bounds.y + (1.0f - maximum) * 0.5f * bounds.height;
        DrawLine((int)bounds.x + pixel, (int)y_min, (int)bounds.x + pixel, (int)y_max, color);
    }

    float x_start = bounds.x + selection_start * bounds.width;
    float x_end = bounds.x + selection_end * bounds.width;
    DrawRectangleLinesEx((Rectangle){x_start, bounds.y, fmaxf(2.0f, x_end - x_start), bounds.height},
                         2.0f,
                         theme->accent);
}

static void draw_imported_spectrum(const AppTheme *theme,
                                   Rectangle bounds,
                                   const AudioAnalysisView *view) {
    DrawRectangleRec(bounds, (Color){21, 22, 25, 255});
    for (int row = 0; row <= 4; row++) {
        float y = bounds.y + bounds.height * (float)row / 4.0f;
        DrawLine((int)bounds.x, (int)y, (int)(bounds.x + bounds.width), (int)y, (Color){50, 52, 57, 255});
    }
    if (view == NULL || !view->analyzed || view->spectrum == NULL || view->spectrum->count == 0U) {
        const char *message = view != NULL && view->loaded ? "Choose a region and click Analyze region"
                                                          : "Spectrum appears after import";
        int width = theme_measure_text(theme, message, 14.0f);
        theme_draw_text(theme,
                        message,
                        bounds.x + (bounds.width - (float)width) * 0.5f,
                        bounds.y + bounds.height * 0.5f - theme_scaled_size(theme, 14.0f) * 0.5f,
                        14.0f,
                        theme->muted_text);
        return;
    }

    const float minimum_db = -90.0f;
    const float maximum_frequency = fminf(20000.0f, (float)view->sample_rate * 0.5f);
    Vector2 previous = {0};
    bool has_previous = false;
    for (size_t bin = 0; bin < view->spectrum->count; bin++) {
        float frequency = view->spectrum->frequencies[bin];
        if (frequency > maximum_frequency) break;
        float db = 20.0f * log10f(fmaxf(view->spectrum->magnitudes[bin], 0.0000316f));
        db = fmaxf(minimum_db, fminf(0.0f, db));
        Vector2 current = {
            bounds.x + frequency / maximum_frequency * bounds.width,
            bounds.y + bounds.height - (db - minimum_db) / -minimum_db * bounds.height,
        };
        if (has_previous) DrawLineV(previous, current, (Color){62, 145, 242, 255});
        previous = current;
        has_previous = true;
    }

    for (int peak = 0; peak < view->peak_count; peak++) {
        if (view->peaks[peak].frequency > maximum_frequency) continue;
        float db = fmaxf(minimum_db, fminf(0.0f, view->peaks[peak].db));
        float x = bounds.x + view->peaks[peak].frequency / maximum_frequency * bounds.width;
        float y = bounds.y + bounds.height - (db - minimum_db) / -minimum_db * bounds.height;
        DrawCircleSector((Vector2){x, y}, 4.0f, 0.0f, 360.0f, 24, (Color){226, 88, 78, 255});
    }
}

AudioAnalysisActions draw_analysis_page(const AppTheme *theme,
                                        Rectangle workspace,
                                        const AudioAnalysisView *view) {
    AudioAnalysisActions actions = {
        .region_start_seconds = view != NULL ? view->region_start_seconds : 0.0f,
        .region_duration_seconds = view != NULL ? view->region_duration_seconds : 0.0f,
    };
    float column_gap = UI_SPACE_2;
    float proposed_left_width =
        roundf((workspace.width * 0.31f) / UI_SPACE_1) * UI_SPACE_1;
    float left_width = fminf(464.0f, fmaxf(392.0f, proposed_left_width));
    float source_height = 248.0f;
    float transport_height = workspace.height - source_height - column_gap;
    Rectangle source = {workspace.x, workspace.y, left_width, source_height};
    shell_draw_card(theme, source, "Source audio", "Import a local WAV, MP3, FLAC, or OGG file");
    shell_draw_badge(theme, (Rectangle){source.x + source.width - 96, source.y + 16, 80, 32}, "READY",
                     (Color){70, 190, 120, 255});
    if (view == NULL || !view->loaded) {
        const char *drop_message = "Click to browse or drop a file";
        if (view != NULL && view->status != NULL && view->status[0] != '\0' &&
            strcmp(view->status, "Choose or drop an audio file to begin.") != 0) {
            drop_message = view->status;
        }
        actions.choose_file = draw_drop_zone(theme,
                                             (Rectangle){source.x + UI_SPACE_2,
                                                         source.y + CARD_BODY_OFFSET,
                                                         source.width - UI_SPACE_4,
                                                         source.height - CARD_BODY_OFFSET - UI_SPACE_2},
                                             "Drop audio here",
                                             drop_message);
    } else {
        float content_x = source.x + UI_SPACE_2;
        float content_width = source.width - UI_SPACE_4;
        draw_fitted_text(theme,
                         view->file_name,
                         content_x,
                         source.y + CARD_BODY_OFFSET,
                         content_width,
                         16.0f,
                         11.0f,
                         theme->text,
                         true);
        draw_fitted_text(theme,
                         TextFormat("%.2f seconds  |  %u Hz", view->duration_seconds, view->sample_rate),
                         content_x,
                         source.y + CARD_BODY_OFFSET + 48.0f,
                         content_width - 176.0f,
                         13.0f,
                         10.0f,
                         theme->muted_text,
                         false);
        draw_fitted_text(theme,
                         TextFormat("%u source channel%s  ->  mono analysis",
                                    view->source_channels,
                                    view->source_channels == 1U ? "" : "s"),
                         content_x,
                         source.y + CARD_BODY_OFFSET + 96.0f,
                         content_width,
                         13.0f,
                         10.0f,
                         theme->muted_text,
                         false);
        actions.choose_file = draw_button(theme,
                                          (Rectangle){source.x + source.width - 168.0f,
                                                      source.y + CARD_BODY_OFFSET + 40.0f,
                                                      152.0f,
                                                      40.0f},
                                          "Replace audio",
                                          theme->accent);
    }

    Rectangle transport = {workspace.x, source.y + source.height + column_gap, left_width, transport_height};
    shell_draw_card(theme, transport, "Transport and region", "Mono playback and bounded frame analysis");
    float transport_x = transport.x + UI_SPACE_2;
    float transport_width = transport.width - UI_SPACE_4;
    float transport_body_y = transport.y + CARD_BODY_OFFSET;
    float button_width = (transport_width - UI_SPACE_1) / 2.0f;
    Rectangle play_all = {transport_x, transport_body_y, button_width, 40.0f};
    Rectangle play_region = {
        transport_x + button_width + UI_SPACE_1, transport_body_y, button_width, 40.0f};
    Rectangle player_bounds = {
        transport_x, transport_body_y + 56.0f, transport_width, 64.0f};
    if (view != NULL && view->loaded) {
        actions.play_full = draw_button(theme, play_all, view->playing_full ? "Playing all" : "Play all", theme->accent);
        actions.play_region = draw_button(theme, play_region,
                                          view->playing_region ? "Playing region" : "Play region", theme->accent);
        TransportPlayerActions player_actions =
            draw_transport_player(theme, player_bounds, &view->player);
        actions.toggle_play_pause = player_actions.toggle_play_pause;
        actions.stop = player_actions.stop;

        float maximum_start = fmaxf(0.001f, view->duration_seconds - actions.region_duration_seconds);
        actions.region_start_seconds = draw_slider(theme,
                                                   (Rectangle){transport_x,
                                                               transport_body_y + 136.0f,
                                                               transport_width,
                                                               40.0f},
                                                   "Region start (seconds)",
                                                   fminf(actions.region_start_seconds, maximum_start),
                                                   0.0f,
                                                   maximum_start);
        float minimum_duration = fminf(0.05f, view->duration_seconds);
        float maximum_duration = fmaxf(minimum_duration + 0.001f, view->duration_seconds);
        actions.region_duration_seconds = draw_slider(theme,
                                                      (Rectangle){transport_x,
                                                                  transport_body_y + 200.0f,
                                                                  transport_width,
                                                                  40.0f},
                                                      "Region length (seconds)",
                                                      actions.region_duration_seconds,
                                                      minimum_duration,
                                                      maximum_duration);
        if (actions.region_start_seconds + actions.region_duration_seconds > view->duration_seconds) {
            actions.region_start_seconds = fmaxf(0.0f, view->duration_seconds - actions.region_duration_seconds);
        }
        actions.analyze_region = draw_button(theme,
                                             (Rectangle){transport_x,
                                                         transport_body_y + 256.0f,
                                                         transport_width,
                                                         40.0f},
                                             "Analyze selected region",
                                             theme->accent);
        draw_fitted_text(theme,
                         view->status != NULL ? view->status : "",
                         transport_x,
                         transport_body_y + 304.0f,
                         transport_width,
                         12.0f,
                         10.0f,
                         theme->muted_text,
                         false);
    } else {
        draw_disabled_button(theme, play_all, "Play all");
        draw_disabled_button(theme, play_region, "Play region");
        draw_transport_player(theme, player_bounds, view != NULL ? &view->player : NULL);
        draw_fitted_text(theme,
                         "Import a file to enable transport and analysis.",
                         transport_x,
                         transport_body_y + 136.0f,
                         transport_width,
                         13.0f,
                         10.0f,
                         theme->muted_text,
                         false);
    }

    float right_x = workspace.x + left_width + column_gap;
    float right_width = workspace.width - left_width - column_gap;
    float waveform_height =
        fmaxf(272.0f, roundf((workspace.height * 0.44f) / UI_SPACE_1) * UI_SPACE_1);
    Rectangle waveform = {right_x, workspace.y, right_width, waveform_height};
    shell_draw_card(theme, waveform, "Waveform", "Decoded mono samples and selected analysis region");
    Rectangle waveform_plot = {
        waveform.x + UI_SPACE_2,
        waveform.y + CARD_BODY_OFFSET,
        waveform.width - UI_SPACE_4,
        waveform.height - CARD_BODY_OFFSET - 48.0f,
    };
    draw_waveform_overview(theme, waveform_plot, view);
    if (view != NULL && view->loaded) {
        theme_draw_text(theme, "0.00 s", waveform_plot.x, waveform_plot.y + waveform_plot.height + 8.0f, 11.0f,
                        theme->muted_text);
        const char *duration_text = TextFormat("%.2f s", view->duration_seconds);
        int duration_width = theme_measure_text(theme, duration_text, 11.0f);
        theme_draw_text(theme, duration_text,
                        waveform_plot.x + waveform_plot.width - (float)duration_width,
                        waveform_plot.y + waveform_plot.height + 8.0f, 11.0f, theme->muted_text);
    }

    Rectangle spectrum = {right_x, waveform.y + waveform.height + column_gap, right_width,
                          workspace.height - waveform.height - column_gap};
    shell_draw_card(theme, spectrum, "Magnitude spectrum", "Hann window  |  FFT  |  peak detection  |  20 Hz - 20 kHz");
    float readout_height = 176.0f;
    Rectangle spectrum_plot = {
        spectrum.x + UI_SPACE_2,
        spectrum.y + CARD_BODY_OFFSET,
        spectrum.width - UI_SPACE_4,
        fmaxf(40.0f, spectrum.height - CARD_BODY_OFFSET - readout_height),
    };
    draw_imported_spectrum(theme, spectrum_plot, view);

    float readout_y = spectrum.y + spectrum.height - readout_height + UI_SPACE_2;
    float readout_x = spectrum.x + UI_SPACE_2;
    theme_draw_heading(theme, "Detected peaks", readout_x, readout_y, 15.0f, theme->text);
    if (view != NULL && view->analyzed && view->peak_count > 0) {
        int shown = view->peak_count < 3 ? view->peak_count : 3;
        for (int peak = 0; peak < shown; peak++) {
            draw_fitted_text(theme,
                             TextFormat("%d. %.1f Hz  %.1f dB",
                                        peak + 1, view->peaks[peak].frequency, view->peaks[peak].db),
                             readout_x,
                             readout_y + 40.0f + (float)peak * 32.0f,
                             spectrum.width * 0.50f - UI_SPACE_4,
                             12.0f,
                             10.0f,
                             theme->muted_text,
                             false);
        }
    } else {
        theme_draw_text(theme, "No analyzed peaks", readout_x, readout_y + 40.0f, 12.0f, theme->muted_text);
    }

    float pitch_x = spectrum.x + spectrum.width * 0.60f;
    float pitch_width = spectrum.x + spectrum.width - UI_SPACE_2 - pitch_x;
    theme_draw_heading(theme, "Estimated pitch", pitch_x, readout_y, 15.0f, theme->text);
    if (view != NULL && view->analyzed && view->pitch != NULL && view->pitch->valid) {
        draw_fitted_text(theme,
                         TextFormat("%.2f Hz", view->pitch->frequency_hz),
                         pitch_x,
                         readout_y + 40.0f,
                         pitch_width,
                         20.0f,
                         14.0f,
                         theme->text,
                         true);
        draw_fitted_text(theme,
                         TextFormat("%s%d  |  %+.1f cents  |  %d%% confidence",
                                    pitch_note_name(view->pitch->midi_note),
                                    pitch_note_octave(view->pitch->midi_note),
                                    view->pitch->cents,
                                    (int)(view->pitch->confidence * 100.0f + 0.5f)),
                         pitch_x,
                         readout_y + 88.0f,
                         pitch_width,
                         12.0f,
                         9.5f,
                         theme->muted_text,
                         false);
    } else {
        theme_draw_heading(theme, "No stable pitch", pitch_x, readout_y + 40.0f, 17.0f, theme->muted_text);
    }
    return actions;
}

HarmonicLabActions draw_harmonic_lab_page(const AppTheme *theme,
                                          Rectangle workspace,
                                          const HarmonicLabView *view) {
    HarmonicLabActions actions = {
        .top_component_count = view != NULL ? view->top_component_count : 1,
    };
    if (view == NULL || !view->source_loaded) {
        shell_draw_card(theme,
                        workspace,
                        "Reconstruction Lab",
                        "Import and analyze audio before building harmonic or Fourier models");
        shell_draw_badge(theme,
                         (Rectangle){workspace.x + workspace.width - 100, workspace.y + 14, 82, 28},
                         "READY",
                         (Color){70, 190, 120, 255});
        theme_draw_heading(theme,
                           "Start with a stable musical region",
                           workspace.x + 28,
                           workspace.y + 126,
                           24.0f,
                           theme->text);
        theme_draw_text(theme,
                        "Spectra builds an integer-harmonic tone for pitched audio and a phase-preserving Fourier frame.",
                        workspace.x + 28,
                        workspace.y + 174,
                        14.0f,
                        theme->muted_text);
        actions.open_analysis = draw_button(theme,
                                            (Rectangle){workspace.x + 28, workspace.y + 222, 196, 44},
                                            "Open Audio Analysis",
                                            theme->accent);
        return actions;
    }

    float comparison_height = 256.0f;
    Rectangle comparison = {workspace.x, workspace.y, workspace.width, comparison_height};
    shell_draw_card(theme,
                    comparison,
                    "Original vs. harmonic resynthesis",
                    "Compare the selected mono region with an integer-multiple additive model");
    shell_draw_badge(theme,
                     (Rectangle){comparison.x + comparison.width - 140, comparison.y + 14, 122, 28},
                     view->harmonic_ready ? "MODEL READY" : "PITCH NEEDED",
                     view->harmonic_ready ? (Color){70, 190, 120, 255} : (Color){229, 160, 62, 255});
    float button_y = comparison.y + 88.0f;
    float button_width = fminf(212.0f, (comparison.width - 64.0f) / 3.0f);
    Rectangle play_original = {comparison.x + 20, button_y, button_width, 42};
    Rectangle play_harmonic = {play_original.x + button_width + 12, button_y, button_width, 42};
    Rectangle export_harmonic = {play_harmonic.x + button_width + 12, button_y, button_width, 42};
    actions.play_original =
        draw_button(theme, play_original, view->playing_original ? "Playing original" : "Play original", theme->accent);
    if (view->harmonic_ready) {
        actions.play_harmonic = draw_button(
            theme, play_harmonic, view->playing_harmonic ? "Playing model" : "Play harmonic", theme->accent);
        actions.export_harmonic =
            draw_button(theme, export_harmonic, "Export harmonic", theme->blue);
    } else {
        draw_disabled_button(theme, play_harmonic, "Play harmonic");
        draw_disabled_button(theme, export_harmonic, "Export harmonic");
    }
    TransportPlayerActions player_actions =
        draw_transport_player(theme,
                              (Rectangle){comparison.x + 20.0f,
                                          comparison.y + 142.0f,
                                          comparison.width - 40.0f,
                                          64.0f},
                              &view->player);
    actions.toggle_play_pause = player_actions.toggle_play_pause;
    actions.stop = player_actions.stop;
    draw_fitted_text(theme,
                     view->status != NULL ? view->status : "",
                     comparison.x + 20.0f,
                     comparison.y + 218.0f,
                     comparison.width - 40.0f,
                     11.5f,
                     9.5f,
                     theme->muted_text,
                     false);

    float lower_y = comparison.y + comparison.height + 18.0f;
    float lower_height = workspace.height - comparison.height - 18.0f;
    float table_width = (workspace.width - 18.0f) * 0.61f;
    Rectangle table = {workspace.x, lower_y, table_width, lower_height};
    shell_draw_card(theme,
                    table,
                    "Extracted harmonics",
                    "Local spectral peaks matched to integer multiples of the estimated fundamental");
    const char *headers[] = {"#", "Expected", "Detected", "Relative amplitude", "Level"};
    float columns[] = {table.x + 22,
                       table.x + table.width * 0.11f,
                       table.x + table.width * 0.30f,
                       table.x + table.width * 0.52f,
                       table.x + table.width * 0.82f};
    for (int index = 0; index < 5; index++) {
        theme_draw_heading(theme, headers[index], columns[index], table.y + 92, 12.0f, theme->muted_text);
    }
    DrawLine((int)table.x + 18,
             (int)table.y + 124,
             (int)(table.x + table.width - 18),
             (int)table.y + 124,
             theme->panel_border);

    int visible_rows = (int)((table.height - 174.0f) / 34.0f);
    if (visible_rows < 1) visible_rows = 1;
    int row_count = view->harmonic_count < visible_rows ? view->harmonic_count : visible_rows;
    for (int row = 0; row < row_count; row++) {
        const ExtractedHarmonic *harmonic = &view->harmonics[row];
        float y = table.y + 138.0f + (float)row * 34.0f;
        Color value_color = harmonic->detected ? theme->text : (Color){118, 118, 124, 255};
        theme_draw_heading(theme,
                           TextFormat("H%02d", harmonic->harmonic_number),
                           columns[0],
                           y,
                           12.0f,
                           value_color);
        theme_draw_text(theme,
                        TextFormat("%.1f Hz", harmonic->expected_frequency),
                        columns[1],
                        y,
                        12.0f,
                        theme->muted_text);
        theme_draw_text(theme,
                        harmonic->detected ? TextFormat("%.1f Hz", harmonic->detected_frequency) : "--",
                        columns[2],
                        y,
                        12.0f,
                        value_color);
        float bar_width = table.width * 0.22f;
        DrawRectangleRounded((Rectangle){columns[3], y + 5, bar_width, 7}, 0.8f, 8, (Color){57, 57, 62, 255});
        if (harmonic->detected) {
            DrawRectangleRounded((Rectangle){columns[3], y + 5, bar_width * harmonic->amplitude, 7},
                                 0.8f,
                                 8,
                                 theme->accent);
        }
        theme_draw_text(theme,
                        harmonic->detected ? TextFormat("%.1f dB", harmonic->db) : "--",
                        columns[4],
                        y,
                        12.0f,
                        value_color);
    }
    if (row_count == 0) {
        theme_draw_text(theme,
                        "No stable pitched model is available for this region.",
                        table.x + 22,
                        table.y + 150,
                        13.0f,
                        theme->muted_text);
    } else if (row_count < view->harmonic_count) {
        theme_draw_text(theme,
                        TextFormat("+ %d additional harmonic rows", view->harmonic_count - row_count),
                        table.x + 22,
                        table.y + table.height - 30,
                        11.0f,
                        theme->muted_text);
    }

    float right_x = table.x + table.width + 18.0f;
    float right_width = workspace.width - table.width - 18.0f;
    float model_height = fminf(210.0f, fmaxf(170.0f, lower_height * 0.38f));
    Rectangle model = {right_x, lower_y, right_width, model_height};
    shell_draw_card(theme, model, "Harmonic model", "Pitched, phase-free additive approximation");
    const char *labels[] = {"Fundamental", "Detected partials", "Tolerance", "Region"};
    const char *values[] = {
        view->pitch != NULL && view->pitch->valid ? TextFormat("%.2f Hz", view->pitch->frequency_hz) : "--",
        TextFormat("%d / %d", view->detected_harmonic_count, view->harmonic_count),
        "30 cents",
        TextFormat("%.2f - %.2f s",
                   view->region_start_seconds,
                   view->region_start_seconds + view->region_duration_seconds),
    };
    for (int index = 0; index < 4; index++) {
        float y = model.y + 84.0f + (float)index * 22.0f;
        theme_draw_text(theme, labels[index], model.x + 20, y, 12.0f, theme->muted_text);
        int width = theme_measure_heading(theme, values[index], 12.0f);
        theme_draw_heading(
            theme, values[index], model.x + model.width - (float)width - 20.0f, y, 12.0f, theme->text);
    }

    Rectangle fourier = {
        right_x,
        model.y + model.height + 18.0f,
        right_width,
        lower_height - model.height - 18.0f,
    };
    shell_draw_card(theme,
                    fourier,
                    "Top-N Fourier frame",
                    "Phase-preserving reconstruction of the centered Hann-windowed frame");
    shell_draw_badge(theme,
                     (Rectangle){fourier.x + fourier.width - 82, fourier.y + 14, 64, 28},
                     view->fourier_ready ? "READY" : "EMPTY",
                     view->fourier_ready ? (Color){70, 190, 120, 255} : (Color){229, 160, 62, 255});
    if (view->fourier_ready && view->fourier_analysis != NULL) {
        theme_draw_text(theme,
                        TextFormat("%u samples  |  %.3f seconds  |  %zu available bins",
                                   view->fourier_analysis->fft_size,
                                   (float)view->fourier_analysis->fft_size /
                                       (float)view->fourier_analysis->windowed_frame.sample_rate,
                                   view->fourier_analysis->component_count),
                        fourier.x + 20,
                        fourier.y + 82,
                        11.0f,
                        theme->muted_text);
        float slider_value = draw_slider(theme,
                                         (Rectangle){fourier.x + 20, fourier.y + 112, fourier.width - 40, 42},
                                         "Strongest frequency components",
                                         (float)actions.top_component_count,
                                         1.0f,
                                         (float)view->maximum_component_count);
        actions.top_component_count = (int)lroundf(slider_value);
        if (actions.top_component_count < 1) actions.top_component_count = 1;
        actions.rebuild_fourier =
            actions.top_component_count != view->rendered_component_count &&
            !IsMouseButtonDown(MOUSE_LEFT_BUTTON);

        float action_y = fourier.y + 172.0f;
        float compact_width = (fourier.width - 52.0f) / 2.0f;
        actions.play_frame = draw_button(theme,
                                         (Rectangle){fourier.x + 20, action_y, compact_width, 38},
                                         view->playing_frame ? "Playing frame" : "Play frame",
                                         theme->accent);
        actions.play_fourier = draw_button(theme,
                                           (Rectangle){fourier.x + 32 + compact_width, action_y, compact_width, 38},
                                           view->playing_fourier ? "Playing Top-N" : "Play Top-N",
                                           theme->accent);
        if (fourier.height >= 264.0f) {
            actions.export_fourier = draw_button(theme,
                                                 (Rectangle){fourier.x + 20,
                                                             action_y + 50,
                                                             fourier.width - 40,
                                                             36},
                                                 "Export Top-N frame",
                                                 theme->blue);
        }
        if (fourier.height >= 340.0f) {
            float component_title_y = action_y + 108.0f;
            theme_draw_heading(
                theme, "Strongest included components", fourier.x + 20, component_title_y, 13.0f, theme->text);
            float header_y = component_title_y + 30.0f;
            float rank_x = fourier.x + 20;
            float frequency_x = fourier.x + fourier.width * 0.20f;
            float level_x = fourier.x + fourier.width * 0.52f;
            float phase_x = fourier.x + fourier.width * 0.76f;
            theme_draw_text(theme, "#", rank_x, header_y, 11.0f, theme->muted_text);
            theme_draw_text(theme, "Frequency", frequency_x, header_y, 11.0f, theme->muted_text);
            theme_draw_text(theme, "Level", level_x, header_y, 11.0f, theme->muted_text);
            theme_draw_text(theme, "Phase", phase_x, header_y, 11.0f, theme->muted_text);
            DrawLine((int)fourier.x + 20,
                     (int)header_y + 22,
                     (int)(fourier.x + fourier.width - 20),
                     (int)header_y + 22,
                     theme->panel_border);

            int available_rows = (int)((fourier.y + fourier.height - header_y - 50.0f) / 25.0f);
            if (available_rows > 8) available_rows = 8;
            if (available_rows > view->rendered_component_count) {
                available_rows = view->rendered_component_count;
            }
            for (int row = 0; row < available_rows; row++) {
                const FourierComponent *component = &view->fourier_analysis->ranked_components[row];
                float y = header_y + 31.0f + (float)row * 25.0f;
                theme_draw_heading(theme, TextFormat("%d", row + 1), rank_x, y, 11.0f, theme->text);
                theme_draw_text(
                    theme, TextFormat("%.1f Hz", component->frequency), frequency_x, y, 11.0f, theme->muted_text);
                theme_draw_text(theme, TextFormat("%.1f dB", component->db), level_x, y, 11.0f, theme->muted_text);
                theme_draw_text(theme,
                                TextFormat("%+.2f rad", component->phase),
                                phase_x,
                                y,
                                11.0f,
                                theme->muted_text);
            }
        }
    } else {
        theme_draw_text(theme,
                        "Analyze a region containing at least eight samples.",
                        fourier.x + 20,
                        fourier.y + 94,
                        12.0f,
                        theme->muted_text);
    }
    return actions;
}

static bool draw_component_count_input(
    const AppTheme *theme,
    Rectangle bounds,
    FullFileReconstructionMode mode,
    int selected_value,
    int maximum_value,
    int *submitted_value) {
    static bool active = false;
    static bool replace_on_type = false;
    static FullFileReconstructionMode input_mode =
        FULL_FILE_MODE_FIXED_GLOBAL;
    static int synchronized_value = -1;
    static char input_text[16] = "5";

    if (maximum_value < 1) maximum_value = 1;
    if (mode != input_mode ||
        (!active && selected_value != synchronized_value)) {
        input_mode = mode;
        synchronized_value = selected_value;
        snprintf(input_text, sizeof(input_text), "%d", selected_value);
        active = false;
        replace_on_type = false;
    }

    bool compact = bounds.width < 520.0f;
    float label_width = compact ? 76.0f : 136.0f;
    float apply_width = compact ? 72.0f : 84.0f;
    float field_width =
        fminf(compact ? 108.0f : 148.0f,
              bounds.width - label_width - apply_width -
                  UI_SPACE_1 * 2.0f);
    Rectangle field = {
        bounds.x + label_width,
        bounds.y,
        fmaxf(72.0f, field_width),
        bounds.height,
    };
    Rectangle apply = {
        field.x + field.width + UI_SPACE_1,
        bounds.y,
        apply_width,
        bounds.height,
    };
    Vector2 mouse = GetMousePosition();
    bool field_clicked =
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, field);
    if (field_clicked) {
        active = true;
        replace_on_type = true;
    }

    bool submit = false;
    if (active) {
        int character = GetCharPressed();
        while (character > 0) {
            if (character >= '0' && character <= '9') {
                if (replace_on_type) {
                    input_text[0] = '\0';
                    replace_on_type = false;
                }
                size_t length = strlen(input_text);
                if (length + 1U < sizeof(input_text)) {
                    input_text[length] = (char)character;
                    input_text[length + 1U] = '\0';
                }
            }
            character = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t length = strlen(input_text);
            if (replace_on_type) {
                input_text[0] = '\0';
                replace_on_type = false;
            } else if (length > 0U) {
                input_text[length - 1U] = '\0';
            }
        }
        if (IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_KP_ENTER)) {
            submit = true;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            snprintf(input_text,
                     sizeof(input_text),
                     "%d",
                     selected_value);
            active = false;
            replace_on_type = false;
        }
    }

    float text_y =
        bounds.y +
        (bounds.height - theme_scaled_size(theme, 12.5f)) * 0.5f -
        1.0f;
    theme_draw_heading(theme,
                       "Custom",
                       bounds.x,
                       text_y,
                       12.5f,
                       theme->text);
    DrawRectangleRounded(field,
                         0.12f,
                         8,
                         (Color){31, 32, 36, 255});
    DrawRectangleRoundedLines(field,
                              0.12f,
                              8,
                              active ? theme->accent
                                     : theme->panel_border);
    const char *display_text =
        input_text[0] != '\0' ? input_text : "0";
    theme_draw_text(theme,
                    display_text,
                    field.x + 12.0f,
                    text_y,
                    12.5f,
                    active ? theme->text : theme->muted_text);
    if (active &&
        ((int)(GetTime() * 2.0) % 2) == 0) {
        int input_width =
            theme_measure_text(theme, display_text, 12.5f);
        DrawRectangle((int)(field.x + 13.0f +
                            (float)input_width),
                      (int)(text_y + 1.0f),
                      1,
                      (int)theme_scaled_size(theme, 12.5f),
                      theme->text);
    }
    if (draw_button(theme, apply, "Apply", theme->accent)) {
        submit = true;
    }
    float guidance_x = apply.x + apply.width + UI_SPACE_2;
    float guidance_width =
        bounds.x + bounds.width - guidance_x;
    if (guidance_width >= 140.0f) {
        draw_fitted_text(
            theme,
            mode == FULL_FILE_MODE_FIXED_GLOBAL
                ? TextFormat("Range 1 - %d padded bins",
                             maximum_value)
                : TextFormat("Range 1 - %d per frame",
                             maximum_value),
            guidance_x,
            text_y,
            guidance_width,
            11.5f,
            9.5f,
            theme->muted_text,
            false);
    }

    if (submit) {
        long parsed = strtol(input_text, NULL, 10);
        if (parsed < 1L) parsed = 1L;
        if (parsed > (long)maximum_value) {
            parsed = (long)maximum_value;
        }
        *submitted_value = (int)parsed;
        synchronized_value = (int)parsed;
        snprintf(input_text,
                 sizeof(input_text),
                 "%d",
                 synchronized_value);
        active = false;
        replace_on_type = false;
        return synchronized_value != selected_value;
    }
    return false;
}

static bool draw_energy_target_input(
    const AppTheme *theme,
    Rectangle bounds,
    float selected_target,
    float *submitted_target) {
    static bool active = false;
    static bool replace_on_type = false;
    static float synchronized_target = -1.0f;
    static char input_text[16] = "90.00";

    float selected_percent =
        fminf(fmaxf(selected_target * 100.0f, 0.01f),
              100.0f);
    if (!active &&
        fabsf(selected_percent - synchronized_target) >
            0.0005f) {
        synchronized_target = selected_percent;
        snprintf(input_text,
                 sizeof(input_text),
                 "%.2f",
                 selected_percent);
        replace_on_type = false;
    }

    bool compact = bounds.width < 520.0f;
    float label_width = compact ? 76.0f : 136.0f;
    float apply_width = compact ? 72.0f : 84.0f;
    float field_width =
        fminf(compact ? 108.0f : 148.0f,
              bounds.width - label_width - apply_width -
                  UI_SPACE_1 * 2.0f);
    Rectangle field = {
        bounds.x + label_width,
        bounds.y,
        fmaxf(72.0f, field_width),
        bounds.height,
    };
    Rectangle apply = {
        field.x + field.width + UI_SPACE_1,
        bounds.y,
        apply_width,
        bounds.height,
    };
    Vector2 mouse = GetMousePosition();
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        CheckCollisionPointRec(mouse, field)) {
        active = true;
        replace_on_type = true;
    }

    bool submit = false;
    if (active) {
        int character = GetCharPressed();
        while (character > 0) {
            bool digit =
                character >= '0' && character <= '9';
            bool decimal =
                character == '.' &&
                strchr(input_text, '.') == NULL;
            if (digit || decimal) {
                if (replace_on_type) {
                    input_text[0] = '\0';
                    replace_on_type = false;
                }
                size_t length = strlen(input_text);
                if (length + 1U < sizeof(input_text)) {
                    input_text[length] = (char)character;
                    input_text[length + 1U] = '\0';
                }
            }
            character = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            size_t length = strlen(input_text);
            if (replace_on_type) {
                input_text[0] = '\0';
                replace_on_type = false;
            } else if (length > 0U) {
                input_text[length - 1U] = '\0';
            }
        }
        if (IsKeyPressed(KEY_ENTER) ||
            IsKeyPressed(KEY_KP_ENTER)) {
            submit = true;
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            snprintf(input_text,
                     sizeof(input_text),
                     "%.2f",
                     selected_percent);
            active = false;
            replace_on_type = false;
        }
    }

    float text_y =
        bounds.y +
        (bounds.height -
         theme_scaled_size(theme, 12.5f)) *
            0.5f -
        1.0f;
    theme_draw_heading(theme,
                       "Custom",
                       bounds.x,
                       text_y,
                       12.5f,
                       theme->text);
    DrawRectangleRounded(
        field, 0.12f, 8, (Color){31, 32, 36, 255});
    DrawRectangleRoundedLines(
        field,
        0.12f,
        8,
        active ? theme->accent : theme->panel_border);
    const char *display_text =
        input_text[0] != '\0' ? input_text : "0";
    theme_draw_text(theme,
                    TextFormat("%s%%", display_text),
                    field.x + 12.0f,
                    text_y,
                    12.5f,
                    active ? theme->text
                           : theme->muted_text);
    if (active &&
        ((int)(GetTime() * 2.0) % 2) == 0) {
        int input_width = theme_measure_text(
            theme, display_text, 12.5f);
        DrawRectangle(
            (int)(field.x + 13.0f +
                  (float)input_width),
            (int)(text_y + 1.0f),
            1,
            (int)theme_scaled_size(theme, 12.5f),
            theme->text);
    }
    if (draw_button(
            theme, apply, "Apply", theme->accent)) {
        submit = true;
    }
    float guidance_x = apply.x + apply.width + UI_SPACE_2;
    float guidance_width =
        bounds.x + bounds.width - guidance_x;
    if (guidance_width >= 140.0f) {
        draw_fitted_text(theme,
                         "Range 0.01% - 100%",
                         guidance_x,
                         text_y,
                         guidance_width,
                         11.5f,
                         9.5f,
                         theme->muted_text,
                         false);
    }

    if (submit) {
        float parsed = strtof(input_text, NULL);
        if (parsed < 0.01f) parsed = 0.01f;
        if (parsed > 100.0f) parsed = 100.0f;
        *submitted_target = parsed / 100.0f;
        bool changed =
            fabsf(parsed - selected_percent) > 0.0005f;
        synchronized_target = parsed;
        snprintf(input_text,
                 sizeof(input_text),
                 "%.2f",
                 parsed);
        active = false;
        replace_on_type = false;
        return changed;
    }
    return false;
}

static int component_count_for_percent(int maximum_value,
                                       int percent) {
    if (maximum_value < 1) return 1;
    if (percent <= 0) return 1;
    if (percent >= 100) return maximum_value;
    long long scaled =
        (long long)maximum_value * (long long)percent;
    int value = (int)((scaled + 50LL) / 100LL);
    return value < 1 ? 1 : value;
}

static int draw_segmented_control(const AppTheme *theme,
                                  Rectangle bounds,
                                  const char *const *labels,
                                  int count,
                                  int selected_index,
                                  bool enabled) {
    if (count <= 0 || bounds.width <= 0.0f ||
        bounds.height <= 0.0f) {
        return -1;
    }

    Vector2 mouse = GetMousePosition();
    Color surface = enabled ? (Color){31, 32, 36, 255}
                            : (Color){29, 30, 33, 255};
    Color outline = enabled ? (Color){58, 60, 66, 255}
                            : (Color){47, 49, 54, 255};
    DrawRectangleRounded(bounds, 0.18f, 12, surface);
    DrawRectangleRoundedLines(bounds, 0.18f, 12, outline);

    float inset = 4.0f;
    float segment_width =
        (bounds.width - inset * 2.0f) / (float)count;
    int pressed_index = -1;
    for (int index = 0; index < count; index++) {
        Rectangle hit = {
            bounds.x + inset + segment_width * (float)index,
            bounds.y + inset,
            segment_width,
            bounds.height - inset * 2.0f,
        };
        bool hovered =
            enabled && CheckCollisionPointRec(mouse, hit);
        bool held =
            hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        bool selected = index == selected_index;
        Rectangle visual = hit;
        visual.x += 2.0f;
        visual.width -= 4.0f;
        if (held) visual.y += 1.0f;

        if (selected) {
            DrawRectangleRounded(
                visual,
                0.20f,
                10,
                held ? (Color){66, 68, 75, 255}
                     : (Color){58, 60, 67, 255});
        } else if (hovered) {
            DrawRectangleRounded(
                visual, 0.20f, 10, (Color){42, 44, 49, 255});
        }

        float maximum_text_width =
            fmaxf(1.0f, hit.width - 16.0f);
        float font_size = fitted_text_size(
            theme,
            labels[index],
            count > 4 ? 12.0f : 13.0f,
            10.5f,
            maximum_text_width,
            true);
        int text_width =
            theme_measure_heading(theme, labels[index], font_size);
        float text_y =
            hit.y +
            (hit.height - theme_scaled_size(theme, font_size)) *
                0.5f -
            1.0f;
        BeginScissorMode((int)hit.x,
                         (int)hit.y,
                         (int)hit.width,
                         (int)hit.height);
        theme_draw_heading(
            theme,
            labels[index],
            hit.x + (hit.width - (float)text_width) * 0.5f,
            text_y,
            font_size,
            !enabled
                ? (Color){101, 103, 109, 255}
                : (selected ? theme->text : theme->muted_text));
        EndScissorMode();

        if (hovered &&
            IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            pressed_index = index;
        }
    }
    return pressed_index;
}

static void format_grouped_count(size_t value,
                                 char *text,
                                 size_t text_size) {
    char digits[32];
    snprintf(digits, sizeof(digits), "%zu", value);
    size_t length = strlen(digits);
    size_t commas = length > 0U ? (length - 1U) / 3U : 0U;
    size_t output_length = length + commas;
    if (text_size == 0U) return;
    if (output_length + 1U > text_size) {
        snprintf(text, text_size, "%s", digits);
        return;
    }

    text[output_length] = '\0';
    size_t source = length;
    size_t destination = output_length;
    int group = 0;
    while (source > 0U) {
        text[--destination] = digits[--source];
        group++;
        if (group == 3 && source > 0U) {
            text[--destination] = ',';
            group = 0;
        }
    }
}

static void format_compact_duration(float seconds,
                                    char *text,
                                    size_t text_size) {
    int total_seconds = (int)floorf(fmaxf(seconds, 0.0f));
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int remaining_seconds = total_seconds % 60;
    if (hours > 0) {
        snprintf(text,
                 text_size,
                 "%d:%02d:%02d",
                 hours,
                 minutes,
                 remaining_seconds);
    } else {
        snprintf(text,
                 text_size,
                 "%d:%02d",
                 minutes,
                 remaining_seconds);
    }
}

static void draw_compact_stat(const AppTheme *theme,
                              Rectangle bounds,
                              const char *label,
                              const char *value,
                              bool draw_divider) {
    draw_fitted_text(theme,
                     label,
                     bounds.x,
                     bounds.y,
                     bounds.width - UI_SPACE_2,
                     10.5f,
                     9.0f,
                     theme->muted_text,
                     false);
    draw_fitted_text(theme,
                     value,
                     bounds.x,
                     bounds.y + 20.0f,
                     bounds.width - UI_SPACE_2,
                     13.5f,
                     10.5f,
                     theme->text,
                     true);
    if (draw_divider) {
        DrawLine((int)(bounds.x + bounds.width - UI_SPACE_1),
                 (int)bounds.y,
                 (int)(bounds.x + bounds.width - UI_SPACE_1),
                 (int)(bounds.y + bounds.height),
                 (Color){55, 57, 63, 255});
    }
}

static Rectangle raylib_rectangle(
    SpectrogramLayoutRect rect) {
    return (Rectangle){
        rect.x, rect.y, rect.width, rect.height};
}

SpectrogramActions draw_spectrogram_page(const AppTheme *theme,
                                         Rectangle workspace,
                                         const SpectrogramView *view) {
    SpectrogramActions actions = {
        .top_component_count = view != NULL ? view->selected_top_components : 5,
        .mode = view != NULL ? view->mode : FULL_FILE_MODE_FIXED_GLOBAL,
        .global_selection_mode =
            view != NULL
                ? view->global_selection_mode
                : GLOBAL_SELECTION_PADDED_FFT_BINS,
        .energy_target =
            view != NULL ? view->selected_energy_target : 0.90f,
    };
    bool source_loaded = view != NULL && view->source_loaded;
    bool processing = view != NULL && view->processing;
    bool ready = view != NULL && view->ready;
    bool fixed_global =
        view == NULL || view->mode == FULL_FILE_MODE_FIXED_GLOBAL;
    bool energy_selection =
        fixed_global && view != NULL &&
        view->global_selection_mode ==
            GLOBAL_SELECTION_SPECTRAL_ENERGY;
    bool fixed_available =
        !fixed_global ||
        (view != NULL && view->maximum_top_components > 0);
    bool configuration_enabled =
        source_loaded && fixed_available;
    int maximum_components =
        view != NULL && view->maximum_top_components > 0
            ? view->maximum_top_components
            : (fixed_global ? 1 : 1023);

    SpectrogramPageLayout page_layout =
        spectrogram_page_layout(workspace.x,
                                workspace.y,
                                workspace.width,
                                workspace.height);
    Rectangle inspector =
        raylib_rectangle(page_layout.inspector);
    Rectangle canvas =
        raylib_rectangle(page_layout.canvas);
    DrawRectangleRounded(
        inspector, 0.035f, 12, theme->panel);
    DrawRectangleRoundedLines(
        inspector, 0.035f, 12, theme->panel_border);
    DrawRectangleRounded(
        canvas, 0.025f, 12, theme->panel);
    DrawRectangleRoundedLines(
        canvas, 0.025f, 12, theme->panel_border);

    float content_x =
        page_layout.inspector_content.x;
    float content_width =
        page_layout.inspector_content.width;
    const char *state_label =
        !source_loaded ? "NO AUDIO"
                       : (processing
                              ? "PROCESSING"
                              : (ready ? "READY"
                                       : "UNAVAILABLE"));
    Color state_color =
        processing ? theme->accent
                   : (ready ? (Color){70, 190, 120, 255}
                            : (source_loaded
                                   ? (Color){229, 160, 62, 255}
                                   : theme->muted_text));
    float state_width =
        processing || (source_loaded && !ready)
            ? 110.0f
            : 76.0f;
    Rectangle state_bounds = {
        inspector.x + inspector.width - state_width -
            UI_SPACE_2,
        inspector.y + UI_SPACE_2,
        state_width,
        28.0f,
    };
    shell_draw_badge(
        theme, state_bounds, state_label, state_color);

    float file_width =
        state_bounds.x - UI_SPACE_1 - content_x;
    draw_fitted_text(
        theme,
        source_loaded && view->file_name != NULL
            ? view->file_name
            : "Reconstruction",
        content_x,
        inspector.y + UI_SPACE_2 + 2.0f,
        file_width,
        14.5f,
        11.0f,
        theme->text,
        true);
    if (source_loaded) {
        char duration_text[24];
        char metadata[96];
        format_compact_duration(view->duration_seconds,
                                duration_text,
                                sizeof(duration_text));
        const char *output_label =
            fixed_global
                ? (view->global_reconstruction_channels > 1U
                       ? "Stereo FFT output"
                       : "Mono FFT output")
                : (view->reconstruction_channels > 1U
                       ? "Stereo STFT output"
                       : "Mono STFT output");
        snprintf(metadata,
                 sizeof(metadata),
                 "%s  |  %.1f kHz  |  %s",
                 duration_text,
                 (float)view->sample_rate / 1000.0f,
                 output_label);
        draw_fitted_text(theme,
                         metadata,
                         content_x,
                         inspector.y + 46.0f,
                         content_width,
                         11.5f,
                         10.0f,
                         theme->muted_text,
                         false);
    } else {
        theme_draw_text(theme,
                        "Import audio to begin",
                        content_x,
                        inspector.y + 46.0f,
                        11.5f,
                        theme->muted_text);
    }

    theme_draw_text(theme,
                    "Model",
                    content_x,
                    inspector.y + 76.0f,
                    11.0f,
                    theme->muted_text);
    const char *mode_labels[] = {
        "Mono FFT", "Stereo FFT", "STFT"};
    int selected_mode =
        !fixed_global
            ? 2
            : (view != NULL &&
                       view->global_channel_mode ==
                           GLOBAL_FOURIER_CHANNEL_SOURCE &&
                       view->source_reconstruction_channels > 1U
                   ? 1
                   : 0);
    int pressed_mode = draw_segmented_control(
        theme,
        raylib_rectangle(page_layout.mode_control),
        mode_labels,
        3,
        selected_mode,
        source_loaded);
    if (pressed_mode >= 0 && pressed_mode != selected_mode) {
        actions.select_mode = true;
        actions.mode =
            pressed_mode == 2
                ? FULL_FILE_MODE_TIME_VARYING_STFT
                : FULL_FILE_MODE_FIXED_GLOBAL;
        if (pressed_mode < 2) {
            actions.select_global_channel_mode = true;
            actions.global_channel_mode =
                pressed_mode == 0
                    ? GLOBAL_FOURIER_CHANNEL_MONO
                    : GLOBAL_FOURIER_CHANNEL_SOURCE;
        }
    }

    Rectangle original_button =
        raylib_rectangle(page_layout.original_button);
    Rectangle reconstruction_button = raylib_rectangle(
        page_layout.reconstruction_button);
    Rectangle export_button =
        raylib_rectangle(page_layout.export_button);
    if (!source_loaded) {
        actions.open_analysis = draw_button(
            theme,
            raylib_rectangle(
                page_layout.choose_audio_button),
            "Choose audio",
            theme->accent);
    } else {
        actions.play_original = draw_button(
            theme,
            original_button,
            "Original",
            theme->accent);
        if (ready) {
            actions.play_reconstruction = draw_button(
                theme,
                reconstruction_button,
                "Reconstruction",
                theme->accent);
            actions.export_reconstruction = draw_button(
                theme,
                export_button,
                "Export WAV",
                theme->blue);
        } else {
            draw_disabled_button(
                theme,
                reconstruction_button,
                "Reconstruction");
            draw_disabled_button(
                theme, export_button, "Export WAV");
        }
    }

    TransportPlayerView empty_player = {0};
    TransportPlayerActions player_actions =
        draw_transport_player(
            theme,
            raylib_rectangle(page_layout.player),
            source_loaded ? &view->player : &empty_player);
    actions.toggle_play_pause =
        player_actions.toggle_play_pause;
    actions.stop = player_actions.stop;

    float preset_label_y;
    if (fixed_global) {
        theme_draw_text(theme,
                        "Keep by",
                        content_x,
                        inspector.y + 348.0f,
                        11.0f,
                        theme->muted_text);
        const char *basis_labels[] = {
            "FFT bins", "Spectral energy"};
        int selected_basis = energy_selection ? 1 : 0;
        int pressed_basis = draw_segmented_control(
            theme,
            raylib_rectangle(
                page_layout.fixed_basis_control),
            basis_labels,
            2,
            selected_basis,
            configuration_enabled);
        if (pressed_basis >= 0 &&
            pressed_basis != selected_basis) {
            actions.select_global_selection_mode = true;
            actions.global_selection_mode =
                pressed_basis == 0
                    ? GLOBAL_SELECTION_PADDED_FFT_BINS
                    : GLOBAL_SELECTION_SPECTRAL_ENERGY;
        }
        preset_label_y = inspector.y + 424.0f;
    } else {
        theme_draw_text(theme,
                        "Components per frame / channel",
                        content_x,
                        inspector.y + 356.0f,
                        11.0f,
                        theme->muted_text);
        preset_label_y = inspector.y + 376.0f;
    }
    theme_draw_text(theme,
                    "Presets",
                    content_x,
                    preset_label_y,
                    11.0f,
                    theme->muted_text);

    const int global_presets[] = {
        maximum_components < 5 ? maximum_components : 5,
        component_count_for_percent(maximum_components, 1),
        component_count_for_percent(maximum_components, 10),
        component_count_for_percent(maximum_components, 30),
        component_count_for_percent(maximum_components, 50),
        maximum_components,
    };
    const char *global_preset_labels[] = {
        "Top 5", "1%", "10%", "30%", "50%", "All"};
    const float energy_presets[] = {
        0.10f, 0.30f, 0.50f, 0.75f, 0.90f, 0.99f};
    const char *energy_preset_labels[] = {
        "10%", "30%", "50%", "75%", "90%", "99%"};
    const int adaptive_presets[] = {
        5, 10, 100, 500, 1000};
    const char *adaptive_preset_labels[] = {
        "5", "10", "100", "500", "1k"};
    const int *preset_values =
        fixed_global ? global_presets : adaptive_presets;
    int preset_count = fixed_global ? 6 : 5;
    const char *preset_labels[6] = {0};
    int selected_preset = -1;
    for (int index = 0; index < preset_count; index++) {
        const int value = preset_values[index];
        if (fixed_global && energy_selection) {
            preset_labels[index] =
                energy_preset_labels[index];
            if (view != NULL &&
                fabsf(view->selected_energy_target -
                       energy_presets[index]) < 0.00005f) {
                selected_preset = index;
            }
        } else if (fixed_global) {
            preset_labels[index] =
                global_preset_labels[index];
            if (view != NULL &&
                view->selected_top_components == value) {
                selected_preset = index;
            }
        } else {
            preset_labels[index] =
                adaptive_preset_labels[index];
            if (view != NULL &&
                view->selected_top_components == value) {
                selected_preset = index;
            }
        }
    }

    int pressed_preset = draw_segmented_control(
        theme,
        raylib_rectangle(
            fixed_global ? page_layout.fixed_presets
                         : page_layout.adaptive_presets),
        preset_labels,
        preset_count,
        selected_preset,
        configuration_enabled);
    if (pressed_preset >= 0 &&
        pressed_preset != selected_preset) {
        if (fixed_global && energy_selection) {
            actions.select_energy_target = true;
            actions.energy_target =
                energy_presets[pressed_preset];
        } else {
            actions.select_top_components = true;
            actions.top_component_count =
                preset_values[pressed_preset];
        }
    }

    SpectrogramLayoutRect custom_layout =
        fixed_global ? page_layout.fixed_custom_input
                     : page_layout.adaptive_custom_input;
    if (configuration_enabled) {
        if (fixed_global && energy_selection) {
            float energy_target = actions.energy_target;
            if (draw_energy_target_input(
                    theme,
                    raylib_rectangle(custom_layout),
                    view->selected_energy_target,
                    &energy_target)) {
                actions.select_energy_target = true;
                actions.energy_target = energy_target;
            }
        } else {
            int exact_value = actions.top_component_count;
            if (draw_component_count_input(
                    theme,
                    raylib_rectangle(custom_layout),
                    view->mode,
                    view->selected_top_components,
                    maximum_components,
                    &exact_value)) {
                actions.select_top_components = true;
                actions.top_component_count = exact_value;
            }
        }
    } else if (!source_loaded) {
        draw_disabled_button(theme,
                             raylib_rectangle(custom_layout),
                             "Choose audio to configure");
    } else {
        draw_disabled_button(theme,
                             raylib_rectangle(custom_layout),
                             view != NULL &&
                                     view->global_channel_mode ==
                                         GLOBAL_FOURIER_CHANNEL_SOURCE
                                 ? "Choose Mono FFT"
                                 : "Use STFT");
    }

    float status_y =
        inspector.y + inspector.height - 35.0f;
    bool show_error_status =
        source_loaded && !processing && !ready &&
        view != NULL && view->status != NULL;
    if (show_error_status) {
        DrawCircleSector(
            (Vector2){content_x + 4.0f, status_y + 10.0f},
            4.0f,
            0.0f,
            360.0f,
            32,
            (Color){229, 160, 62, 255});
        draw_fitted_text(theme,
                         view->status,
                         content_x + 16.0f,
                         status_y,
                         content_width - 16.0f,
                         11.5f,
                         10.0f,
                         theme->muted_text,
                         false);
    }
    if (processing) {
        float progress =
            fminf(fmaxf(view->progress, 0.0f), 1.0f);
        Rectangle progress_track = raylib_rectangle(
            page_layout.progress_track);
        DrawRectangleRec(
            progress_track, (Color){43, 43, 47, 255});
        DrawRectangleRec(
            (Rectangle){progress_track.x,
                        progress_track.y,
                        progress_track.width * progress,
                        progress_track.height},
            theme->accent);
    }

    theme_draw_heading(theme,
                       "Spectrogram",
                       page_layout.canvas_title.x,
                       page_layout.canvas_title.y,
                       17.0f,
                       theme->text);
    Rectangle legend =
        raylib_rectangle(page_layout.legend_bar);
    DrawRectangleGradientH(
        (int)legend.x,
        (int)legend.y,
        (int)legend.width,
        (int)legend.height,
        (Color){17, 18, 28, 255},
        (Color){244, 192, 72, 255});
    theme_draw_text(theme,
                    "-100 dB",
                    page_layout.legend_labels.x,
                    page_layout.legend_labels.y,
                    10.5f,
                    theme->muted_text);
    const char *zero_db = "0 dB";
    int zero_width =
        theme_measure_text(theme, zero_db, 10.5f);
    theme_draw_text(theme,
                    zero_db,
                    page_layout.legend_labels.x +
                        page_layout.legend_labels.width -
                        (float)zero_width,
                    page_layout.legend_labels.y,
                    10.5f,
                    theme->muted_text);

    char first_value[64] = "--";
    char second_value[64] = "--";
    char third_value[64] = "--";
    const char *first_label = "Selection";
    const char *second_label = "Resolved";
    const char *third_label = "Result";
    if (source_loaded && view != NULL) {
        int displayed_components =
            ready ? view->rendered_top_components
                  : view->selected_top_components;
        char component_text[32];
        format_grouped_count(
            displayed_components > 0
                ? (size_t)displayed_components
                : 0U,
            component_text,
            sizeof(component_text));
        if (fixed_global && !fixed_available) {
            snprintf(first_value,
                     sizeof(first_value),
                     "Unavailable");
            second_label = "Grid share";
            snprintf(second_value,
                     sizeof(second_value),
                     "--");
            third_label = "Energy";
            snprintf(third_value,
                     sizeof(third_value),
                     "--");
        } else if (fixed_global && energy_selection) {
            snprintf(first_value,
                     sizeof(first_value),
                     "%.2f%% energy",
                     view->selected_energy_target * 100.0f);
            snprintf(second_value,
                     sizeof(second_value),
                     view->global_reconstruction_channels > 1U
                         ? "%s bins / ch"
                         : "%s bins",
                     component_text);
            third_label =
                processing ? "Progress" : "Grid share";
            snprintf(
                third_value,
                sizeof(third_value),
                processing
                    ? "%.0f%%"
                    : "%.2f%% of padded grid",
                processing
                    ? view->progress * 100.0f
                    : (maximum_components > 0
                           ? (float)displayed_components *
                                 100.0f /
                                 (float)maximum_components
                           : 0.0f));
        } else if (fixed_global) {
            snprintf(first_value,
                     sizeof(first_value),
                     view->global_reconstruction_channels > 1U
                         ? "%s bins / ch"
                         : "%s bins",
                     component_text);
            second_label = "Grid share";
            snprintf(second_value,
                     sizeof(second_value),
                     "%.2f%%",
                     maximum_components > 0
                         ? (float)displayed_components * 100.0f /
                               (float)maximum_components
                         : 0.0f);
            third_label =
                processing ? "Progress" : "Energy";
            snprintf(third_value,
                     sizeof(third_value),
                     processing ? "%.0f%%" : "%.3f%%",
                     processing
                         ? view->progress * 100.0f
                         : view->retained_energy * 100.0f);
        } else {
            snprintf(first_value,
                     sizeof(first_value),
                     "Top %s / frame / ch",
                     component_text);
            second_label = "Frames";
            format_grouped_count(view->frame_count,
                                 second_value,
                                 sizeof(second_value));
            third_label =
                processing ? "Progress" : "Energy";
            snprintf(third_value,
                     sizeof(third_value),
                     processing ? "%.0f%%" : "%.1f%%",
                     processing
                         ? view->progress * 100.0f
                         : view->retained_energy * 100.0f);
        }
    } else {
        snprintf(first_value,
                 sizeof(first_value),
                 "No audio");
        second_label = "Model";
        snprintf(second_value,
                 sizeof(second_value),
                 "Not selected");
    }

    Rectangle stats =
        raylib_rectangle(page_layout.stats_content);
    DrawRectangleRounded(
        raylib_rectangle(page_layout.stats_panel),
        0.10f,
        10,
        (Color){29, 30, 34, 255});
    float stat_width = stats.width / 3.0f;
    draw_compact_stat(
        theme,
        (Rectangle){stats.x,
                    stats.y,
                    stat_width,
                    stats.height},
        first_label,
        first_value,
        true);
    draw_compact_stat(
        theme,
        (Rectangle){stats.x + stat_width,
                    stats.y,
                    stat_width,
                    stats.height},
        second_label,
        second_value,
        true);
    draw_compact_stat(
        theme,
        (Rectangle){stats.x + stat_width * 2.0f,
                    stats.y,
                    stat_width,
                    stats.height},
        third_label,
        third_value,
        false);

    Rectangle plot =
        raylib_rectangle(page_layout.plot);
    if (plot.height < 90.0f) plot.height = 90.0f;
    if (plot.width < 90.0f) plot.width = 90.0f;
    DrawRectangleRec(plot, (Color){17, 18, 28, 255});
    if (view != NULL && view->spectrogram_texture != NULL &&
        view->spectrogram_texture->id != 0U) {
        Texture2D texture = *view->spectrogram_texture;
        DrawTexturePro(texture,
                       (Rectangle){0.0f, 0.0f, (float)texture.width, (float)texture.height},
                       plot,
                       (Vector2){0.0f, 0.0f},
                       0.0f,
                       WHITE);
    } else {
        const char *message = source_loaded
                                  ? (processing ? "Building the selected reconstruction..."
                                                : "Spectrogram data is unavailable.")
                                  : "Import audio to see its time-frequency structure";
        int width = theme_measure_heading(theme, message, 16.0f);
        theme_draw_heading(theme,
                           message,
                           plot.x + (plot.width - (float)width) * 0.5f,
                           plot.y + plot.height * 0.5f - 10.0f,
                           16.0f,
                           theme->text);
    }

    for (int division = 1; division < 4; division++) {
        float x = plot.x + plot.width * (float)division / 4.0f;
        float y = plot.y + plot.height * (float)division / 4.0f;
        DrawLine((int)x, (int)plot.y, (int)x, (int)(plot.y + plot.height), (Color){235, 235, 240, 35});
        DrawLine((int)plot.x, (int)y, (int)(plot.x + plot.width), (int)y, (Color){235, 235, 240, 35});
    }
    DrawRectangleLinesEx(plot, 1.0f, (Color){82, 84, 92, 255});

    float maximum_frequency = view != NULL ? view->maximum_frequency : 0.0f;
    theme_draw_text(theme,
                    maximum_frequency > 0.0f ? TextFormat("%.0f Hz", maximum_frequency) : "-- Hz",
                    canvas.x + 12.0f,
                    plot.y - 3.0f,
                    11.0f,
                    theme->muted_text);
    theme_draw_text(theme,
                    "0 Hz",
                    canvas.x + 24.0f,
                    plot.y + plot.height - 12.0f,
                    11.0f,
                    theme->muted_text);
    theme_draw_text(theme, "0 s", plot.x, plot.y + plot.height + 11.0f, 11.0f, theme->muted_text);
    if (view != NULL && view->duration_seconds > 0.0f) {
        char duration[24];
        format_compact_duration(view->duration_seconds,
                                duration,
                                sizeof(duration));
        int duration_width =
            theme_measure_text(theme, duration, 11.0f);
        theme_draw_text(theme,
                        duration,
                        plot.x + plot.width - (float)duration_width,
                        plot.y + plot.height + 11.0f,
                        11.0f,
                        theme->muted_text);
    }

    return actions;
}

static void draw_setting_row(const AppTheme *theme, Rectangle bounds, const char *label, const char *description,
                             const char *value, bool available) {
    theme_draw_heading(theme, label, bounds.x, bounds.y, 14.0f, available ? theme->text : (Color){128, 128, 133, 255});
    float description_y = bounds.y + theme_scaled_size(theme, 14.0f) + 4.0f;
    if (description_y + theme_scaled_size(theme, 12.5f) <= bounds.y + bounds.height - 5.0f) {
        theme_draw_text(theme, description, bounds.x, description_y, 12.5f, theme->muted_text);
    }
    int width = theme_measure_heading(theme, value, 13.0f);
    theme_draw_heading(theme, value, bounds.x + bounds.width - (float)width, bounds.y + 2.0f, 13.0f,
                       available ? theme->text : (Color){118, 118, 124, 255});
    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1), (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height - 1), theme->panel_border);
}

static void draw_text_scale_row(AppTheme *theme, Rectangle bounds) {
    theme_draw_heading(theme, "Text size", bounds.x, bounds.y, 14.0f, theme->text);
    float description_y = bounds.y + theme_scaled_size(theme, 14.0f) + 4.0f;
    if (description_y + theme_scaled_size(theme, 12.5f) <= bounds.y + bounds.height - 5.0f) {
        theme_draw_text(theme, "100% is the readable desktop baseline", bounds.x, description_y, 12.5f,
                        theme->muted_text);
    }

    float controls_x = bounds.x + bounds.width - 196.0f;
    if (draw_button(theme, (Rectangle){controls_x, bounds.y + 1.0f, 48, 44}, "A-", theme->accent)) {
        theme->text_scale -= 0.05f;
        if (theme->text_scale < 0.90f) theme->text_scale = 0.90f;
    }

    char scale_text[16];
    snprintf(scale_text, sizeof(scale_text), "%d%%", (int)(theme->text_scale * 100.0f + 0.5f));
    if (draw_button(theme, (Rectangle){controls_x + 54.0f, bounds.y + 1.0f, 82, 44}, scale_text, theme->muted_text)) {
        theme->text_scale = 1.10f;
    }
    if (draw_button(theme, (Rectangle){controls_x + 142.0f, bounds.y + 1.0f, 54, 44}, "A+", theme->accent)) {
        theme->text_scale += 0.05f;
        if (theme->text_scale > 1.40f) theme->text_scale = 1.40f;
    }

    DrawLine((int)bounds.x, (int)(bounds.y + bounds.height - 1), (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height - 1), theme->panel_border);
}

static bool draw_fft_memory_limit_row(
    const AppTheme *theme,
    Rectangle bounds,
    size_t current_bytes,
    size_t *selected_bytes) {
    static const size_t limits[] = {
        (size_t)512U * 1024U * 1024U,
        (size_t)768U * 1024U * 1024U,
        (size_t)1024U * 1024U * 1024U,
        (size_t)1536U * 1024U * 1024U,
    };
    static const char *labels[] = {
        "512 MB", "768 MB", "1 GB", "1.5 GB"};
    int selected_index = 1;
    for (int index = 0; index < 4; index++) {
        if (current_bytes == limits[index]) {
            selected_index = index;
            break;
        }
    }

    theme_draw_heading(
        theme, "Whole-file FFT memory", bounds.x, bounds.y,
        14.0f, theme->text);
    float description_y =
        bounds.y + theme_scaled_size(theme, 14.0f) + 4.0f;
    if (description_y + theme_scaled_size(theme, 12.5f) <=
        bounds.y + bounds.height - 5.0f) {
        draw_fitted_text(
            theme,
            "Unlocks heavier stereo FFT models",
            bounds.x,
            description_y,
            bounds.width - 120.0f,
            12.5f,
            10.0f,
            theme->muted_text,
            false);
    }
    Rectangle value_button = {
        bounds.x + bounds.width - 104.0f,
        bounds.y + 1.0f,
        104.0f,
        44.0f,
    };
    bool changed = draw_button(
        theme,
        value_button,
        labels[selected_index],
        theme->accent);
    if (changed && selected_bytes != NULL) {
        int next_index = (selected_index + 1) % 4;
        *selected_bytes = limits[next_index];
    }
    DrawLine((int)bounds.x,
             (int)(bounds.y + bounds.height - 1),
             (int)(bounds.x + bounds.width),
             (int)(bounds.y + bounds.height - 1),
             theme->panel_border);
    return changed;
}

SettingsActions draw_settings_page(
    AppTheme *theme,
    Rectangle workspace,
    bool audio_ready,
    const SettingsView *view) {
    SettingsActions actions = {0};
    float card_width = (workspace.width - 18.0f) * 0.5f;
    float card_height = (workspace.height - 18.0f) * 0.5f;
    Rectangle general = {workspace.x, workspace.y, card_width, card_height};
    Rectangle audio = {general.x + general.width + 18.0f, workspace.y, card_width, card_height};
    Rectangle analysis = {workspace.x, workspace.y + card_height + 18.0f, card_width, card_height};
    Rectangle appearance = {audio.x, analysis.y, card_width, card_height};
    shell_draw_card(theme, general, "General", "Workspace and session behavior");
    shell_draw_card(theme, audio, "Audio engine", "Playback and rendering defaults");
    shell_draw_card(theme, analysis, "Analysis", "FFT, pitch, and peak detection defaults");
    shell_draw_card(theme, appearance, "Appearance", "Interface scale and visual behavior");

    float rows_y = general.y + 16.0f + theme_scaled_size(theme, 17.0f) + 5.0f +
                   theme_scaled_size(theme, 14.0f) + 18.0f;
    float card_body_height = general.y + general.height - rows_y - 12.0f;
    float top_row_height =
        fminf(68.0f, card_body_height / 4.0f);
    float bottom_row_height = fminf(72.0f, card_body_height / 4.0f);
    draw_setting_row(theme, (Rectangle){general.x + 20, rows_y, general.width - 40, top_row_height},
                     "Startup workspace", "Page shown when Spectra opens", "Overview", true);
    draw_setting_row(theme, (Rectangle){general.x + 20, rows_y + top_row_height, general.width - 40, top_row_height},
                     "DSP processing", "Whole-file transforms run away from the UI", "Background", true);
    draw_setting_row(theme, (Rectangle){general.x + 20, rows_y + top_row_height * 2.0f, general.width - 40, top_row_height},
                     "Reconstruction cache", "Reuses recently rendered component counts", "256 MB", true);
    size_t selected_limit =
        view != NULL ? view->global_memory_limit_bytes
                     : (size_t)768U * 1024U * 1024U;
    if (draw_fft_memory_limit_row(
            theme,
            (Rectangle){general.x + 20,
                        rows_y + top_row_height * 3.0f,
                        general.width - 40,
                        top_row_height},
            selected_limit,
            &selected_limit)) {
        actions.select_global_memory_limit = true;
        actions.global_memory_limit_bytes = selected_limit;
    }

    draw_setting_row(theme, (Rectangle){audio.x + 20, rows_y, audio.width - 40, top_row_height},
                     "Output device", "Raylib system playback destination", audio_ready ? "System default" : "Unavailable", true);
    draw_setting_row(theme, (Rectangle){audio.x + 20, rows_y + top_row_height, audio.width - 40, top_row_height},
                     "Sample rate", "Synthesis and analysis sample rate", "44,100 Hz", true);
    draw_setting_row(theme, (Rectangle){audio.x + 20, rows_y + top_row_height * 2.0f, audio.width - 40, top_row_height},
                     "Channel mode", "Preserves mono/stereo imported playback", "Source-aware", true);

    float bottom_rows_y = analysis.y + (rows_y - general.y);
    draw_setting_row(theme, (Rectangle){analysis.x + 20, bottom_rows_y, analysis.width - 40, bottom_row_height},
                     "FFT size", "Frequency resolution for spectrum analysis", "16,384", true);
    draw_setting_row(theme, (Rectangle){analysis.x + 20, bottom_rows_y + bottom_row_height, analysis.width - 40, bottom_row_height},
                     "Window function", "Reduces spectral leakage before FFT", "Hann", true);
    draw_setting_row(theme, (Rectangle){analysis.x + 20, bottom_rows_y + bottom_row_height * 2.0f, analysis.width - 40, bottom_row_height},
                     "Pitch search range", "Generated-tone estimator limits", "40 - 1,200 Hz", true);
    draw_setting_row(theme, (Rectangle){analysis.x + 20, bottom_rows_y + bottom_row_height * 3.0f, analysis.width - 40, bottom_row_height},
                     "Peak threshold", "Minimum level for detected peaks", "-55 dB", true);

    draw_setting_row(theme, (Rectangle){appearance.x + 20, bottom_rows_y, appearance.width - 40, bottom_row_height},
                     "Theme", "Professional neutral workspace", "Spectra Dark", true);
    draw_setting_row(theme, (Rectangle){appearance.x + 20, bottom_rows_y + bottom_row_height, appearance.width - 40, bottom_row_height},
                     "UI scaling", "Automatically fits the available window", "Automatic", true);
    draw_setting_row(theme, (Rectangle){appearance.x + 20, bottom_rows_y + bottom_row_height * 2.0f, appearance.width - 40, bottom_row_height},
                     "Full screen", "Toggle borderless full screen", "F11", true);
    draw_text_scale_row(theme, (Rectangle){appearance.x + 20, bottom_rows_y + bottom_row_height * 3.0f,
                                           appearance.width - 40, bottom_row_height});
    return actions;
}
