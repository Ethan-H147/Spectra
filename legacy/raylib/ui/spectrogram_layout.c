#include "ui/spectrogram_layout.h"

#include <math.h>
#include <stdio.h>

#define LAYOUT_SPACE_1 8.0f
#define LAYOUT_SPACE_2 16.0f
#define LAYOUT_SPACE_4 32.0f

static float clampf_local(float value,
                          float minimum,
                          float maximum) {
    return fminf(fmaxf(value, minimum), maximum);
}

static bool rect_has_area(SpectrogramLayoutRect rect) {
    return rect.width > 0.0f && rect.height > 0.0f;
}

static bool rect_contains(SpectrogramLayoutRect outer,
                          SpectrogramLayoutRect inner) {
    const float epsilon = 0.01f;
    return inner.x + epsilon >= outer.x &&
           inner.y + epsilon >= outer.y &&
           inner.x + inner.width <=
               outer.x + outer.width + epsilon &&
           inner.y + inner.height <=
               outer.y + outer.height + epsilon;
}

static bool rects_overlap(SpectrogramLayoutRect first,
                          SpectrogramLayoutRect second) {
    return first.x < second.x + second.width &&
           first.x + first.width > second.x &&
           first.y < second.y + second.height &&
           first.y + first.height > second.y;
}

static bool fail_validation(char *message,
                            size_t message_size,
                            const char *reason) {
    if (message != NULL && message_size > 0U) {
        snprintf(message, message_size, "%s", reason);
    }
    return false;
}

SpectrogramPageLayout spectrogram_page_layout(float x,
                                              float y,
                                              float width,
                                              float height) {
    SpectrogramPageLayout layout = {0};
    layout.workspace =
        (SpectrogramLayoutRect){x, y, width, height};

    float inspector_width =
        clampf_local(width * 0.32f, 344.0f, 400.0f);
    layout.inspector = (SpectrogramLayoutRect){
        x, y, inspector_width, height};
    layout.inspector_content = (SpectrogramLayoutRect){
        x + LAYOUT_SPACE_2,
        y + LAYOUT_SPACE_2,
        inspector_width - LAYOUT_SPACE_4,
        height - LAYOUT_SPACE_4};
    layout.canvas = (SpectrogramLayoutRect){
        x + inspector_width + LAYOUT_SPACE_2,
        y,
        width - inspector_width - LAYOUT_SPACE_2,
        height};

    float content_x = layout.inspector_content.x;
    float content_width = layout.inspector_content.width;
    float action_width =
        (content_width - LAYOUT_SPACE_1) * 0.5f;
    layout.mode_control = (SpectrogramLayoutRect){
        content_x, y + 96.0f, content_width, 40.0f};
    layout.choose_audio_button = (SpectrogramLayoutRect){
        content_x, y + 152.0f, content_width, 42.0f};
    layout.original_button = (SpectrogramLayoutRect){
        content_x, y + 152.0f, action_width, 42.0f};
    layout.reconstruction_button = (SpectrogramLayoutRect){
        content_x + action_width + LAYOUT_SPACE_1,
        y + 152.0f,
        action_width,
        42.0f};
    layout.export_button = (SpectrogramLayoutRect){
        content_x, y + 202.0f, content_width, 42.0f};
    layout.player = (SpectrogramLayoutRect){
        content_x, y + 260.0f, content_width, 68.0f};
    layout.fixed_basis_control = (SpectrogramLayoutRect){
        content_x, y + 368.0f, content_width, 38.0f};
    layout.fixed_presets = (SpectrogramLayoutRect){
        content_x, y + 444.0f, content_width, 38.0f};
    layout.fixed_custom_input = (SpectrogramLayoutRect){
        content_x, y + 498.0f, content_width, 40.0f};
    layout.adaptive_presets = (SpectrogramLayoutRect){
        content_x, y + 396.0f, content_width, 38.0f};
    layout.adaptive_custom_input = (SpectrogramLayoutRect){
        content_x, y + 450.0f, content_width, 40.0f};
    layout.progress_track = (SpectrogramLayoutRect){
        x, y + height - 5.0f, inspector_width, 5.0f};

    layout.canvas_title = (SpectrogramLayoutRect){
        layout.canvas.x + LAYOUT_SPACE_2,
        y + LAYOUT_SPACE_2,
        150.0f,
        24.0f};
    float legend_width =
        fminf(128.0f, layout.canvas.width * 0.32f);
    layout.legend_bar = (SpectrogramLayoutRect){
        layout.canvas.x + layout.canvas.width -
            legend_width - LAYOUT_SPACE_2,
        y + 18.0f,
        legend_width,
        8.0f};
    layout.legend_labels = (SpectrogramLayoutRect){
        layout.legend_bar.x,
        layout.legend_bar.y + 12.0f,
        layout.legend_bar.width,
        18.0f};
    layout.stats_panel = (SpectrogramLayoutRect){
        layout.canvas.x + LAYOUT_SPACE_1,
        y + 64.0f,
        layout.canvas.width - LAYOUT_SPACE_2,
        66.0f};
    layout.stats_content = (SpectrogramLayoutRect){
        layout.canvas.x + LAYOUT_SPACE_2,
        y + 72.0f,
        layout.canvas.width - LAYOUT_SPACE_4,
        50.0f};
    layout.plot = (SpectrogramLayoutRect){
        layout.canvas.x + 64.0f,
        y + 148.0f,
        layout.canvas.width - 80.0f,
        height - 188.0f};
    return layout;
}

bool spectrogram_page_layout_validate(
    const SpectrogramPageLayout *layout,
    bool fixed_mode,
    char *message,
    size_t message_size) {
    if (layout == NULL) {
        return fail_validation(
            message, message_size, "layout is null");
    }
    if (!rect_has_area(layout->workspace) ||
        !rect_has_area(layout->inspector) ||
        !rect_has_area(layout->canvas) ||
        !rect_has_area(layout->plot)) {
        return fail_validation(
            message, message_size, "a primary region has no area");
    }
    if (!rect_contains(layout->workspace, layout->inspector) ||
        !rect_contains(layout->workspace, layout->canvas)) {
        return fail_validation(
            message, message_size, "page columns leave the workspace");
    }
    if (rects_overlap(layout->inspector, layout->canvas)) {
        return fail_validation(
            message, message_size, "page columns overlap");
    }

    const SpectrogramLayoutRect inspector_regions[] = {
        layout->mode_control,
        layout->choose_audio_button,
        layout->original_button,
        layout->reconstruction_button,
        layout->export_button,
        layout->player,
        layout->fixed_basis_control,
        layout->fixed_presets,
        layout->fixed_custom_input,
        layout->adaptive_presets,
        layout->adaptive_custom_input,
        layout->progress_track,
    };
    size_t inspector_region_count =
        sizeof(inspector_regions) /
        sizeof(inspector_regions[0]);
    for (size_t index = 0U;
         index < inspector_region_count;
         index++) {
        if (!rect_has_area(inspector_regions[index]) ||
            !rect_contains(layout->inspector,
                           inspector_regions[index])) {
            return fail_validation(
                message,
                message_size,
                "an inspector control leaves its panel");
        }
    }
    if (rects_overlap(layout->original_button,
                      layout->reconstruction_button) ||
        rects_overlap(layout->original_button,
                      layout->export_button) ||
        rects_overlap(layout->reconstruction_button,
                      layout->export_button) ||
        rects_overlap(layout->export_button,
                      layout->player)) {
        return fail_validation(
            message, message_size, "playback controls overlap");
    }
    SpectrogramLayoutRect mode_presets =
        fixed_mode ? layout->fixed_presets
                   : layout->adaptive_presets;
    SpectrogramLayoutRect mode_custom =
        fixed_mode ? layout->fixed_custom_input
                   : layout->adaptive_custom_input;
    if (rects_overlap(layout->player, mode_presets) ||
        rects_overlap(mode_presets, mode_custom)) {
        return fail_validation(
            message,
            message_size,
            "mode-specific inspector controls overlap");
    }

    const SpectrogramLayoutRect canvas_regions[] = {
        layout->canvas_title,
        layout->legend_bar,
        layout->legend_labels,
        layout->stats_panel,
        layout->stats_content,
        layout->plot,
    };
    size_t canvas_region_count =
        sizeof(canvas_regions) / sizeof(canvas_regions[0]);
    for (size_t index = 0U;
         index < canvas_region_count;
         index++) {
        if (!rect_has_area(canvas_regions[index]) ||
            !rect_contains(layout->canvas,
                           canvas_regions[index])) {
            return fail_validation(
                message,
                message_size,
                "a canvas region leaves its panel");
        }
    }
    if (rects_overlap(layout->canvas_title,
                      layout->legend_bar) ||
        rects_overlap(layout->canvas_title,
                      layout->legend_labels)) {
        return fail_validation(
            message, message_size, "title and legend overlap");
    }
    if (rects_overlap(layout->legend_labels,
                      layout->stats_panel)) {
        return fail_validation(
            message,
            message_size,
            "legend labels and statistics overlap");
    }
    if (!rect_contains(layout->stats_panel,
                       layout->stats_content)) {
        return fail_validation(
            message,
            message_size,
            "statistics content leaves its surface");
    }
    if (rects_overlap(layout->stats_panel, layout->plot)) {
        return fail_validation(
            message, message_size, "statistics and plot overlap");
    }
    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }
    return true;
}
