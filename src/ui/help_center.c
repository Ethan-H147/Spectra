#include "ui/help_center.h"

#include "ui/widgets.h"

#include <math.h>
#include <stddef.h>

#define HELP_PAD 16.0f
#define HELP_GAP 16.0f
#define HELP_HEADER_HEIGHT 104.0f

typedef struct {
    const char *title;
    const char *description;
} HelpItem;

typedef struct {
    AppPage page;
    const char *title;
    const char *navigation_label;
    const char *summary;
    HelpItem steps[3];
    HelpItem features[5];
    const char *tip;
} HelpGuide;

static const HelpGuide HELP_GUIDES[] = {
    {
        APP_PAGE_OVERVIEW,
        "Overview",
        "Start here",
        "Choose a workflow and follow sound through Spectra's signal path.",
        {
            {"Choose a workflow", "Create a tone, analyze audio, or continue into reconstruction."},
            {"Use the sidebar", "Move between workspaces with the navigation or number keys 1-5."},
            {"Follow the pipeline", "The numbered stages show how each workspace relates to the DSP process."},
        },
        {
            {"Workflow cards", "Open the Synthesizer, Audio Analysis, or Harmonic Lab directly."},
            {"Signal path", "See source, synthesis, spectrum, pitch, harmonics, and resynthesis in order."},
            {"Workspace shortcuts", "Press 1-5 to switch workspaces without reaching for the mouse."},
            {"Status bar", "Check audio-device state and the active processing configuration."},
            {"Local processing", "Imported audio and analysis remain on this computer."},
        },
        "For a recorded song or voice, begin in Audio Analysis. For a generated tone, begin in Synthesizer.",
    },
    {
        APP_PAGE_SYNTH,
        "Harmonic Synthesizer",
        "Create tones",
        "Build a tone from a fundamental frequency and sixteen adjustable harmonics.",
        {
            {"Load a starting shape", "Choose a sine, square-like, saw-like, clarinet, or string preset."},
            {"Set the tone", "Adjust frequency, duration, and master gain before listening."},
            {"Shape and compare", "Move H1-H16, then use the waveform, spectrum, and pitch readouts."},
        },
        {
            {"Player and export", "Play, pause, stop, follow progress, or choose where to save a WAV."},
            {"Fundamental", "Sets the base pitch. Harmonic frequencies are multiples of this value."},
            {"Presets", "Load useful amplitude patterns without locking any control."},
            {"H1-H16", "Control the relative strength of each harmonic partial."},
            {"Visual analysis", "Waveform shows shape; spectrum and pitch show frequency structure."},
        },
        "Press Space to replay the synthesized tone while refining the harmonic balance.",
    },
    {
        APP_PAGE_ANALYSIS,
        "Audio Analysis",
        "Inspect audio",
        "Import audio, isolate a useful region, and inspect its spectrum and pitch.",
        {
            {"Import audio", "Click the drop zone or drag in WAV, MP3, OGG, or FLAC audio."},
            {"Select a region", "Set the start and length around a steady note or sound."},
            {"Analyze", "Run the FFT, then inspect detected peaks and the estimated pitch."},
        },
        {
            {"Source audio", "Shows the imported file, duration, sample rate, and channel conversion."},
            {"Transport", "Choose full or region playback, then pause, resume, stop, or follow progress."},
            {"Region controls", "Choose the exact time range used for FFT and pitch analysis."},
            {"Waveform", "Shows amplitude over time and outlines the active analysis region."},
            {"Spectrum and pitch", "Displays strong frequency peaks and the best fundamental estimate."},
        },
        "A steady, nearly single-pitch region produces a clearer harmonic model than a busy musical passage.",
    },
    {
        APP_PAGE_HARMONIC_LAB,
        "Harmonic Lab",
        "Reconstruct sound",
        "Compare a pitched harmonic model with a phase-preserving Fourier frame.",
        {
            {"Analyze first", "Create a stable analyzed region in Audio Analysis."},
            {"Compare models", "Listen to the original, harmonic model, and Top-N Fourier frame."},
            {"Choose detail", "Increase strongest components to trade simplicity for fidelity."},
        },
        {
            {"Original vs harmonic", "A/B either source with shared pause, time, progress, and stop controls."},
            {"Extracted harmonics", "See expected multiples, matched peaks, amplitudes, and levels."},
            {"Harmonic model", "Summarizes the fundamental, detected partials, tolerance, and region."},
            {"Top-N Fourier frame", "Keeps the strongest bins with their original phase."},
            {"Export controls", "Save the harmonic result or the reconstructed Fourier frame as WAV."},
        },
        "The harmonic model explains pitched structure; the Fourier frame preserves more local detail.",
    },
    {
        APP_PAGE_SPECTROGRAM,
        "Spectrogram",
        "Explore over time",
        "Compare a sparse whole-file FFT-bin model with a more recognizable time-varying STFT reconstruction.",
        {
            {"Import a file", "Spectra preserves mono or stereo for reconstruction and uses a mono reference for the visualization."},
            {"Choose a method", "Mono FFT is the memory-safe fixed model, Stereo FFT preserves left/right, and STFT chooses a new set per frame."},
            {"Set, compare, export", "Choose a padded-bin or spectral-energy budget, A/B the result, then save it as WAV."},
        },
        {
            {"Mono vs stereo FFT", "Long stereo files automatically start in Mono FFT. Stereo FFT is available when its two full-file models fit the memory limit."},
            {"FFT-bin budget", "Counts use the one-sided, zero-padded FFT grid and are per channel only in Stereo FFT. Zero-padding does not add source detail."},
            {"Energy budget", "Choose a retained-spectral-energy target. Spectra uses the smallest strongest-bin set that reaches it and reports the resolved bin count."},
            {"Time-varying STFT", "Top-N is selected again in each overlapping frame, preserving speech and musical motion."},
            {"Background + cache", "DSP runs away from the UI, and recent component counts reopen instantly from a bounded cache."},
        },
        "Use Mono FFT for the dramatic build-up effect on long files, Stereo FFT when channel separation matters, and STFT for recognizable audio with fewer components.",
    },
    {
        APP_PAGE_SETTINGS,
        "Settings",
        "Tune the app",
        "Review engine defaults and adjust Spectra for your screen and workflow.",
        {
            {"Set readability", "Use A- and A+ to choose a comfortable text scale."},
            {"Check the engine", "Confirm output device, sample rate, channel mode, and FFT defaults."},
            {"Use shortcuts", "F1 opens this guide and F11 toggles borderless full screen."},
        },
        {
            {"General", "Shows startup behavior, background DSP, and the reconstruction cache."},
            {"FFT memory", "Click the memory value to cycle from 512 MB to 1.5 GB. Higher limits can unlock long Stereo FFT models but use more RAM."},
            {"Audio engine", "Reports the active playback destination and synthesis format."},
            {"Analysis", "Documents FFT size, Hann window, pitch range, and peak threshold."},
            {"Appearance", "Controls text size and records the active display behavior."},
        },
        "The interface is designed around 110% text by default; increase it without losing the responsive layout.",
    },
};

static float help_fitted_size(const AppTheme *theme,
                              const char *text,
                              float preferred,
                              float minimum,
                              float maximum_width,
                              bool heading) {
    float size = preferred;
    while (size > minimum) {
        int width = heading ? theme_measure_heading(theme, text, size)
                            : theme_measure_text(theme, text, size);
        if ((float)width <= maximum_width) {
            break;
        }
        size -= 0.5f;
    }
    return fmaxf(size, minimum);
}

static void help_draw_fitted(const AppTheme *theme,
                             const char *text,
                             float x,
                             float y,
                             float maximum_width,
                             float preferred,
                             float minimum,
                             Color color,
                             bool heading) {
    if (text == NULL || text[0] == '\0' || maximum_width <= 0.0f) {
        return;
    }

    float size =
        help_fitted_size(theme, text, preferred, minimum, maximum_width, heading);
    float height = theme_scaled_size(theme, size) + 4.0f;
    BeginScissorMode((int)x, (int)y, (int)maximum_width, (int)ceilf(height));
    if (heading) {
        theme_draw_heading(theme, text, x, y, size, color);
    } else {
        theme_draw_text(theme, text, x, y, size, color);
    }
    EndScissorMode();
}

static const HelpGuide *find_guide(AppPage page) {
    for (size_t index = 0; index < sizeof(HELP_GUIDES) / sizeof(HELP_GUIDES[0]); index++) {
        if (HELP_GUIDES[index].page == page) {
            return &HELP_GUIDES[index];
        }
    }
    return &HELP_GUIDES[0];
}

static bool draw_topic_button(const AppTheme *theme,
                              Rectangle bounds,
                              const HelpGuide *guide,
                              bool active) {
    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, bounds);
    bool pressed = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    Color fill = active ? (Color){42, 49, 61, 255}
                        : (hovered ? (Color){39, 40, 44, 255} : BLANK);

    if (fill.a > 0U) {
        DrawRectangleRounded(bounds, 0.08f, 8, fill);
    }
    if (active) {
        DrawRectangleRounded((Rectangle){bounds.x, bounds.y + 8.0f, 3.0f, bounds.height - 16.0f},
                             0.8f,
                             8,
                             theme->accent);
    }
    help_draw_fitted(theme,
                     guide->title,
                     bounds.x + 16.0f,
                     bounds.y + 7.0f,
                     bounds.width - 32.0f,
                     14.0f,
                     11.0f,
                     active ? theme->text : theme->muted_text,
                     true);
    help_draw_fitted(theme,
                     guide->navigation_label,
                     bounds.x + 16.0f,
                     bounds.y + 34.0f,
                     bounds.width - 32.0f,
                     10.5f,
                     9.0f,
                     active ? (Color){143, 188, 245, 255} : (Color){124, 124, 130, 255},
                     false);
    return pressed;
}

static void draw_step_number(const AppTheme *theme, Vector2 center, int number) {
    DrawCircleSector(center, 14.0f, 0.0f, 360.0f, 48, (Color){38, 128, 235, 255});
    const char *number_text = TextFormat("%d", number);
    int width = theme_measure_heading(theme, number_text, 11.0f);
    float y = center.y - theme_scaled_size(theme, 11.0f) * 0.5f - 1.0f;
    theme_draw_heading(theme, number_text, center.x - (float)width * 0.5f, y, 11.0f, WHITE);
}

static void draw_quick_step(const AppTheme *theme,
                            Rectangle bounds,
                            const HelpItem *step,
                            int number,
                            bool horizontal) {
    DrawRectangleRounded(bounds, 0.055f, 8, (Color){29, 30, 33, 255});
    DrawRectangleRoundedLines(bounds, 0.055f, 8, (Color){52, 55, 61, 255});
    draw_step_number(theme,
                     horizontal ? (Vector2){bounds.x + 24.0f, bounds.y + 24.0f}
                                : (Vector2){bounds.x + 22.0f, bounds.y + bounds.height * 0.5f},
                     number);

    float text_x = horizontal ? bounds.x + 48.0f : bounds.x + 48.0f;
    float title_y = horizontal ? bounds.y + 10.0f : bounds.y + 7.0f;
    float text_width = bounds.x + bounds.width - 14.0f - text_x;
    help_draw_fitted(theme,
                     step->title,
                     text_x,
                     title_y,
                     text_width,
                     12.5f,
                     10.0f,
                     theme->text,
                     true);

    float description_x = horizontal ? bounds.x + 14.0f : text_x;
    float description_y = horizontal ? bounds.y + 54.0f : bounds.y + 34.0f;
    float description_width =
        horizontal ? bounds.width - 28.0f : bounds.x + bounds.width - 14.0f - description_x;
    help_draw_fitted(theme,
                     step->description,
                     description_x,
                     description_y,
                     description_width,
                     10.5f,
                     8.5f,
                     theme->muted_text,
                     false);
}

static void draw_feature(const AppTheme *theme,
                         Rectangle bounds,
                         const HelpItem *feature,
                         bool card_style) {
    if (card_style) {
        DrawRectangleRounded(bounds, 0.045f, 8, (Color){30, 31, 34, 255});
        DrawRectangleRoundedLines(bounds, 0.045f, 8, (Color){50, 52, 58, 255});
    } else {
        DrawLine((int)bounds.x,
                 (int)(bounds.y + bounds.height - 1.0f),
                 (int)(bounds.x + bounds.width),
                 (int)(bounds.y + bounds.height - 1.0f),
                 (Color){49, 51, 56, 255});
    }

    float inset = card_style ? 14.0f : 0.0f;
    float label_width = card_style ? bounds.width * 0.34f : fminf(176.0f, bounds.width * 0.34f);
    float y = bounds.y + (bounds.height - theme_scaled_size(theme, 11.5f)) * 0.5f - 1.0f;
    help_draw_fitted(theme,
                     feature->title,
                     bounds.x + inset,
                     y,
                     label_width - inset,
                     11.5f,
                     9.5f,
                     theme->text,
                     true);
    float description_x = bounds.x + label_width + 12.0f;
    help_draw_fitted(theme,
                     feature->description,
                     description_x,
                     y,
                     bounds.x + bounds.width - inset - description_x,
                     10.5f,
                     8.5f,
                     theme->muted_text,
                     false);
}

HelpCenterActions draw_help_center(const AppTheme *theme,
                                   Rectangle workspace,
                                   AppPage selected_topic) {
    HelpCenterActions actions = {
        .topic = selected_topic,
        .close = false,
    };
    const HelpGuide *guide = find_guide(selected_topic);

    float navigation_width =
        fminf(272.0f, fmaxf(232.0f, workspace.width * 0.22f));
    Rectangle navigation = {
        workspace.x,
        workspace.y,
        navigation_width,
        workspace.height,
    };
    Rectangle content = {
        navigation.x + navigation.width + HELP_GAP,
        workspace.y,
        workspace.width - navigation.width - HELP_GAP,
        workspace.height,
    };

    shell_draw_card(theme, navigation, "Help Center", "Guides for every workspace");
    float topic_y = navigation.y + HELP_HEADER_HEIGHT;
    float topic_height = 56.0f;
    float topic_gap = 8.0f;
    for (size_t index = 0; index < sizeof(HELP_GUIDES) / sizeof(HELP_GUIDES[0]); index++) {
        Rectangle topic_bounds = {
            navigation.x + HELP_PAD,
            topic_y + (topic_height + topic_gap) * (float)index,
            navigation.width - HELP_PAD * 2.0f,
            topic_height,
        };
        if (draw_topic_button(theme,
                              topic_bounds,
                              &HELP_GUIDES[index],
                              HELP_GUIDES[index].page == selected_topic)) {
            actions.topic = HELP_GUIDES[index].page;
            guide = &HELP_GUIDES[index];
        }
    }

    const char *shortcut = "F1  Help   |   Esc  Close";
    help_draw_fitted(theme,
                     shortcut,
                     navigation.x + HELP_PAD,
                     navigation.y + navigation.height - 36.0f,
                     navigation.width - HELP_PAD * 2.0f,
                     10.0f,
                     8.5f,
                     (Color){122, 124, 132, 255},
                     false);

    DrawRectangleRounded(content, 0.025f, 8, theme->panel);
    DrawRectangleRoundedLines(content, 0.025f, 8, theme->panel_border);
    help_draw_fitted(theme,
                     guide->title,
                     content.x + HELP_PAD,
                     content.y + HELP_PAD,
                     content.width - 176.0f,
                     17.0f,
                     12.0f,
                     theme->text,
                     true);
    help_draw_fitted(theme,
                     guide->summary,
                     content.x + HELP_PAD,
                     content.y + HELP_PAD + theme_scaled_size(theme, 17.0f) + 8.0f,
                     content.width - HELP_PAD * 2.0f,
                     13.5f,
                     9.5f,
                     theme->muted_text,
                     false);
    actions.close = draw_button(theme,
                                (Rectangle){content.x + content.width - 136.0f,
                                            content.y + HELP_PAD,
                                            120.0f,
                                            40.0f},
                                "Close  Esc",
                                theme->muted_text);

    float body_x = content.x + HELP_PAD;
    float body_width = content.width - HELP_PAD * 2.0f;
    float body_y = content.y + HELP_HEADER_HEIGHT;
    theme_draw_heading(theme, "QUICK START", body_x, body_y, 10.5f, (Color){125, 146, 174, 255});

    bool wide = body_width >= 1300.0f;
    float steps_y = body_y + 28.0f;
    float steps_height = wide ? 100.0f : 174.0f;
    if (wide) {
        float step_width = (body_width - HELP_GAP * 2.0f) / 3.0f;
        for (int index = 0; index < 3; index++) {
            draw_quick_step(theme,
                            (Rectangle){body_x + (step_width + HELP_GAP) * (float)index,
                                        steps_y,
                                        step_width,
                                        steps_height},
                            &guide->steps[index],
                            index + 1,
                            true);
        }
    } else {
        float step_height = (steps_height - HELP_GAP) / 3.0f;
        for (int index = 0; index < 3; index++) {
            draw_quick_step(theme,
                            (Rectangle){body_x,
                                        steps_y + (step_height + HELP_GAP * 0.5f) * (float)index,
                                        body_width,
                                        step_height},
                            &guide->steps[index],
                            index + 1,
                            false);
        }
    }

    float features_title_y = steps_y + steps_height + 20.0f;
    theme_draw_heading(
        theme, "FEATURES IN THIS WORKSPACE", body_x, features_title_y, 10.5f, (Color){125, 146, 174, 255});
    float features_y = features_title_y + 28.0f;
    float content_bottom = content.y + content.height - HELP_PAD;
    float remaining_height = content_bottom - features_y;
    float features_bottom = features_y;

    if (wide) {
        float feature_width = (body_width - HELP_GAP) * 0.5f;
        float feature_height = fminf(76.0f, fmaxf(64.0f, (remaining_height - HELP_GAP * 2.0f - 64.0f) / 3.0f));
        for (int index = 0; index < 5; index++) {
            int column = index % 2;
            int row = index / 2;
            Rectangle feature_bounds = {
                body_x + (feature_width + HELP_GAP) * (float)column,
                features_y + (feature_height + HELP_GAP) * (float)row,
                feature_width,
                feature_height,
            };
            draw_feature(theme, feature_bounds, &guide->features[index], true);
            if (feature_bounds.y + feature_bounds.height > features_bottom) {
                features_bottom = feature_bounds.y + feature_bounds.height;
            }
        }
    } else {
        float feature_height = fminf(64.0f, fmaxf(42.0f, remaining_height / 5.0f));
        for (int index = 0; index < 5; index++) {
            Rectangle feature_bounds = {
                body_x,
                features_y + feature_height * (float)index,
                body_width,
                feature_height,
            };
            draw_feature(theme, feature_bounds, &guide->features[index], false);
            features_bottom = feature_bounds.y + feature_bounds.height;
        }
    }

    float tip_y = features_bottom + HELP_GAP;
    if (content_bottom - tip_y >= 52.0f) {
        Rectangle tip = {body_x, tip_y, body_width, content_bottom - tip_y};
        DrawRectangleRounded(tip, 0.04f, 8, (Color){31, 39, 50, 255});
        DrawRectangleRoundedLines(tip, 0.04f, 8, (Color){49, 79, 116, 255});
        theme_draw_heading(theme, "TIP", tip.x + 14.0f, tip.y + 12.0f, 10.0f, (Color){125, 178, 242, 255});
        help_draw_fitted(theme,
                         guide->tip,
                         tip.x + 58.0f,
                         tip.y + 11.0f,
                         tip.width - 72.0f,
                         10.5f,
                         8.5f,
                         theme->muted_text,
                         false);
    }

    return actions;
}
