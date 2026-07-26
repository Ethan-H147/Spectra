#include "dsp/channel_mix.h"

bool downmix_interleaved_to_mono(const float *interleaved,
                                 size_t frame_count,
                                 unsigned int channel_count,
                                 float *mono) {
    if (interleaved == NULL || mono == NULL || frame_count == 0U || channel_count == 0U) {
        return false;
    }

    for (size_t frame = 0; frame < frame_count; frame++) {
        double sum = 0.0;
        size_t frame_offset = frame * (size_t)channel_count;
        for (unsigned int channel = 0; channel < channel_count; channel++) {
            sum += interleaved[frame_offset + channel];
        }
        mono[frame] = (float)(sum / (double)channel_count);
    }
    return true;
}
