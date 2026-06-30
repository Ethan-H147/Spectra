#include "ui/theme.h"

#include <stddef.h>

static const char *FONT_CANDIDATES[] = {
    "assets/fonts/Inter-Regular.ttf",
    "assets/fonts/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
};

void theme_init(AppTheme *theme) {
    if (theme == NULL) {
        return;
    }

    *theme = (AppTheme){
        .font = GetFontDefault(),
        .owns_font = false,
        .background = {244, 247, 248, 255},
        .panel = {250, 252, 252, 255},
        .panel_border = {204, 214, 218, 255},
        .text = {24, 31, 34, 255},
        .muted_text = {86, 98, 102, 255},
        .accent = {15, 118, 110, 255},
        .blue = {37, 99, 235, 255},
        .danger = {180, 35, 24, 255},
    };

    for (size_t i = 0; i < sizeof(FONT_CANDIDATES) / sizeof(FONT_CANDIDATES[0]); i++) {
        if (!FileExists(FONT_CANDIDATES[i])) {
            continue;
        }

        Font loaded = LoadFontEx(FONT_CANDIDATES[i], 32, NULL, 0);
        if (loaded.texture.id > 0) {
            SetTextureFilter(loaded.texture, TEXTURE_FILTER_BILINEAR);
            theme->font = loaded;
            theme->owns_font = true;
            break;
        }
    }
}

void theme_unload(AppTheme *theme) {
    if (theme == NULL || !theme->owns_font) {
        return;
    }

    UnloadFont(theme->font);
    theme->owns_font = false;
}

void theme_draw_text(const AppTheme *theme, const char *text, float x, float y, float size, Color color) {
    if (theme == NULL) {
        DrawText(text, (int)x, (int)y, (int)size, color);
        return;
    }

    DrawTextEx(theme->font, text, (Vector2){x, y}, size, size * 0.06f, color);
}

int theme_measure_text(const AppTheme *theme, const char *text, float size) {
    if (theme == NULL) {
        return MeasureText(text, (int)size);
    }

    Vector2 measured = MeasureTextEx(theme->font, text, size, size * 0.06f);
    return (int)measured.x;
}
