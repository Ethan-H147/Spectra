#include "ui/spectrogram_layout.h"
#include "ui/spectrum_viewport.h"

#include <math.h>
#include <stdio.h>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

#define ASSERT_NEAR(actual, expected, tolerance, message) \
    ASSERT_TRUE(fabsf((actual) - (expected)) <= (tolerance), message)

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

static int validate_spectrum_viewport(void) {
    SpectrumViewportRect plot = {10.0f, 20.0f, 600.0f, 300.0f};
    SpectrumViewport viewport = spectrum_viewport_default(20000.0f);
    ASSERT_NEAR(viewport.minimum_frequency, 20.0f, 0.001f,
                "default spectrum should begin at 20 Hz");
    ASSERT_NEAR(viewport.maximum_frequency, 20000.0f, 0.001f,
                "default spectrum should end at the visible data limit");
    ASSERT_NEAR(viewport.minimum_db, -90.0f, 0.001f,
                "default spectrum should begin at -90 dB");
    ASSERT_NEAR(viewport.maximum_db, 0.0f, 0.001f,
                "default spectrum should end at 0 dB");

    float anchor_x = 310.0f;
    float anchor_y = 95.0f;
    float anchor_frequency =
        spectrum_viewport_frequency_at_x(&viewport, plot, anchor_x);
    float anchor_db =
        spectrum_viewport_db_at_y(&viewport, plot, anchor_y);
    float original_frequency_span =
        viewport.maximum_frequency - viewport.minimum_frequency;
    float original_db_span = viewport.maximum_db - viewport.minimum_db;
    spectrum_viewport_zoom(
        &viewport, plot, anchor_x, anchor_y, 2.0f, 20000.0f);
    ASSERT_TRUE(
        viewport.maximum_frequency - viewport.minimum_frequency <
            original_frequency_span,
        "wheel-up should zoom the frequency scale in");
    ASSERT_TRUE(
        viewport.maximum_db - viewport.minimum_db < original_db_span,
        "wheel-up should zoom the dB scale in");
    ASSERT_NEAR(
        spectrum_viewport_frequency_at_x(&viewport, plot, anchor_x),
        anchor_frequency,
        0.01f,
        "zoom should preserve the frequency under the cursor");
    ASSERT_NEAR(
        spectrum_viewport_db_at_y(&viewport, plot, anchor_y),
        anchor_db,
        0.01f,
        "zoom should preserve the dB value under the cursor");

    float frequency_before_pan = viewport.minimum_frequency;
    float db_before_pan = viewport.minimum_db;
    spectrum_viewport_pan_pixels(
        &viewport, plot, 60.0f, 30.0f, 20000.0f);
    ASSERT_TRUE(
        viewport.minimum_frequency < frequency_before_pan,
        "dragging right should move spectrum content right");
    ASSERT_TRUE(
        viewport.minimum_db > db_before_pan,
        "dragging down should move spectrum content down");

    SpectrumViewport hit_view = {
        .minimum_frequency = 0.0f,
        .maximum_frequency = 1000.0f,
        .minimum_db = -100.0f,
        .maximum_db = 0.0f,
    };
    Peak peaks[] = {
        {.frequency = 250.0f, .db = -30.0f},
        {.frequency = 500.0f, .db = -50.0f},
        {.frequency = 750.0f, .db = -70.0f},
    };
    float peak_x =
        spectrum_viewport_x_for_frequency(&hit_view, plot, peaks[1].frequency);
    float peak_y =
        spectrum_viewport_y_for_db(&hit_view, plot, peaks[1].db);
    ASSERT_TRUE(
        spectrum_viewport_nearest_peak(
            &hit_view, plot, peaks, 3, peak_x + 5.0f, peak_y - 4.0f, 12.0f) == 1,
        "hover selection should snap to the nearest visible peak");
    ASSERT_TRUE(
        spectrum_viewport_nearest_peak(
            &hit_view, plot, peaks, 3, plot.x, plot.y, 8.0f) == -1,
        "hover selection should ignore distant peaks");

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
    ASSERT_TRUE(
        validate_spectrum_viewport() == 0,
        "spectrum viewport behavior should be valid");

    printf("All UI layout and spectrum viewport tests passed.\n");
    return 0;
}
