#include "ui/theme.h"

#include <stddef.h>

#define BASE_TEXT_SCALE 1.85f

typedef struct {
    const char *regular;
    const char *bold;
} FontCandidate;

static const FontCandidate FONT_CANDIDATES[] = {
    {"assets/fonts/Inter-Regular.ttf", "assets/fonts/Inter-SemiBold.ttf"},
    {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/segoeuib.ttf"},
    {"assets/fonts/DejaVuSans.ttf", "assets/fonts/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
     "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"},
};

static float body_text_spacing(float size) {
    return size < 22.0f ? size * 0.018f : size * 0.006f;
}

static float heading_text_spacing(float size) {
    return size >= 30.0f ? size * -0.012f : 0.0f;
}

static Font load_font_candidate(const char *path) {
    Font font = {0};
    if (path == NULL || !FileExists(path)) {
        return font;
    }

    font = LoadFontEx(path, 64, NULL, 0);
    if (font.texture.id > 0) {
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    }
    return font;
}

void theme_init(AppTheme *theme) {
    if (theme == NULL) {
        return;
    }

    *theme = (AppTheme){
        .font = GetFontDefault(),
        .bold_font = GetFontDefault(),
        .owns_font = false,
        .owns_bold_font = false,
        .text_scale = 1.10f,
        .background = {30, 30, 32, 255},
        .panel = {36, 36, 39, 255},
        .panel_border = {57, 57, 62, 255},
        .text = {245, 245, 247, 255},
        .muted_text = {166, 166, 172, 255},
        .accent = {38, 128, 235, 255},
        .blue = {38, 128, 235, 255},
        .danger = {226, 88, 78, 255},
    };

    for (size_t i = 0; i < sizeof(FONT_CANDIDATES) / sizeof(FONT_CANDIDATES[0]); i++) {
        Font regular = load_font_candidate(FONT_CANDIDATES[i].regular);
        if (regular.texture.id == 0) {
            continue;
        }

        theme->font = regular;
        theme->owns_font = true;

        Font bold = load_font_candidate(FONT_CANDIDATES[i].bold);
        if (bold.texture.id > 0) {
            theme->bold_font = bold;
            theme->owns_bold_font = true;
        } else {
            theme->bold_font = regular;
        }
        break;
    }
}

void theme_unload(AppTheme *theme) {
    if (theme == NULL) {
        return;
    }

    if (theme->owns_bold_font) {
        UnloadFont(theme->bold_font);
        theme->owns_bold_font = false;
    }
    if (theme->owns_font) {
        UnloadFont(theme->font);
        theme->owns_font = false;
    }
}

void theme_draw_text(const AppTheme *theme, const char *text, float x, float y, float size, Color color) {
    if (theme == NULL) {
        DrawText(text, (int)x, (int)y, (int)size, color);
        return;
    }

    float scaled_size = theme_scaled_size(theme, size);
    DrawTextEx(theme->font, text, (Vector2){x, y}, scaled_size, body_text_spacing(scaled_size), color);
}

void theme_draw_heading(const AppTheme *theme, const char *text, float x, float y, float size, Color color) {
    if (theme == NULL) {
        DrawText(text, (int)x, (int)y, (int)size, color);
        return;
    }

    float scaled_size = theme_scaled_size(theme, size);
    DrawTextEx(theme->bold_font, text, (Vector2){x, y}, scaled_size, heading_text_spacing(scaled_size), color);
}

float theme_scaled_size(const AppTheme *theme, float size) {
    if (theme == NULL) {
        return size;
    }

    return size * BASE_TEXT_SCALE * theme->text_scale;
}

int theme_measure_text(const AppTheme *theme, const char *text, float size) {
    if (theme == NULL) {
        return MeasureText(text, (int)size);
    }

    float scaled_size = theme_scaled_size(theme, size);
    Vector2 measured = MeasureTextEx(theme->font, text, scaled_size, body_text_spacing(scaled_size));
    return (int)measured.x;
}

int theme_measure_heading(const AppTheme *theme, const char *text, float size) {
    if (theme == NULL) {
        return MeasureText(text, (int)size);
    }

    float scaled_size = theme_scaled_size(theme, size);
    Vector2 measured = MeasureTextEx(theme->bold_font, text, scaled_size, heading_text_spacing(scaled_size));
    return (int)measured.x;
}
