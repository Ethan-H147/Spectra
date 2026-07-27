#include "ui/spectrogram_layout.h"

#include <stdio.h>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

static int validate_workspace(float width, float height) {
    SpectrogramPageLayout layout =
        spectrogram_page_layout(312.0f, 112.0f, width, height);
    char message[160];
    ASSERT_TRUE(
        spectrogram_page_layout_validate(
            &layout, true, message, sizeof(message)),
        message);
    ASSERT_TRUE(
        spectrogram_page_layout_validate(
            &layout, false, message, sizeof(message)),
        message);

    float legend_clearance =
        layout.stats_panel.y -
        (layout.legend_labels.y +
         layout.legend_labels.height);
    ASSERT_TRUE(
        legend_clearance >= 8.0f,
        "legend labels should have at least 8 px before statistics");

    float plot_clearance =
        layout.plot.y -
        (layout.stats_panel.y + layout.stats_panel.height);
    ASSERT_TRUE(
        plot_clearance >= 16.0f,
        "statistics should have at least 16 px before the plot");

    float action_gap =
        layout.reconstruction_button.x -
        (layout.original_button.x +
         layout.original_button.width);
    ASSERT_TRUE(
        action_gap >= 8.0f,
        "playback buttons should keep an 8 px gap");
    return 0;
}

int main(void) {
    ASSERT_TRUE(
        validate_workspace(756.0f, 624.0f) == 0,
        "minimum window layout should be valid");
    ASSERT_TRUE(
        validate_workspace(1256.0f, 724.0f) == 0,
        "default window layout should be valid");
    ASSERT_TRUE(
        validate_workspace(1704.0f, 920.0f) == 0,
        "full-screen layout should be valid");
    ASSERT_TRUE(
        validate_workspace(2400.0f, 1200.0f) == 0,
        "ultrawide layout should be valid");

    printf("All UI layout tests passed.\n");
    return 0;
}
