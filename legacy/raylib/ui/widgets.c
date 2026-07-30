#include "ui/widgets.h"

#include "dsp/signal_utils.h"

#include <math.h>
#include <stdio.h>

static void draw_centered_button_label(const AppTheme *theme,
                                       Rectangle bounds,
                                       const char *text,
                                       Color color) {
    float font_size = 14.5f;
    while (font_size > 11.5f &&
           theme_measure_heading(theme, text, font_size) > (int)(bounds.width - 16.0f)) {
        font_size -= 0.5f;
    }
    float rendered_size = theme_scaled_size(theme, font_size);
    int text_width = theme_measure_heading(theme, text, font_size);
    theme_draw_heading(theme,
                       text,
                       bounds.x + bounds.width * 0.5f - (float)text_width * 0.5f,
                       bounds.y + (bounds.height - rendered_size) * 0.5f - 1.0f,
                       font_size,
                       color);
}

bool draw_button(const AppTheme *theme, Rectangle bounds, const char *text, Color accent) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool held = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    Color fill = held ? (Color){38, 42, 49, 255}
                      : (hovered ? (Color){49, 53, 61, 255} : (Color){42, 43, 47, 255});
    Color border = hovered ? (Color){accent.r, accent.g, accent.b, 210}
                           : (Color){58, 60, 66, 255};

    Rectangle visual_bounds = bounds;
    if (held) {
        visual_bounds.y += 1.0f;
    }
    DrawRectangleRounded(visual_bounds, 0.12f, 8, fill);
    DrawRectangleRoundedLines(visual_bounds, 0.12f, 8, border);
    draw_centered_button_label(theme, visual_bounds, text, theme->text);
    return pressed;
}

bool draw_choice_button(const AppTheme *theme,
                        Rectangle bounds,
                        const char *text,
                        bool selected) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool held = hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    Color fill = selected ? (Color){34, 85, 145, 255}
                          : (held ? (Color){38, 42, 49, 255}
                                  : (hovered ? (Color){49, 53, 61, 255}
                                             : (Color){40, 41, 45, 255}));
    Color border = selected ? theme->accent
                            : (hovered ? (Color){79, 83, 92, 255}
                                       : (Color){57, 59, 65, 255});

    Rectangle visual_bounds = bounds;
    if (held) {
        visual_bounds.y += 1.0f;
    }
    DrawRectangleRounded(visual_bounds, 0.12f, 8, fill);
    DrawRectangleRoundedLines(visual_bounds, 0.12f, 8, border);
    draw_centered_button_label(
        theme, visual_bounds, text, selected ? WHITE : theme->text);
    return pressed;
}

static void format_transport_time(float seconds, char *text, size_t text_size) {
    int total_seconds = (int)floorf(fmaxf(seconds, 0.0f));
    snprintf(text, text_size, "%d:%02d", total_seconds / 60, total_seconds % 60);
}

TransportPlayerActions draw_transport_player(const AppTheme *theme,
                                              Rectangle bounds,
                                              const TransportPlayerView *view) {
    TransportPlayerActions actions = {0};
    bool available = view != NULL && view->available;
    bool playing = available && view->playing;
    bool paused = available && view->paused;
    Vector2 mouse = GetMousePosition();

    Color fill = (Color){28, 29, 33, 255};
    Color border = available ? (Color){62, 65, 72, 255}
                             : (Color){49, 51, 56, 255};
    DrawRectangleRounded(bounds, 0.10f, 10, fill);
    DrawRectangleRoundedLines(bounds, 0.10f, 10, border);

    float control_size = fminf(40.0f, bounds.height - 16.0f);
    Rectangle play_bounds = {
        bounds.x + 12.0f,
        bounds.y + (bounds.height - control_size) * 0.5f,
        control_size,
        control_size,
    };
    bool play_hovered = available && CheckCollisionPointRec(mouse, play_bounds);
    bool play_held = play_hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    Color control_fill = !available
                             ? (Color){43, 44, 48, 255}
                             : (play_held ? (Color){31, 107, 187, 255}
                                          : (play_hovered ? (Color){48, 137, 230, 255}
                                                          : theme->accent));
    Vector2 play_center = {
        play_bounds.x + play_bounds.width * 0.5f,
        play_bounds.y + play_bounds.height * 0.5f,
    };
    DrawCircleSector(play_center, control_size * 0.5f, 0.0f, 360.0f, 48, control_fill);
    if (playing) {
        Color icon = available ? WHITE : (Color){116, 118, 124, 255};
        DrawRectangle((int)(play_center.x - 6.0f), (int)(play_center.y - 8.0f), 4, 16, icon);
        DrawRectangle((int)(play_center.x + 2.0f), (int)(play_center.y - 8.0f), 4, 16, icon);
    } else {
        Color icon = available ? WHITE : (Color){116, 118, 124, 255};
        DrawTriangle((Vector2){play_center.x - 5.0f, play_center.y - 8.0f},
                     (Vector2){play_center.x - 5.0f, play_center.y + 8.0f},
                     (Vector2){play_center.x + 8.0f, play_center.y},
                     icon);
    }
    if (available && play_hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        actions.toggle_play_pause = true;
    }

    Rectangle stop_bounds = {
        play_bounds.x + play_bounds.width + 8.0f,
        bounds.y + (bounds.height - 32.0f) * 0.5f,
        32.0f,
        32.0f,
    };
    bool stop_hovered = available && CheckCollisionPointRec(mouse, stop_bounds);
    bool stop_held = stop_hovered && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    Color stop_fill = stop_held ? (Color){62, 63, 69, 255}
                               : (stop_hovered ? (Color){50, 52, 58, 255}
                                               : (Color){38, 39, 43, 255});
    DrawRectangleRounded(stop_bounds, 0.18f, 8, stop_fill);
    DrawRectangleRoundedLines(
        stop_bounds, 0.18f, 8, stop_hovered ? theme->muted_text : border);
    Color stop_icon = available ? theme->text : (Color){106, 108, 114, 255};
    DrawRectangle((int)(stop_bounds.x + 11.0f),
                  (int)(stop_bounds.y + 11.0f),
                  10,
                  10,
                  stop_icon);
    if (available && stop_hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        actions.stop = true;
    }

    float content_x = stop_bounds.x + stop_bounds.width + 12.0f;
    float right_x = bounds.x + bounds.width - 14.0f;
    const char *title = available && view->title != NULL ? view->title : "Nothing selected";
    theme_draw_heading(theme,
                       title,
                       content_x,
                       bounds.y + 9.0f,
                       13.0f,
                       available ? theme->text : theme->muted_text);

    const char *state =
        !available ? "EMPTY" : (playing ? "PLAYING" : (paused ? "PAUSED" : "READY"));
    Color state_color = playing ? (Color){70, 190, 120, 255}
                                : (paused ? (Color){229, 160, 62, 255}
                                          : theme->muted_text);
    theme_draw_heading(theme,
                       state,
                       content_x,
                       bounds.y + 31.0f,
                       10.0f,
                       available ? state_color : (Color){102, 104, 110, 255});

    char position_text[20];
    char duration_text[20];
    format_transport_time(available ? view->position_seconds : 0.0f,
                          position_text,
                          sizeof(position_text));
    format_transport_time(available ? view->duration_seconds : 0.0f,
                          duration_text,
                          sizeof(duration_text));
    char time_text[48];
    snprintf(time_text, sizeof(time_text), "%s / %s", position_text, duration_text);
    int time_width = theme_measure_heading(theme, time_text, 11.5f);
    theme_draw_heading(theme,
                       time_text,
                       right_x - (float)time_width,
                       bounds.y + 31.0f,
                       11.5f,
                       available ? theme->text : theme->muted_text);

    Rectangle progress_track = {
        content_x,
        bounds.y + bounds.height - 14.0f,
        fmaxf(24.0f, right_x - content_x),
        5.0f,
    };
    float progress = 0.0f;
    if (available && view->duration_seconds > 0.0f) {
        progress = clampf(view->position_seconds / view->duration_seconds, 0.0f, 1.0f);
    }
    DrawRectangleRounded(progress_track, 0.8f, 8, (Color){57, 59, 65, 255});
    if (progress > 0.0f) {
        DrawRectangleRounded((Rectangle){progress_track.x,
                                         progress_track.y,
                                         progress_track.width * progress,
                                         progress_track.height},
                             0.8f,
                             8,
                             theme->accent);
    }
    return actions;
}

float draw_slider(const AppTheme *theme, Rectangle bounds, const char *label, float value, float min_value, float max_value) {
    Vector2 mouse = GetMousePosition();
    float font_size = bounds.width < 130.0f ? 12.5f : 14.5f;
    float rendered_size = theme_scaled_size(theme, font_size);
    Rectangle track = {bounds.x, bounds.y + rendered_size + 8.0f, bounds.width, 6.0f};
    Rectangle interaction = {
        bounds.x,
        bounds.y,
        bounds.width,
        track.y + track.height + 8.0f - bounds.y,
    };
    bool active = CheckCollisionPointRec(mouse, interaction) && IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    if (active) {
        float t = clampf((mouse.x - track.x) / track.width, 0.0f, 1.0f);
        value = min_value + t * (max_value - min_value);
    }

    float normalized = (value - min_value) / (max_value - min_value);
    normalized = clampf(normalized, 0.0f, 1.0f);
    char value_text[64];
    snprintf(value_text, sizeof(value_text), bounds.width < 130.0f ? "%.1f" : "%.2f", value);
    theme_draw_text(theme, label, bounds.x, bounds.y, font_size, theme->muted_text);
    theme_draw_text(theme,
                    value_text,
                    bounds.x + bounds.width - (float)theme_measure_text(theme, value_text, font_size),
                    bounds.y,
                    font_size,
                    theme->muted_text);
    DrawRectangleRounded(track, 0.8f, 8, (Color){65, 65, 70, 255});
    DrawRectangleRounded((Rectangle){track.x, track.y, track.width * normalized, track.height},
                         0.8f,
                         8,
                         theme->accent);
    DrawCircle((int)(track.x + track.width * normalized), (int)(track.y + track.height * 0.5f), 7, theme->accent);
    return value;
}

void draw_panel(const AppTheme *theme, Rectangle bounds, const char *title) {
    Rectangle shadow = {bounds.x, bounds.y + 2.0f, bounds.width, bounds.height};
    DrawRectangleRounded(shadow, 0.035f, 10, (Color){0, 0, 0, 45});
    DrawRectangleRounded(bounds, 0.035f, 10, theme->panel);
    DrawRectangleRoundedLines(bounds, 0.035f, 10, theme->panel_border);
    theme_draw_heading(theme, title, bounds.x + 20.0f, bounds.y + 15.0f, 21.0f, theme->text);
}
