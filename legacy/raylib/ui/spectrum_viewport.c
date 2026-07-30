#include "ui/spectrum_viewport.h"

#include <float.h>
#include <math.h>

#define SPECTRUM_WORLD_MINIMUM_DB (-120.0f)
#define SPECTRUM_WORLD_MAXIMUM_DB 12.0f
#define SPECTRUM_MINIMUM_DB_SPAN 12.0f
#define SPECTRUM_MINIMUM_FREQUENCY_SPAN 25.0f
#define SPECTRUM_ZOOM_FACTOR 0.82f

static float viewport_clampf(float value, float minimum, float maximum) {
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static float valid_data_maximum(float data_maximum_frequency) {
    return data_maximum_frequency > 0.0f ? data_maximum_frequency : 1.0f;
}

static void clamp_range(float *minimum,
                        float *maximum,
                        float world_minimum,
                        float world_maximum,
                        float minimum_span) {
    float world_span = world_maximum - world_minimum;
    if (world_span <= 0.0f) return;
    if (minimum_span > world_span) minimum_span = world_span;

    float span = *maximum - *minimum;
    if (span < minimum_span) {
        float center = (*minimum + *maximum) * 0.5f;
        span = minimum_span;
        *minimum = center - span * 0.5f;
        *maximum = center + span * 0.5f;
    }
    if (span >= world_span) {
        *minimum = world_minimum;
        *maximum = world_maximum;
        return;
    }
    if (*minimum < world_minimum) {
        *maximum += world_minimum - *minimum;
        *minimum = world_minimum;
    }
    if (*maximum > world_maximum) {
        *minimum -= *maximum - world_maximum;
        *maximum = world_maximum;
    }
}

SpectrumViewport spectrum_viewport_default(float data_maximum_frequency) {
    float maximum = valid_data_maximum(data_maximum_frequency);
    SpectrumViewport viewport = {
        .minimum_frequency = maximum > 20.0f ? 20.0f : 0.0f,
        .maximum_frequency = maximum,
        .minimum_db = -90.0f,
        .maximum_db = 0.0f,
    };
    spectrum_viewport_clamp(&viewport, maximum);
    return viewport;
}

void spectrum_viewport_clamp(SpectrumViewport *viewport,
                             float data_maximum_frequency) {
    if (viewport == NULL) return;
    float maximum = valid_data_maximum(data_maximum_frequency);
    float minimum_frequency_span =
        fminf(SPECTRUM_MINIMUM_FREQUENCY_SPAN, maximum);
    clamp_range(&viewport->minimum_frequency,
                &viewport->maximum_frequency,
                0.0f,
                maximum,
                minimum_frequency_span);
    clamp_range(&viewport->minimum_db,
                &viewport->maximum_db,
                SPECTRUM_WORLD_MINIMUM_DB,
                SPECTRUM_WORLD_MAXIMUM_DB,
                SPECTRUM_MINIMUM_DB_SPAN);
}

float spectrum_viewport_frequency_at_x(const SpectrumViewport *viewport,
                                       SpectrumViewportRect plot,
                                       float x) {
    if (viewport == NULL || plot.width <= 0.0f) return 0.0f;
    float position = viewport_clampf((x - plot.x) / plot.width, 0.0f, 1.0f);
    return viewport->minimum_frequency +
           position *
               (viewport->maximum_frequency - viewport->minimum_frequency);
}

float spectrum_viewport_db_at_y(const SpectrumViewport *viewport,
                                SpectrumViewportRect plot,
                                float y) {
    if (viewport == NULL || plot.height <= 0.0f) return 0.0f;
    float position =
        1.0f - viewport_clampf((y - plot.y) / plot.height, 0.0f, 1.0f);
    return viewport->minimum_db +
           position * (viewport->maximum_db - viewport->minimum_db);
}

float spectrum_viewport_x_for_frequency(const SpectrumViewport *viewport,
                                        SpectrumViewportRect plot,
                                        float frequency) {
    if (viewport == NULL) return plot.x;
    float span = viewport->maximum_frequency - viewport->minimum_frequency;
    if (span <= FLT_EPSILON) return plot.x;
    return plot.x +
           (frequency - viewport->minimum_frequency) / span * plot.width;
}

float spectrum_viewport_y_for_db(const SpectrumViewport *viewport,
                                 SpectrumViewportRect plot,
                                 float db) {
    if (viewport == NULL) return plot.y;
    float span = viewport->maximum_db - viewport->minimum_db;
    if (span <= FLT_EPSILON) return plot.y;
    return plot.y +
           (viewport->maximum_db - db) / span * plot.height;
}

void spectrum_viewport_zoom(SpectrumViewport *viewport,
                            SpectrumViewportRect plot,
                            float anchor_x,
                            float anchor_y,
                            float wheel_delta,
                            float data_maximum_frequency) {
    if (viewport == NULL || plot.width <= 0.0f || plot.height <= 0.0f ||
        wheel_delta == 0.0f) {
        return;
    }

    float horizontal_anchor =
        viewport_clampf((anchor_x - plot.x) / plot.width, 0.0f, 1.0f);
    float vertical_anchor =
        1.0f - viewport_clampf((anchor_y - plot.y) / plot.height, 0.0f, 1.0f);
    float anchor_frequency =
        viewport->minimum_frequency +
        horizontal_anchor *
            (viewport->maximum_frequency - viewport->minimum_frequency);
    float anchor_db =
        viewport->minimum_db +
        vertical_anchor * (viewport->maximum_db - viewport->minimum_db);
    float scale = powf(SPECTRUM_ZOOM_FACTOR, wheel_delta);
    float frequency_span =
        (viewport->maximum_frequency - viewport->minimum_frequency) * scale;
    float db_span = (viewport->maximum_db - viewport->minimum_db) * scale;

    viewport->minimum_frequency =
        anchor_frequency - horizontal_anchor * frequency_span;
    viewport->maximum_frequency =
        viewport->minimum_frequency + frequency_span;
    viewport->minimum_db = anchor_db - vertical_anchor * db_span;
    viewport->maximum_db = viewport->minimum_db + db_span;
    spectrum_viewport_clamp(viewport, data_maximum_frequency);
}

void spectrum_viewport_pan_pixels(SpectrumViewport *viewport,
                                  SpectrumViewportRect plot,
                                  float delta_x,
                                  float delta_y,
                                  float data_maximum_frequency) {
    if (viewport == NULL || plot.width <= 0.0f || plot.height <= 0.0f) {
        return;
    }
    float frequency_shift =
        delta_x / plot.width *
        (viewport->maximum_frequency - viewport->minimum_frequency);
    float db_shift =
        delta_y / plot.height *
        (viewport->maximum_db - viewport->minimum_db);
    viewport->minimum_frequency -= frequency_shift;
    viewport->maximum_frequency -= frequency_shift;
    viewport->minimum_db += db_shift;
    viewport->maximum_db += db_shift;
    spectrum_viewport_clamp(viewport, data_maximum_frequency);
}

int spectrum_viewport_nearest_peak(const SpectrumViewport *viewport,
                                   SpectrumViewportRect plot,
                                   const Peak *peaks,
                                   int peak_count,
                                   float x,
                                   float y,
                                   float radius) {
    if (viewport == NULL || peaks == NULL || peak_count <= 0 ||
        radius < 0.0f) {
        return -1;
    }

    float best_distance_squared = radius * radius;
    int best_index = -1;
    for (int index = 0; index < peak_count; index++) {
        if (peaks[index].frequency < viewport->minimum_frequency ||
            peaks[index].frequency > viewport->maximum_frequency ||
            peaks[index].db < viewport->minimum_db ||
            peaks[index].db > viewport->maximum_db) {
            continue;
        }
        float peak_x =
            spectrum_viewport_x_for_frequency(viewport, plot, peaks[index].frequency);
        float peak_y = spectrum_viewport_y_for_db(viewport, plot, peaks[index].db);
        float delta_x = peak_x - x;
        float delta_y = peak_y - y;
        float distance_squared = delta_x * delta_x + delta_y * delta_y;
        if (distance_squared <= best_distance_squared) {
            best_distance_squared = distance_squared;
            best_index = index;
        }
    }
    return best_index;
}
