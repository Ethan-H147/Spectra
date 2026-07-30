#include "ui/theme.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#define BASE_TEXT_SCALE 1.85f
#define BODY_FONT_BASE_SIZE 32
#define HEADING_FONT_BASE_SIZE 32
#define DISPLAY_FONT_BASE_SIZE 64
#define DISPLAY_FONT_THRESHOLD 42.0f

typedef struct {
    const char *regular;
    const char *bold;
} FontCandidate;

static const FontCandidate FONT_CANDIDATES[] = {
    {"assets/fonts/Inter-Regular.ttf", "assets/fonts/Inter-SemiBold.ttf"},
    {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/seguisb.ttf"},
    {"assets/fonts/DejaVuSans.ttf", "assets/fonts/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"},
    {"/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
     "/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf"},
};

static float body_text_spacing(float size) {
    return size < 22.0f ? size * 0.012f : 0.0f;
}

static float heading_text_spacing(float size) {
    return size >= 30.0f ? size * -0.012f : 0.0f;
}

static float snap_text_pixel(float value) {
    return floorf(value + 0.5f);
}

static Font load_font_candidate(const char *path, int base_size) {
    Font font = {0};
    if (path == NULL || !FileExists(path)) {
        return font;
    }

    font = LoadFontEx(path, base_size, NULL, 0);
    if (font.texture.id > 0) {
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    }
    return font;
}

static bool text_needs_fallback(const char *text) {
    if (text == NULL) return false;
    for (const unsigned char *character =
             (const unsigned char *)text;
         *character != '\0';
         character++) {
        if (*character >= 0x80U) return true;
    }
    return false;
}

static Font theme_font_for_text(const AppTheme *theme,
                                const char *text,
                                bool heading,
                                float scaled_size) {
    if (theme != NULL && theme->owns_fallback_font &&
        text_needs_fallback(text)) {
        return theme->fallback_font;
    }
    if (!heading) {
        return theme->font;
    }
    return scaled_size >= DISPLAY_FONT_THRESHOLD
               ? theme->display_font
               : theme->bold_font;
}

static void unload_owned_fonts(AppTheme *theme) {
    if (theme == NULL) return;
    if (theme->owns_fallback_font) {
        UnloadFont(theme->fallback_font);
    }
    if (theme->owns_display_font) {
        UnloadFont(theme->display_font);
    }
    if (theme->owns_bold_font) {
        UnloadFont(theme->bold_font);
    }
    if (theme->owns_font) {
        UnloadFont(theme->font);
    }
    theme->font = GetFontDefault();
    theme->bold_font = GetFontDefault();
    theme->display_font = GetFontDefault();
    theme->fallback_font = GetFontDefault();
    theme->owns_font = false;
    theme->owns_bold_font = false;
    theme->owns_display_font = false;
    theme->owns_fallback_font = false;
}

void theme_init(AppTheme *theme) {
    if (theme == NULL) {
        return;
    }

    *theme = (AppTheme){
        .font = GetFontDefault(),
        .bold_font = GetFontDefault(),
        .display_font = GetFontDefault(),
        .fallback_font = GetFontDefault(),
        .owns_font = false,
        .owns_bold_font = false,
        .owns_display_font = false,
        .owns_fallback_font = false,
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
        Font regular = load_font_candidate(
            FONT_CANDIDATES[i].regular, BODY_FONT_BASE_SIZE);
        if (regular.texture.id == 0) {
            continue;
        }

        theme->font = regular;
        theme->owns_font = true;

        Font bold = load_font_candidate(
            FONT_CANDIDATES[i].bold, HEADING_FONT_BASE_SIZE);
        if (bold.texture.id > 0) {
            theme->bold_font = bold;
            theme->owns_bold_font = true;
        } else {
            theme->bold_font = regular;
        }

        Font display = load_font_candidate(
            FONT_CANDIDATES[i].bold, DISPLAY_FONT_BASE_SIZE);
        if (display.texture.id > 0) {
            theme->display_font = display;
            theme->owns_display_font = true;
        } else {
            theme->display_font = theme->bold_font;
        }
        break;
    }
}

bool theme_ensure_text_coverage(AppTheme *theme,
                                const char *utf8_text) {
    if (theme == NULL || utf8_text == NULL ||
        utf8_text[0] == '\0') {
        return false;
    }

    bool needs_extended_font = false;
    for (const unsigned char *character =
             (const unsigned char *)utf8_text;
         *character != '\0';
         character++) {
        if (*character >= 0x80U) {
            needs_extended_font = true;
            break;
        }
    }
    if (!needs_extended_font) return true;

    int dynamic_count = 0;
    int *dynamic_codepoints =
        LoadCodepoints(utf8_text, &dynamic_count);
    if (dynamic_codepoints == NULL || dynamic_count <= 0) {
        if (dynamic_codepoints != NULL) {
            UnloadCodepoints(dynamic_codepoints);
        }
        return false;
    }

    int capacity = 95 + dynamic_count;
    int *codepoints =
        (int *)calloc((size_t)capacity, sizeof(int));
    if (codepoints == NULL) {
        UnloadCodepoints(dynamic_codepoints);
        return false;
    }
    int count = 0;
    for (int codepoint = 32; codepoint <= 126; codepoint++) {
        codepoints[count++] = codepoint;
    }
    for (int index = 0; index < dynamic_count; index++) {
        int codepoint = dynamic_codepoints[index];
        bool duplicate = false;
        for (int existing = 0; existing < count; existing++) {
            if (codepoints[existing] == codepoint) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) codepoints[count++] = codepoint;
    }
    UnloadCodepoints(dynamic_codepoints);

    const char *fallback_candidates[] = {
        "C:/Windows/Fonts/Deng.ttf",
        "C:/Windows/Fonts/NotoSansSC-VF.ttf",
        "C:/Windows/Fonts/simhei.ttf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
    };
    Font fallback = {0};
    for (size_t index = 0;
         index < sizeof(fallback_candidates) /
                     sizeof(fallback_candidates[0]);
         index++) {
        const char *path = fallback_candidates[index];
        if (!FileExists(path)) continue;
        fallback = LoadFontEx(path, 64, codepoints, count);
        if (fallback.texture.id > 0U) break;
    }
    free(codepoints);
    if (fallback.texture.id == 0U) return false;
    SetTextureFilter(fallback.texture, TEXTURE_FILTER_BILINEAR);

    if (theme->owns_fallback_font) {
        UnloadFont(theme->fallback_font);
    }
    theme->fallback_font = fallback;
    theme->owns_fallback_font = true;
    return true;
}

void theme_unload(AppTheme *theme) {
    if (theme == NULL) {
        return;
    }

    unload_owned_fonts(theme);
}

void theme_draw_text(const AppTheme *theme, const char *text, float x, float y, float size, Color color) {
    if (theme == NULL) {
        DrawText(text, (int)x, (int)y, (int)size, color);
        return;
    }

    float scaled_size = theme_scaled_size(theme, size);
    Font font = theme_font_for_text(
        theme, text, false, scaled_size);
    DrawTextEx(font,
               text,
               (Vector2){snap_text_pixel(x),
                         snap_text_pixel(y)},
               scaled_size,
               body_text_spacing(scaled_size),
               color);
}

void theme_draw_heading(const AppTheme *theme, const char *text, float x, float y, float size, Color color) {
    if (theme == NULL) {
        DrawText(text, (int)x, (int)y, (int)size, color);
        return;
    }

    float scaled_size = theme_scaled_size(theme, size);
    Font font = theme_font_for_text(
        theme, text, true, scaled_size);
    DrawTextEx(font,
               text,
               (Vector2){snap_text_pixel(x),
                         snap_text_pixel(y)},
               scaled_size,
               heading_text_spacing(scaled_size),
               color);
}

float theme_scaled_size(const AppTheme *theme, float size) {
    if (theme == NULL) {
        return size;
    }

    return snap_text_pixel(
        size * BASE_TEXT_SCALE * theme->text_scale);
}

int theme_measure_text(const AppTheme *theme, const char *text, float size) {
    if (theme == NULL) {
        return MeasureText(text, (int)size);
    }

    float scaled_size = theme_scaled_size(theme, size);
    Font font = theme_font_for_text(
        theme, text, false, scaled_size);
    Vector2 measured = MeasureTextEx(font, text, scaled_size, body_text_spacing(scaled_size));
    return (int)measured.x;
}

int theme_measure_heading(const AppTheme *theme, const char *text, float size) {
    if (theme == NULL) {
        return MeasureText(text, (int)size);
    }

    float scaled_size = theme_scaled_size(theme, size);
    Font font = theme_font_for_text(
        theme, text, true, scaled_size);
    Vector2 measured = MeasureTextEx(font, text, scaled_size, heading_text_spacing(scaled_size));
    return (int)measured.x;
}
