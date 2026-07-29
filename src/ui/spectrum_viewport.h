#ifndef SPECTRA_SPECTRUM_VIEWPORT_H
#define SPECTRA_SPECTRUM_VIEWPORT_H

#include "dsp/dsp_types.h"

typedef struct {
    float x;
    float y;
    float width;
    float height;
} SpectrumViewportRect;

typedef struct {
    float minimum_frequency;
    float maximum_frequency;
    float minimum_db;
    float maximum_db;
} SpectrumViewport;

SpectrumViewport spectrum_viewport_default(float data_maximum_frequency);
void spectrum_viewport_clamp(SpectrumViewport *viewport,
                             float data_maximum_frequency);

float spectrum_viewport_frequency_at_x(const SpectrumViewport *viewport,
                                       SpectrumViewportRect plot,
                                       float x);
float spectrum_viewport_db_at_y(const SpectrumViewport *viewport,
                                SpectrumViewportRect plot,
                                float y);
float spectrum_viewport_x_for_frequency(const SpectrumViewport *viewport,
                                        SpectrumViewportRect plot,
                                        float frequency);
float spectrum_viewport_y_for_db(const SpectrumViewport *viewport,
                                 SpectrumViewportRect plot,
                                 float db);

void spectrum_viewport_zoom(SpectrumViewport *viewport,
                            SpectrumViewportRect plot,
                            float anchor_x,
                            float anchor_y,
                            float wheel_delta,
                            float data_maximum_frequency);
void spectrum_viewport_pan_pixels(SpectrumViewport *viewport,
                                  SpectrumViewportRect plot,
                                  float delta_x,
                                  float delta_y,
                                  float data_maximum_frequency);

int spectrum_viewport_nearest_peak(const SpectrumViewport *viewport,
                                   SpectrumViewportRect plot,
                                   const Peak *peaks,
                                   int peak_count,
                                   float x,
                                   float y,
                                   float radius);

#endif
