#include "dsp/channel_mix.h"

#include <stdint.h>
#include <stdlib.h>

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

bool interleave_sample_buffers(const SampleBuffer *channels,
                               unsigned int channel_count,
                               InterleavedBuffer *output) {
    if (channels == NULL || output == NULL || channel_count == 0U ||
        channel_count > 2U || channels[0].samples == NULL ||
        channels[0].count == 0U || channels[0].sample_rate == 0U ||
        channels[0].count >
            SIZE_MAX / ((size_t)channel_count * sizeof(float))) {
        return false;
    }

    size_t frame_count = channels[0].count;
    unsigned int sample_rate = channels[0].sample_rate;
    for (unsigned int channel = 1U; channel < channel_count; channel++) {
        if (channels[channel].samples == NULL ||
            channels[channel].count != frame_count ||
            channels[channel].sample_rate != sample_rate) {
            return false;
        }
    }

    size_t sample_count = frame_count * (size_t)channel_count;
    float *samples = (float *)calloc(sample_count, sizeof(float));
    if (samples == NULL) {
        return false;
    }
    for (size_t frame = 0U; frame < frame_count; frame++) {
        size_t destination = frame * (size_t)channel_count;
        for (unsigned int channel = 0U; channel < channel_count;
             channel++) {
            samples[destination + channel] =
                channels[channel].samples[frame];
        }
    }

    interleaved_buffer_free(output);
    *output = (InterleavedBuffer){
        .samples = samples,
        .frame_count = frame_count,
        .sample_rate = sample_rate,
        .channel_count = channel_count,
    };
    return true;
}
