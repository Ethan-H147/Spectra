#include "dsp/dsp_types.h"

#include <stdlib.h>

void sample_buffer_free(SampleBuffer *buffer) {
    if (buffer == NULL) {
        return;
    }

    free(buffer->samples);
    buffer->samples = NULL;
    buffer->count = 0;
    buffer->sample_rate = 0;
}

void spectrum_free(Spectrum *spectrum) {
    if (spectrum == NULL) {
        return;
    }

    free(spectrum->frequencies);
    free(spectrum->magnitudes);
    spectrum->frequencies = NULL;
    spectrum->magnitudes = NULL;
    spectrum->count = 0;
}
