#include "ui/widgets.h"

#include "dsp/signal_utils.h"

#include <stdio.h>

bool draw_button(const AppTheme *theme, Rectangle bounds, const char *text, Color accent) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color fill = hovered ? (Color){49, 55, 64, 255} : (Color){43, 43, 47, 255};
    Color border = hovered ? accent : theme->panel_border;

    DrawRectangleRounded(bounds, 0.14f, 8, fill);
    DrawRectangleRoundedLines(bounds, 0.14f, 8, border);

    float font_size = 14.5f;
    while (font_size > 11.5f && theme_measure_heading(theme, text, font_size) > (int)(bounds.width - 14.0f)) {
        font_size -= 0.5f;
    }
    float rendered_size = theme_scaled_size(theme, font_size);
    int text_width = theme_measure_heading(theme, text, font_size);
    theme_draw_heading(theme,
                       text,
                       bounds.x + bounds.width * 0.5f - (float)text_width * 0.5f,
                       bounds.y + (bounds.height - rendered_size) * 0.5f - 1.0f,
                       font_size,
                       hovered ? accent : theme->text);
    return pressed;
}

float draw_slider(const AppTheme *theme, Rectangle bounds, const char *label, float value, float min_value, float max_value) {
    Vector2 mouse = GetMousePosition();
    Rectangle track = {bounds.x, bounds.y + 27.0f, bounds.width, 6.0f};
    bool active = CheckCollisionPointRec(mouse, (Rectangle){bounds.x, bounds.y + 12.0f, bounds.width, 36.0f}) &&
                  IsMouseButtonDown(MOUSE_LEFT_BUTTON);

    if (active) {
        float t = clampf((mouse.x - track.x) / track.width, 0.0f, 1.0f);
        value = min_value + t * (max_value - min_value);
    }

    float normalized = (value - min_value) / (max_value - min_value);
    normalized = clampf(normalized, 0.0f, 1.0f);
    char value_text[64];
    float font_size = bounds.width < 130.0f ? 12.5f : 14.5f;
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
