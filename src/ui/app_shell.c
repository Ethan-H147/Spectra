#include "ui/app_shell.h"

#include "ui/widgets.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define SIDEBAR_WIDTH 280.0f
#define TOPBAR_HEIGHT 88.0f
#define STATUSBAR_HEIGHT 40.0f
#define CONTENT_MARGIN 30.0f

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

void shell_draw_card(const AppTheme *theme, Rectangle bounds, const char *title, const char *subtitle) {
    DrawRectangleRounded(bounds, 0.035f, 8, theme->panel);
    DrawRectangleRoundedLines(bounds, 0.035f, 8, theme->panel_border);
    theme_draw_heading(theme, title, bounds.x + 18.0f, bounds.y + 16.0f, 17.0f, theme->text);
    float subtitle_y = bounds.y + 16.0f + theme_scaled_size(theme, 17.0f) + 5.0f;
    theme_draw_text(theme, subtitle, bounds.x + 18.0f, subtitle_y, 14.0f, theme->muted_text);
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
    DrawCircle(18, (int)(bounds.y + bounds.height * 0.5f), 4, state_color);
    float text_y = bounds.y + (bounds.height - theme_scaled_size(theme, 12.0f)) * 0.5f - 1.0f;
    theme_draw_text(theme, audio_ready ? "Audio device ready" : "Audio device unavailable", 30.0f,
                    text_y, 12.0f, theme->muted_text);
    const char *pipeline = "44.1 kHz  |  Mono  |  FFT 16384  |  Local processing";
    int width = theme_measure_text(theme, pipeline, 12.0f);
    float pipeline_x = window_width - (float)width - 18.0f;
    if (pipeline_x > 310.0f) {
        theme_draw_text(theme, pipeline, pipeline_x, text_y, 12.0f, theme->muted_text);
    }
}

AppShellFrame draw_app_shell(const AppTheme *theme, AppPage active_page, bool audio_ready) {
    float window_width = (float)GetScreenWidth();
    float window_height = (float)GetScreenHeight();
    AppShellFrame frame = {
        .page = active_page,
        .workspace = {SIDEBAR_WIDTH + CONTENT_MARGIN,
                      TOPBAR_HEIGHT + 24.0f,
                      window_width - SIDEBAR_WIDTH - CONTENT_MARGIN * 2.0f,
                      window_height - TOPBAR_HEIGHT - STATUSBAR_HEIGHT - 48.0f},
        .toggle_fullscreen = false,
    };

    DrawRectangle(0, 0, (int)SIDEBAR_WIDTH, (int)(window_height - STATUSBAR_HEIGHT), (Color){25, 25, 27, 255});
    DrawLine((int)SIDEBAR_WIDTH, 0, (int)SIDEBAR_WIDTH, (int)(window_height - STATUSBAR_HEIGHT), theme->panel_border);
    DrawRectangle((int)SIDEBAR_WIDTH, 0, (int)(window_width - SIDEBAR_WIDTH), (int)TOPBAR_HEIGHT,
                  (Color){29, 29, 31, 255});
    DrawLine((int)SIDEBAR_WIDTH, (int)TOPBAR_HEIGHT, (int)window_width, (int)TOPBAR_HEIGHT, theme->panel_border);

    draw_brand_mark((Rectangle){18, 17, 44, 44});
    theme_draw_heading(theme, "Spectra", 72.0f, 10.0f, 18.0f, theme->text);
    theme_draw_text(theme, "FOURIER AUDIO LAB", 72.0f, 51.0f, 9.5f, theme->muted_text);

    theme_draw_text(theme, "WORKSPACES", 18.0f, 101.0f, 10.0f, (Color){108, 108, 112, 255});
    for (size_t i = 0; i < sizeof(NAV_ITEMS) / sizeof(NAV_ITEMS[0]); i++) {
        Rectangle nav_bounds = {10.0f, 130.0f + (float)i * 60.0f, SIDEBAR_WIDTH - 20.0f, 54.0f};
        if (draw_nav_item(theme, nav_bounds, &NAV_ITEMS[i], active_page == NAV_ITEMS[i].page)) {
            frame.page = NAV_ITEMS[i].page;
        }
    }

    NavItem settings = {APP_PAGE_SETTINGS, "Settings", "", SHELL_ICON_SETTINGS};
    float settings_y = window_height - STATUSBAR_HEIGHT - 72.0f;
    if (draw_nav_item(theme, (Rectangle){10, settings_y, SIDEBAR_WIDTH - 20.0f, 56}, &settings,
                      active_page == APP_PAGE_SETTINGS)) {
        frame.page = APP_PAGE_SETTINGS;
    }

    float topbar_x = SIDEBAR_WIDTH + CONTENT_MARGIN;
    theme_draw_heading(theme, page_title(active_page), topbar_x, 8.0f, 18.0f, theme->text);
    theme_draw_text(theme, page_context(active_page), topbar_x, 51.0f, 11.5f, theme->muted_text);

    const char *fullscreen_label = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE) ? "Exit full screen  F11" : "Full screen  F11";
    if (draw_button(theme, (Rectangle){window_width - 230.0f, 17, 206, 50}, fullscreen_label, theme->accent)) {
        frame.toggle_fullscreen = true;
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
    shell_draw_badge(theme, (Rectangle){analyze_card.x + 18, action_y + 4.0f, 122, 28}, "MILESTONE 4",
                     (Color){229, 160, 62, 255});
    shell_draw_card(theme, reconstruct_card, compact_cards ? "Reconstruct audio" : "Reconstruct a sound",
                    compact_cards ? "Extract and compare harmonics" : "Extract harmonics and compare playback");
    shell_draw_badge(theme, (Rectangle){reconstruct_card.x + 18, action_y + 4.0f, 122, 28}, "MILESTONE 5",
                     (Color){229, 160, 62, 255});

    float signal_title_y = synth_card.y + synth_card.height + 24.0f;
    float step_y = signal_title_y + theme_scaled_size(theme, 19.0f) + 12.0f;
    float step_height = fminf(100.0f, fmaxf(86.0f, workspace.height * 0.12f));
    theme_draw_heading(theme, "Signal path", workspace.x, signal_title_y, 19.0f, theme->text);
    const char *titles[] = {"Source", "Synthesis", "Spectrum", "Pitch", "Harmonics", "Resynthesis"};
    const char *details[] = {"Generated tone", "ADSR + gain", "FFT + peaks", "Estimated f0", "Peak matching", "Additive model"};
    float step_width = (workspace.width - 50.0f) / 6.0f;
    for (int i = 0; i < 6; i++) {
        Rectangle step = {workspace.x + (step_width + 10.0f) * (float)i, step_y, step_width, step_height};
        draw_step(theme, step, TextFormat("%d", i + 1), titles[i], details[i], i < 4);
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
    theme_draw_text(theme, "Every planned workspace maps directly to a stage in the documented signal pipeline.",
                    scope.x + 22.0f, body_y + body_step, 14.0f, theme->muted_text);
    float badge_y = scope.y + scope.height - 38.0f;
    shell_draw_badge(theme, (Rectangle){scope.x + 22, badge_y, 160, 28}, "LOCAL PROCESSING", theme->accent);
    shell_draw_badge(theme, (Rectangle){scope.x + 192, badge_y, 132, 28}, "NO AI CLAIMS", (Color){157, 113, 224, 255});
    return requested_page;
}

static void draw_drop_zone(const AppTheme *theme, Rectangle bounds, const char *title, const char *subtitle) {
    DrawRectangleRounded(bounds, 0.025f, 8, (Color){27, 27, 30, 255});
    DrawRectangleRoundedLines(bounds, 0.025f, 8, (Color){72, 72, 78, 255});
    float icon_y = bounds.y + fminf(50.0f, bounds.height * 0.34f);
    DrawCircleLines((int)(bounds.x + bounds.width * 0.5f), (int)icon_y, 16.0f, theme->muted_text);
    DrawLine((int)(bounds.x + bounds.width * 0.5f), (int)icon_y - 10,
             (int)(bounds.x + bounds.width * 0.5f), (int)icon_y + 10, theme->muted_text);
    DrawLine((int)(bounds.x + bounds.width * 0.5f - 10), (int)icon_y,
             (int)(bounds.x + bounds.width * 0.5f + 10), (int)icon_y, theme->muted_text);
    int title_width = theme_measure_heading(theme, title, 17.0f);
    theme_draw_heading(theme, title, bounds.x + (bounds.width - (float)title_width) * 0.5f, icon_y + 27.0f, 17.0f,
                       theme->text);
    int sub_width = theme_measure_text(theme, subtitle, 13.0f);
    theme_draw_text(theme, subtitle, bounds.x + (bounds.width - (float)sub_width) * 0.5f, icon_y + 54.0f, 13.0f,
                    theme->muted_text);
}

void draw_analysis_page(const AppTheme *theme, Rectangle workspace) {
    float column_gap = 18.0f;
    float left_width = fminf(430.0f, fmaxf(360.0f, workspace.width * 0.30f));
    float source_height = fmaxf(210.0f, workspace.height * 0.34f);
    float transport_height = fmaxf(180.0f, workspace.height * 0.25f);
    float pitch_height = workspace.height - source_height - transport_height - column_gap * 2.0f;
    Rectangle source = {workspace.x, workspace.y, left_width, source_height};
    shell_draw_card(theme, source, "Source audio", "Import a local WAV, MP3, FLAC, or OGG file");
    draw_drop_zone(theme, (Rectangle){source.x + 18, source.y + 100, source.width - 36, source.height - 118},
                   "Drop audio here", "Decoder planned for Milestone 4");
    shell_draw_badge(theme, (Rectangle){source.x + source.width - 140, source.y + 14, 122, 28}, "MILESTONE 4",
                     (Color){229, 160, 62, 255});

    Rectangle transport = {workspace.x, source.y + source.height + column_gap, left_width, transport_height};
    shell_draw_card(theme, transport, "Transport", "Original sample playback and selection");
    float button_width = (transport.width - 56.0f) / 3.0f;
    draw_disabled_button(theme, (Rectangle){transport.x + 18, transport.y + 100, button_width, 44}, "Play");
    draw_disabled_button(theme, (Rectangle){transport.x + 28 + button_width, transport.y + 100, button_width, 44}, "Stop");
    draw_disabled_button(theme, (Rectangle){transport.x + 38 + button_width * 2.0f, transport.y + 100, button_width, 44}, "Set region");
    theme_draw_text(theme, "No sample loaded", transport.x + 18, transport.y + transport.height - 34, 13.0f,
                    theme->muted_text);

    Rectangle pitch = {workspace.x, transport.y + transport.height + column_gap, left_width, pitch_height};
    shell_draw_card(theme, pitch, "Estimated pitch", "Sample periodicity with confidence checks");
    theme_draw_heading(theme, "-- Hz", pitch.x + 20, pitch.y + 100, 30.0f, (Color){118, 118, 124, 255});
    theme_draw_text(theme, "Note  --", pitch.x + 20, pitch.y + 164, 14.0f, theme->muted_text);
    theme_draw_text(theme, "Confidence  --", pitch.x + 20, pitch.y + 198, 14.0f, theme->muted_text);

    float right_x = workspace.x + left_width + column_gap;
    float right_width = workspace.width - left_width - column_gap;
    float waveform_height = fmaxf(210.0f, workspace.height * 0.43f);
    Rectangle waveform = {right_x, workspace.y, right_width, waveform_height};
    shell_draw_card(theme, waveform, "Waveform", "Decoded mono samples and selected analysis region");
    DrawLine((int)waveform.x + 20, (int)(waveform.y + waveform.height * 0.60f),
             (int)(waveform.x + waveform.width - 20), (int)(waveform.y + waveform.height * 0.60f),
             (Color){72, 72, 78, 255});
    theme_draw_text(theme, "Waveform appears after import", waveform.x + 22, waveform.y + waveform.height - 36,
                    13.0f, theme->muted_text);

    Rectangle spectrum = {right_x, waveform.y + waveform.height + column_gap, right_width,
                          workspace.height - waveform.height - column_gap};
    shell_draw_card(theme, spectrum, "Magnitude spectrum", "Hann window  |  FFT  |  peak detection  |  20 Hz - 20 kHz");
    for (int i = 0; i < 5; i++) {
        float y = spectrum.y + 106.0f + (spectrum.height - 132.0f) * (float)i / 4.0f;
        DrawLine((int)spectrum.x + 20, (int)y, (int)(spectrum.x + spectrum.width - 20), (int)y,
                 (Color){52, 52, 56, 255});
    }
    shell_draw_badge(theme, (Rectangle){spectrum.x + spectrum.width - 140, spectrum.y + 14, 122, 28}, "MILESTONE 4",
                     (Color){229, 160, 62, 255});
}

void draw_harmonic_lab_page(const AppTheme *theme, Rectangle workspace) {
    float comparison_height = fminf(220.0f, fmaxf(180.0f, workspace.height * 0.26f));
    Rectangle comparison = {workspace.x, workspace.y, workspace.width, comparison_height};
    shell_draw_card(theme, comparison, "Original vs. reconstructed", "A/B playback, level matching, and waveform comparison");
    draw_disabled_button(theme, (Rectangle){comparison.x + 20, comparison.y + 104, 154, 44}, "Play Original");
    draw_disabled_button(theme, (Rectangle){comparison.x + 186, comparison.y + 104, 182, 44}, "Play Resynthesis");
    draw_disabled_button(theme, (Rectangle){comparison.x + 380, comparison.y + 104, 140, 44}, "Export WAV");
    shell_draw_badge(theme, (Rectangle){comparison.x + comparison.width - 140, comparison.y + 14, 122, 28}, "MILESTONE 5",
                     (Color){229, 160, 62, 255});

    float lower_y = workspace.y + comparison.height + 18.0f;
    float lower_height = workspace.height - comparison.height - 18.0f;
    float table_width = (workspace.width - 18.0f) * 0.64f;
    Rectangle table = {workspace.x, lower_y, table_width, lower_height};
    shell_draw_card(theme, table, "Extracted harmonics", "Detected partials normalized against the strongest component");
    const char *headers[] = {"#", "Expected", "Detected", "Amplitude", "Level"};
    float columns[] = {table.x + 22,
                       table.x + table.width * 0.12f,
                       table.x + table.width * 0.32f,
                       table.x + table.width * 0.53f,
                       table.x + table.width * 0.80f};
    for (int i = 0; i < 5; i++) theme_draw_heading(theme, headers[i], columns[i], table.y + 102, 13.0f, theme->muted_text);
    DrawLine((int)table.x + 18, (int)table.y + 136, (int)(table.x + table.width - 18), (int)table.y + 136,
             theme->panel_border);
    float row_step = fminf(46.0f, (table.height - 158.0f) / 8.0f);
    for (int row = 0; row < 8; row++) {
        float y = table.y + 148.0f + (float)row * row_step;
        theme_draw_text(theme, TextFormat("H%02d", row + 1), columns[0], y, 13.0f, (Color){118, 118, 124, 255});
        theme_draw_text(theme, "--", columns[1], y, 13.0f, (Color){118, 118, 124, 255});
        theme_draw_text(theme, "--", columns[2], y, 13.0f, (Color){118, 118, 124, 255});
        DrawRectangleRounded((Rectangle){columns[3], y + 3, table.width * 0.16f, 6}, 0.8f, 4, (Color){57, 57, 62, 255});
        theme_draw_text(theme, "-- dB", columns[4], y, 13.0f, (Color){118, 118, 124, 255});
    }

    Rectangle model = {table.x + table.width + 18.0f, lower_y, workspace.width - table.width - 18.0f, lower_height};
    shell_draw_card(theme, model, "Resynthesis model", "Controls for rebuilding the analyzed tone");
    const char *labels[] = {"Fundamental", "Harmonic count", "Tolerance", "Duration", "Envelope"};
    const char *values[] = {"-- Hz", "16", "35 cents", "Match source", "Extracted ADSR"};
    for (int i = 0; i < 5; i++) {
        float row_gap = fminf(68.0f, (model.height - 116.0f) / 5.0f);
        float y = model.y + 102.0f + (float)i * row_gap;
        theme_draw_text(theme, labels[i], model.x + 20, y, 13.0f, theme->muted_text);
        int width = theme_measure_heading(theme, values[i], 13.0f);
        theme_draw_heading(theme, values[i], model.x + model.width - (float)width - 20.0f, y, 13.0f, theme->text);
        DrawLine((int)model.x + 20, (int)y + 28, (int)(model.x + model.width - 20), (int)y + 28, theme->panel_border);
    }
}

void draw_spectrogram_page(const AppTheme *theme, Rectangle workspace) {
    Rectangle toolbar = {workspace.x, workspace.y, workspace.width, 100};
    shell_draw_card(theme, toolbar, "STFT controls", "Window size 2048  |  Hop 512  |  Hann  |  dB range -100 to 0");
    shell_draw_badge(theme, (Rectangle){toolbar.x + toolbar.width - 140, toolbar.y + 14, 122, 28}, "MILESTONE 6",
                     (Color){229, 160, 62, 255});

    Rectangle heatmap = {workspace.x, workspace.y + 118, workspace.width, workspace.height - 118.0f};
    shell_draw_card(theme, heatmap, "Time-frequency view", "Frequency (Hz) over time (seconds)");
    Rectangle plot = {heatmap.x + 72, heatmap.y + 104, heatmap.width - 102, heatmap.height - 150};
    DrawRectangleRec(plot, (Color){17, 18, 24, 255});
    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 54; col++) {
            float energy = (float)((row * 17 + col * 11 + row * col) % 100) / 100.0f;
            unsigned char r = (unsigned char)(25 + energy * 55.0f);
            unsigned char g = (unsigned char)(35 + energy * 85.0f);
            unsigned char b = (unsigned char)(65 + energy * 150.0f);
            float cell_w = plot.width / 54.0f;
            float cell_h = plot.height / 16.0f;
            DrawRectangleRec((Rectangle){plot.x + (float)col * cell_w, plot.y + (float)row * cell_h,
                                         cell_w + 0.5f, cell_h + 0.5f}, (Color){r, g, b, 150});
        }
    }
    DrawRectangleRec(plot, (Color){15, 15, 18, 185});
    const char *message = "Spectrogram preview - STFT engine not implemented yet";
    int width = theme_measure_heading(theme, message, 17.0f);
    theme_draw_heading(theme, message, plot.x + (plot.width - (float)width) * 0.5f,
                       plot.y + plot.height * 0.5f - 10.0f, 17.0f, theme->text);
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

void draw_settings_page(AppTheme *theme, Rectangle workspace, bool audio_ready) {
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
    float top_row_height = fminf(78.0f, card_body_height / 3.0f);
    float bottom_row_height = fminf(72.0f, card_body_height / 4.0f);
    draw_setting_row(theme, (Rectangle){general.x + 20, rows_y, general.width - 40, top_row_height},
                     "Startup workspace", "Page shown when Spectra opens", "Overview", true);
    draw_setting_row(theme, (Rectangle){general.x + 20, rows_y + top_row_height, general.width - 40, top_row_height},
                     "Restore last session", "Remember synth controls between launches", "Planned", false);
    draw_setting_row(theme, (Rectangle){general.x + 20, rows_y + top_row_height * 2.0f, general.width - 40, top_row_height},
                     "Autosave", "Store workspace changes locally", "Planned", false);

    draw_setting_row(theme, (Rectangle){audio.x + 20, rows_y, audio.width - 40, top_row_height},
                     "Output device", "Raylib system playback destination", audio_ready ? "System default" : "Unavailable", true);
    draw_setting_row(theme, (Rectangle){audio.x + 20, rows_y + top_row_height, audio.width - 40, top_row_height},
                     "Sample rate", "Synthesis and analysis sample rate", "44,100 Hz", true);
    draw_setting_row(theme, (Rectangle){audio.x + 20, rows_y + top_row_height * 2.0f, audio.width - 40, top_row_height},
                     "Channel mode", "Generated and analyzed signal layout", "Mono", true);

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
}
