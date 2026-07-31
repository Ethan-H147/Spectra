#include "dsp/spectral_effects.h"

#include "dsp/fft.h"
#include "dsp/windowing.h"
#include "platform/parallel_for.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static bool valid_source(const InterleavedBuffer *source) {
    return source != NULL && source->samples != NULL &&
           source->frame_count > 0U && source->sample_rate > 0U &&
           source->channel_count > 0U &&
           source->frame_count <=
               SIZE_MAX / (size_t)source->channel_count;
}

static bool valid_transform(unsigned int window_size,
                            unsigned int hop_size) {
    return window_size >= 8U &&
           (window_size & (window_size - 1U)) == 0U &&
           hop_size > 0U && hop_size <= window_size;
}

typedef struct {
    unsigned int size;
    unsigned int *reversed_indices;
    float *twiddle_real;
    float *twiddle_imaginary;
} RepeatedFftPlan;

static unsigned int reverse_bits_for_plan(
    unsigned int value,
    unsigned int bits) {
    unsigned int reversed = 0U;
    for (unsigned int index = 0U; index < bits; ++index) {
        reversed =
            (reversed << 1U) | (value & 1U);
        value >>= 1U;
    }
    return reversed;
}

static bool repeated_fft_plan_init(
    RepeatedFftPlan *plan,
    unsigned int size) {
    if (plan == NULL || size < 2U ||
        (size & (size - 1U)) != 0U) {
        return false;
    }

    *plan = (RepeatedFftPlan){0};
    plan->reversed_indices =
        (unsigned int *)calloc(
            size, sizeof(unsigned int));
    plan->twiddle_real =
        (float *)calloc(size - 1U, sizeof(float));
    plan->twiddle_imaginary =
        (float *)calloc(size - 1U, sizeof(float));
    if (plan->reversed_indices == NULL ||
        plan->twiddle_real == NULL ||
        plan->twiddle_imaginary == NULL) {
        free(plan->reversed_indices);
        free(plan->twiddle_real);
        free(plan->twiddle_imaginary);
        *plan = (RepeatedFftPlan){0};
        return false;
    }

    unsigned int bits = 0U;
    for (unsigned int value = size;
         value > 1U;
         value >>= 1U) {
        ++bits;
    }
    for (unsigned int index = 0U; index < size; ++index) {
        plan->reversed_indices[index] =
            reverse_bits_for_plan(index, bits);
    }

    for (unsigned int block_size = 2U;
         block_size <= size;
         block_size <<= 1U) {
        const unsigned int half_block =
            block_size / 2U;
        const unsigned int offset = half_block - 1U;
        const float angle_step =
            -2.0f * (float)M_PI /
            (float)block_size;
        const float step_real = cosf(angle_step);
        const float step_imaginary =
            sinf(angle_step);
        float twiddle_real = 1.0f;
        float twiddle_imaginary = 0.0f;
        for (unsigned int index = 0U;
             index < half_block;
             ++index) {
            plan->twiddle_real[offset + index] =
                twiddle_real;
            plan->twiddle_imaginary[offset + index] =
                twiddle_imaginary;
            const float next_real =
                twiddle_real * step_real -
                twiddle_imaginary * step_imaginary;
            twiddle_imaginary =
                twiddle_real * step_imaginary +
                twiddle_imaginary * step_real;
            twiddle_real = next_real;
        }
    }
    plan->size = size;
    return true;
}

static void repeated_fft_plan_free(RepeatedFftPlan *plan) {
    if (plan == NULL) {
        return;
    }
    free(plan->reversed_indices);
    free(plan->twiddle_real);
    free(plan->twiddle_imaginary);
    *plan = (RepeatedFftPlan){0};
}

static void repeated_fft(
    const RepeatedFftPlan *plan,
    float *real,
    float *imaginary) {
    if (plan == NULL || real == NULL ||
        imaginary == NULL || plan->size < 2U) {
        return;
    }

    const unsigned int size = plan->size;
    for (unsigned int index = 0U; index < size; ++index) {
        const unsigned int reversed =
            plan->reversed_indices[index];
        if (reversed > index) {
            const float real_value = real[index];
            const float imaginary_value =
                imaginary[index];
            real[index] = real[reversed];
            imaginary[index] = imaginary[reversed];
            real[reversed] = real_value;
            imaginary[reversed] = imaginary_value;
        }
    }

    for (unsigned int block_size = 2U;
         block_size <= size;
         block_size <<= 1U) {
        const unsigned int half_block =
            block_size / 2U;
        const unsigned int offset = half_block - 1U;
        for (unsigned int block_start = 0U;
             block_start < size;
             block_start += block_size) {
            for (unsigned int index = 0U;
                 index < half_block;
                 ++index) {
                const unsigned int even_index =
                    block_start + index;
                const unsigned int odd_index =
                    even_index + half_block;
                const float twiddle_real =
                    plan->twiddle_real[offset + index];
                const float twiddle_imaginary =
                    plan->twiddle_imaginary[
                        offset + index];
                const float transformed_real =
                    twiddle_real * real[odd_index] -
                    twiddle_imaginary *
                        imaginary[odd_index];
                const float transformed_imaginary =
                    twiddle_real *
                        imaginary[odd_index] +
                    twiddle_imaginary *
                        real[odd_index];

                real[odd_index] =
                    real[even_index] -
                    transformed_real;
                imaginary[odd_index] =
                    imaginary[even_index] -
                    transformed_imaginary;
                real[even_index] += transformed_real;
                imaginary[even_index] +=
                    transformed_imaginary;
            }
        }
    }
}

static bool report_progress(
    SpectralEffectProgressCallback callback,
    void *context,
    float progress) {
    return callback == NULL || callback(progress, context);
}

static bool allocate_interleaved(size_t frame_count,
                                 unsigned int sample_rate,
                                 unsigned int channel_count,
                                 InterleavedBuffer *buffer) {
    if (buffer == NULL || frame_count == 0U || sample_rate == 0U ||
        channel_count == 0U ||
        frame_count > SIZE_MAX / (size_t)channel_count) {
        return false;
    }
    const size_t sample_count =
        frame_count * (size_t)channel_count;
    if (sample_count > SIZE_MAX / sizeof(float)) {
        return false;
    }

    float *samples =
        (float *)calloc(sample_count, sizeof(float));
    if (samples == NULL) {
        return false;
    }
    *buffer = (InterleavedBuffer){
        .samples = samples,
        .frame_count = frame_count,
        .sample_rate = sample_rate,
        .channel_count = channel_count,
    };
    return true;
}

static float interpolated_sample(const float *samples,
                                 size_t frame_count,
                                 unsigned int channel_count,
                                 unsigned int channel,
                                 double position) {
    if (position <= 0.0) {
        return samples[channel];
    }
    const double final_position = (double)(frame_count - 1U);
    if (position >= final_position) {
        return samples[
            (frame_count - 1U) * (size_t)channel_count +
            (size_t)channel];
    }

    const size_t first = (size_t)position;
    const size_t second = first + 1U;
    const float fraction = (float)(position - (double)first);
    const float first_sample =
        samples[first * (size_t)channel_count + (size_t)channel];
    const float second_sample =
        samples[second * (size_t)channel_count + (size_t)channel];
    return first_sample +
           (second_sample - first_sample) * fraction;
}

typedef struct {
    const InterleavedBuffer *source;
    InterleavedBuffer *output;
    float pitch_factor;
} ResampleContext;

static void resample_frame_range(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context_pointer) {
    (void)worker_index;
    ResampleContext *context =
        (ResampleContext *)context_pointer;
    const InterleavedBuffer *source = context->source;
    InterleavedBuffer *output = context->output;
    for (size_t frame = begin; frame < end; ++frame) {
        const double source_position =
            (double)frame * (double)context->pitch_factor;
        for (unsigned int channel = 0U;
             channel < output->channel_count;
             ++channel) {
            output->samples[
                frame * (size_t)output->channel_count +
                (size_t)channel] =
                interpolated_sample(
                    source->samples,
                    source->frame_count,
                    source->channel_count,
                    channel,
                    source_position);
        }
    }
}

bool playback_rate_pitch_shift(
    const InterleavedBuffer *source,
    float pitch_factor,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context) {
    if (!valid_source(source) || output == NULL ||
        !isfinite(pitch_factor) || pitch_factor < 0.125f ||
        pitch_factor > 8.0f) {
        return false;
    }

    const double output_frames_double =
        ceil((double)source->frame_count / (double)pitch_factor);
    if (output_frames_double < 1.0 ||
        output_frames_double > (double)SIZE_MAX) {
        return false;
    }

    InterleavedBuffer prepared = {0};
    if (!allocate_interleaved(
            (size_t)output_frames_double,
            source->sample_rate,
            source->channel_count,
            &prepared)) {
        return false;
    }
    if (!report_progress(
            progress_callback, progress_context, 0.0f)) {
        interleaved_buffer_free(&prepared);
        return false;
    }

    ResampleContext context = {
        .source = source,
        .output = &prepared,
        .pitch_factor = pitch_factor,
    };
    spectra_parallel_for(
        prepared.frame_count,
        65536U,
        resample_frame_range,
        &context);

    if (!report_progress(
            progress_callback, progress_context, 1.0f)) {
        interleaved_buffer_free(&prepared);
        return false;
    }
    interleaved_buffer_free(output);
    *output = prepared;
    return true;
}

static float wrap_phase(float phase) {
    return remainderf(phase, 2.0f * (float)M_PI);
}

static void inverse_fft_real(
    const RepeatedFftPlan *plan,
    float *real,
    float *imaginary) {
    const unsigned int size = plan->size;
    for (unsigned int index = 0U; index < size; ++index) {
        imaginary[index] = -imaginary[index];
    }
    repeated_fft(plan, real, imaginary);
    for (unsigned int index = 0U; index < size; ++index) {
        real[index] /= (float)size;
    }
}

static void inverse_fft_complex(
    const RepeatedFftPlan *plan,
    float *real,
    float *imaginary) {
    const unsigned int size = plan->size;
    for (unsigned int index = 0U; index < size; ++index) {
        imaginary[index] = -imaginary[index];
    }
    repeated_fft(plan, real, imaginary);
    for (unsigned int index = 0U; index < size; ++index) {
        real[index] /= (float)size;
        imaginary[index] =
            -imaginary[index] / (float)size;
    }
}

static size_t centered_frame_count(size_t source_count,
                                   unsigned int hop_size) {
    return 1U +
           (source_count + (size_t)hop_size - 1U) /
               (size_t)hop_size;
}

static void load_centered_frame(
    const InterleavedBuffer *source,
    unsigned int channel,
    size_t frame_index,
    unsigned int window_size,
    unsigned int hop_size,
    const float *window,
    float *real,
    float *imaginary) {
    const size_t padding = (size_t)window_size / 2U;
    const size_t padded_start =
        frame_index * (size_t)hop_size;
    memset(
        real, 0, (size_t)window_size * sizeof(float));
    memset(
        imaginary, 0, (size_t)window_size * sizeof(float));

    const unsigned int first_index =
        padded_start < padding
            ? (unsigned int)(padding - padded_start)
            : 0U;
    const size_t source_start =
        padded_start + (size_t)first_index - padding;
    if (source_start >= source->frame_count ||
        first_index >= window_size) {
        return;
    }
    const size_t available =
        source->frame_count - source_start;
    const unsigned int copy_count =
        (unsigned int)(
            available <
                    (size_t)(window_size - first_index)
                ? available
                : (size_t)(window_size - first_index));
    for (unsigned int offset = 0U;
         offset < copy_count;
         ++offset) {
        const unsigned int index =
            first_index + offset;
        const size_t source_index =
            source_start + (size_t)offset;
        real[index] =
            source->samples[
                source_index *
                    (size_t)source->channel_count +
                (size_t)channel] *
            window[index];
    }
}

typedef struct {
    const InterleavedBuffer *source;
    InterleavedBuffer *output;
    const float *window;
    const float *weights;
    const float *expected_phase;
    const float *base_angular_frequency;
    const RepeatedFftPlan *fft_plan;
    float *worker_real;
    float *worker_imaginary;
    float *worker_previous_phase;
    float *worker_synthesis_phase;
    float *worker_raw;
    float pitch_factor;
    double synthesis_hop;
    unsigned int window_size;
    unsigned int analysis_hop;
    unsigned int task_count;
    size_t phase_count;
    size_t frame_count;
    size_t raw_count;
    SpectralEffectProgressCallback progress_callback;
    void *progress_context;
    SpectraAtomicBoolean cancelled;
} PhaseVocoderContext;

static void process_phase_vocoder_channels(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context_pointer) {
    PhaseVocoderContext *context =
        (PhaseVocoderContext *)context_pointer;
    float *real =
        context->worker_real +
        (size_t)worker_index * context->window_size;
    float *imaginary =
        context->worker_imaginary +
        (size_t)worker_index * context->window_size;
    float *previous_phase =
        context->worker_previous_phase +
        (size_t)worker_index * context->phase_count;
    float *synthesis_phase =
        context->worker_synthesis_phase +
        (size_t)worker_index * context->phase_count;
    float *raw =
        context->worker_raw +
        (size_t)worker_index * context->raw_count;
    const unsigned int final_bin =
        context->window_size / 2U;

    const unsigned int progress_channel_begin =
        (unsigned int)(
            begin * context->source->channel_count /
            context->task_count);
    const unsigned int progress_channel_end =
        (unsigned int)(
            end * context->source->channel_count /
            context->task_count);
    const size_t progress_frame_count =
        (size_t)(progress_channel_end -
                 progress_channel_begin) *
        context->frame_count;

    for (size_t task = begin; task < end; ++task) {
        const unsigned int channel_begin =
            (unsigned int)(
                task * context->source->channel_count /
                context->task_count);
        const unsigned int channel_end =
            (unsigned int)(
                (task + 1U) *
                context->source->channel_count /
                context->task_count);
        for (unsigned int channel = channel_begin;
             channel < channel_end;
             ++channel) {
            if (spectra_atomic_boolean_load(
                    &context->cancelled)) {
                return;
            }
            memset(
                previous_phase,
                0,
                context->phase_count * sizeof(float));
            memset(
                synthesis_phase,
                0,
                context->phase_count * sizeof(float));
            memset(
                raw,
                0,
                context->raw_count * sizeof(float));

            for (size_t frame = 0U;
                 frame < context->frame_count;
                 ++frame) {
                if ((frame & 7U) == 0U &&
                    spectra_atomic_boolean_load(
                        &context->cancelled)) {
                    return;
                }
                load_centered_frame(
                    context->source,
                    channel,
                    frame,
                    context->window_size,
                    context->analysis_hop,
                    context->window,
                    real,
                    imaginary);
                repeated_fft(
                    context->fft_plan, real, imaginary);

                for (unsigned int bin = 0U;
                     bin <= final_bin;
                     ++bin) {
                    const float magnitude =
                        hypotf(real[bin], imaginary[bin]);
                    const float phase =
                        atan2f(imaginary[bin], real[bin]);
                    if (frame == 0U) {
                        synthesis_phase[bin] = phase;
                    } else {
                        const float deviation =
                            wrap_phase(
                                phase -
                                previous_phase[bin] -
                                context->expected_phase[bin]);
                        const float angular_frequency =
                            context
                                ->base_angular_frequency[bin] +
                            deviation /
                                (float)context->analysis_hop;
                        synthesis_phase[bin] +=
                            angular_frequency *
                            (float)context->synthesis_hop;
                    }
                    previous_phase[bin] = phase;

                    if (bin == 0U || bin == final_bin) {
                        real[bin] =
                            magnitude *
                            cosf(synthesis_phase[bin]);
                        imaginary[bin] = 0.0f;
                    } else {
                        const float output_real =
                            magnitude *
                            cosf(synthesis_phase[bin]);
                        const float output_imaginary =
                            magnitude *
                            sinf(synthesis_phase[bin]);
                        real[bin] = output_real;
                        imaginary[bin] = output_imaginary;
                        real[context->window_size - bin] =
                            output_real;
                        imaginary[
                            context->window_size - bin] =
                            -output_imaginary;
                    }
                }

                inverse_fft_real(
                    context->fft_plan, real, imaginary);
                const double synthesis_start =
                    (double)frame * context->synthesis_hop;
                const size_t first_start =
                    (size_t)synthesis_start;
                const float fraction =
                    (float)(
                        synthesis_start -
                        (double)first_start);
                const float first_weight =
                    1.0f - fraction;
                for (unsigned int index = 0U;
                     index < context->window_size;
                     ++index) {
                    const size_t first =
                        first_start + (size_t)index;
                    if (first >= context->raw_count) {
                        break;
                    }
                    const float sample =
                        real[index] * context->window[index];
                    raw[first] += sample * first_weight;
                    if (fraction > 0.0f &&
                        first + 1U < context->raw_count) {
                        raw[first + 1U] += sample * fraction;
                    }
                }

                if (worker_index == 0U &&
                    (frame & 7U) == 7U &&
                    progress_frame_count > 0U) {
                    const size_t completed_frames =
                        (size_t)(
                            channel - progress_channel_begin) *
                            context->frame_count +
                        frame + 1U;
                    if (!report_progress(
                            context->progress_callback,
                            context->progress_context,
                            0.85f *
                                (float)completed_frames /
                                (float)progress_frame_count)) {
                        spectra_atomic_boolean_store(
                            &context->cancelled, true);
                        return;
                    }
                }
            }

            for (size_t index = 0U;
                 index < context->raw_count;
                 ++index) {
                raw[index] =
                    context->weights[index] > 1.0e-8f
                        ? raw[index] /
                              context->weights[index]
                        : 0.0f;
            }

            const double padding =
                (double)context->window_size * 0.5;
            for (size_t frame = 0U;
                 frame < context->source->frame_count;
                 ++frame) {
                const double position =
                    padding +
                    (double)frame *
                        (double)context->pitch_factor;
                const size_t first = (size_t)position;
                const size_t second =
                    first + 1U < context->raw_count
                        ? first + 1U
                        : first;
                const float fraction =
                    (float)(position - (double)first);
                const float sample =
                    first < context->raw_count
                        ? raw[first] +
                              (raw[second] - raw[first]) *
                                  fraction
                        : 0.0f;
                context->output->samples[
                    frame *
                        (size_t)context->source->channel_count +
                    (size_t)channel] = sample;
            }
        }
    }
}

bool phase_vocoder_pitch_shift(
    const InterleavedBuffer *source,
    float pitch_factor,
    unsigned int window_size,
    unsigned int analysis_hop,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context) {
    if (!valid_source(source) || output == NULL ||
        !valid_transform(window_size, analysis_hop) ||
        !isfinite(pitch_factor) || pitch_factor < 0.25f ||
        pitch_factor > 4.0f) {
        return false;
    }

    const size_t frame_count =
        centered_frame_count(
            source->frame_count, analysis_hop);
    const double synthesis_hop =
        (double)analysis_hop * (double)pitch_factor;
    const double raw_count_double =
        ceil((double)(frame_count - 1U) * synthesis_hop) +
        (double)window_size + 2.0;
    if (raw_count_double < (double)window_size ||
        raw_count_double > (double)SIZE_MAX) {
        return false;
    }
    const size_t raw_count = (size_t)raw_count_double;
    const unsigned int task_count =
        source->channel_count < 4U
            ? source->channel_count
            : 4U;
    const unsigned int worker_count =
        spectra_parallel_worker_count(task_count, 1U);
    const size_t phase_count =
        (size_t)window_size / 2U + 1U;
    if ((size_t)worker_count >
            SIZE_MAX / (size_t)window_size ||
        (size_t)worker_count >
            SIZE_MAX / phase_count ||
        (size_t)worker_count >
            SIZE_MAX / raw_count) {
        return false;
    }

    InterleavedBuffer prepared = {0};
    if (!allocate_interleaved(
            source->frame_count,
            source->sample_rate,
            source->channel_count,
            &prepared)) {
        return false;
    }

    float *window = NULL;
    float *worker_real = NULL;
    float *worker_imaginary = NULL;
    float *worker_previous_phase = NULL;
    float *worker_synthesis_phase = NULL;
    float *worker_raw = NULL;
    float *weights = NULL;
    float *window_squared = NULL;
    float *expected_phase = NULL;
    float *base_angular_frequency = NULL;
    RepeatedFftPlan fft_plan = {0};
    window =
        (float *)calloc(window_size, sizeof(float));
    worker_real = (float *)calloc(
        (size_t)worker_count * window_size,
        sizeof(float));
    worker_imaginary = (float *)calloc(
        (size_t)worker_count * window_size,
        sizeof(float));
    worker_previous_phase = (float *)calloc(
        (size_t)worker_count * phase_count,
        sizeof(float));
    worker_synthesis_phase = (float *)calloc(
        (size_t)worker_count * phase_count,
        sizeof(float));
    worker_raw = (float *)calloc(
        (size_t)worker_count * raw_count,
        sizeof(float));
    weights =
        (float *)calloc(raw_count, sizeof(float));
    window_squared =
        (float *)calloc(window_size, sizeof(float));
    expected_phase =
        (float *)calloc(phase_count, sizeof(float));
    base_angular_frequency =
        (float *)calloc(phase_count, sizeof(float));
    if (window == NULL ||
        worker_real == NULL ||
        worker_imaginary == NULL ||
        worker_previous_phase == NULL ||
        worker_synthesis_phase == NULL ||
        worker_raw == NULL ||
        weights == NULL ||
        window_squared == NULL ||
        expected_phase == NULL ||
        base_angular_frequency == NULL ||
        !repeated_fft_plan_init(
            &fft_plan, window_size)) {
        free(window);
        free(worker_real);
        free(worker_imaginary);
        free(worker_previous_phase);
        free(worker_synthesis_phase);
        free(worker_raw);
        free(weights);
        free(window_squared);
        free(expected_phase);
        free(base_angular_frequency);
        repeated_fft_plan_free(&fft_plan);
        interleaved_buffer_free(&prepared);
        return false;
    }
    hann_window(window, window_size);
    for (unsigned int index = 0U;
         index < window_size;
         ++index) {
        window_squared[index] =
            window[index] * window[index];
    }

    const unsigned int final_bin = window_size / 2U;
    const float expected_scale =
        2.0f * (float)M_PI *
        (float)analysis_hop / (float)window_size;
    for (unsigned int bin = 0U;
         bin <= final_bin;
         ++bin) {
        expected_phase[bin] =
            expected_scale * (float)bin;
        base_angular_frequency[bin] =
            2.0f * (float)M_PI *
            (float)bin / (float)window_size;
    }

    for (size_t frame = 0U;
         frame < frame_count;
         ++frame) {
        const double synthesis_start =
            (double)frame * synthesis_hop;
        const size_t first_start =
            (size_t)synthesis_start;
        const float fraction =
            (float)(
                synthesis_start -
                (double)first_start);
        const float first_weight =
            1.0f - fraction;
        for (unsigned int index = 0U;
             index < window_size;
             ++index) {
            const size_t first =
                first_start + (size_t)index;
            if (first >= raw_count) {
                break;
            }
            const float weight =
                window_squared[index];
            weights[first] +=
                weight * first_weight;
            if (fraction > 0.0f &&
                first + 1U < raw_count) {
                weights[first + 1U] +=
                    weight * fraction;
            }
        }
    }

    PhaseVocoderContext context = {
        .source = source,
        .output = &prepared,
        .window = window,
        .weights = weights,
        .expected_phase = expected_phase,
        .base_angular_frequency = base_angular_frequency,
        .fft_plan = &fft_plan,
        .worker_real = worker_real,
        .worker_imaginary = worker_imaginary,
        .worker_previous_phase = worker_previous_phase,
        .worker_synthesis_phase = worker_synthesis_phase,
        .worker_raw = worker_raw,
        .pitch_factor = pitch_factor,
        .synthesis_hop = synthesis_hop,
        .window_size = window_size,
        .analysis_hop = analysis_hop,
        .task_count = task_count,
        .phase_count = phase_count,
        .frame_count = frame_count,
        .raw_count = raw_count,
        .progress_callback = progress_callback,
        .progress_context = progress_context,
    };
    spectra_atomic_boolean_init(&context.cancelled, false);
    spectra_parallel_for_limited(
        task_count,
        1U,
        worker_count,
        process_phase_vocoder_channels,
        &context);
    bool completed =
        !spectra_atomic_boolean_load(&context.cancelled);
    if (completed) {
        completed = report_progress(
            progress_callback,
            progress_context,
            0.98f);
    }

    free(window);
    free(worker_real);
    free(worker_imaginary);
    free(worker_previous_phase);
    free(worker_synthesis_phase);
    free(worker_raw);
    free(weights);
    free(window_squared);
    free(expected_phase);
    free(base_angular_frequency);
    repeated_fft_plan_free(&fft_plan);

    if (!completed ||
        !report_progress(
            progress_callback, progress_context, 1.0f)) {
        interleaved_buffer_free(&prepared);
        return false;
    }
    interleaved_buffer_free(output);
    *output = prepared;
    return true;
}

typedef struct {
    InterleavedBuffer *output;
    const float *weights;
} NormalizeOutputContext;

static void normalize_output_range(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context_pointer) {
    (void)worker_index;
    NormalizeOutputContext *context =
        (NormalizeOutputContext *)context_pointer;
    for (size_t frame = begin; frame < end; ++frame) {
        const float weight = context->weights[frame];
        for (unsigned int channel = 0U;
             channel < context->output->channel_count;
             ++channel) {
            const size_t output_index =
                frame *
                    (size_t)context->output->channel_count +
                (size_t)channel;
            context->output->samples[output_index] =
                weight > 1.0e-8f
                    ? context->output->samples[output_index] /
                          weight
                    : 0.0f;
        }
    }
}

typedef struct {
    const InterleavedBuffer *source;
    InterleavedBuffer *output;
    const float *window;
    const double *oscillator_real;
    const double *oscillator_imaginary;
    const RepeatedFftPlan *fft_plan;
    float *worker_real;
    float *worker_imaginary;
    double phase_scale;
    size_t frame_offset;
    size_t frame_stride;
    size_t frame_count;
    unsigned int channel;
    unsigned int window_size;
    unsigned int hop_size;
} FrequencyShiftFrameContext;

static void process_frequency_shift_frame_range(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context_pointer) {
    FrequencyShiftFrameContext *context =
        (FrequencyShiftFrameContext *)context_pointer;
    float *real = context->worker_real +
        (size_t)worker_index * context->window_size;
    float *imaginary = context->worker_imaginary +
        (size_t)worker_index * context->window_size;
    const unsigned int final_bin =
        context->window_size / 2U;
    const size_t padding =
        (size_t)context->window_size / 2U;

    for (size_t item = begin; item < end; ++item) {
        const size_t frame =
            context->frame_offset +
            item * context->frame_stride;
        if (frame >= context->frame_count) {
            continue;
        }
        load_centered_frame(
            context->source,
            context->channel,
            frame,
            context->window_size,
            context->hop_size,
            context->window,
            real,
            imaginary);
        repeated_fft(context->fft_plan, real, imaginary);

        imaginary[0] = 0.0f;
        for (unsigned int bin = 1U;
             bin < final_bin;
             ++bin) {
            real[bin] *= 2.0f;
            imaginary[bin] *= 2.0f;
        }
        imaginary[final_bin] = 0.0f;
        for (unsigned int bin = final_bin + 1U;
             bin < context->window_size;
             ++bin) {
            real[bin] = 0.0f;
            imaginary[bin] = 0.0f;
        }
        inverse_fft_complex(
            context->fft_plan, real, imaginary);

        const size_t padded_start =
            frame * (size_t)context->hop_size;
        const unsigned int first_index =
            padded_start < padding
                ? (unsigned int)(padding - padded_start)
                : 0U;
        const size_t source_start =
            padded_start + (size_t)first_index - padding;
        if (source_start >= context->source->frame_count ||
            first_index >= context->window_size) {
            continue;
        }
        const size_t available =
            context->source->frame_count - source_start;
        const unsigned int copy_count =
            (unsigned int)(
                available <
                        (size_t)(
                            context->window_size - first_index)
                    ? available
                    : (size_t)(
                          context->window_size - first_index));
        const double starting_phase =
            context->phase_scale * (double)source_start;
        const double starting_real = cos(starting_phase);
        const double starting_imaginary = sin(starting_phase);
        for (unsigned int offset = 0U;
             offset < copy_count;
             ++offset) {
            const unsigned int index = first_index + offset;
            const size_t source_index =
                source_start + (size_t)offset;
            const double modulation_real =
                starting_real *
                    context->oscillator_real[offset] -
                starting_imaginary *
                    context->oscillator_imaginary[offset];
            const double modulation_imaginary =
                starting_real *
                    context->oscillator_imaginary[offset] +
                starting_imaginary *
                    context->oscillator_real[offset];
            const float shifted_sample =
                real[index] * (float)modulation_real -
                imaginary[index] *
                    (float)modulation_imaginary;
            context->output->samples[
                source_index *
                    (size_t)context->source->channel_count +
                (size_t)context->channel] +=
                shifted_sample * context->window[index];
        }
    }
}

bool analytic_frequency_shift(
    const InterleavedBuffer *source,
    float shift_hz,
    unsigned int window_size,
    unsigned int hop_size,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context) {
    if (!valid_source(source) || output == NULL ||
        !valid_transform(window_size, hop_size) ||
        !isfinite(shift_hz) ||
        fabsf(shift_hz) >
            (float)source->sample_rate * 0.5f) {
        return false;
    }

    InterleavedBuffer prepared = {0};
    if (!allocate_interleaved(
            source->frame_count,
            source->sample_rate,
            source->channel_count,
            &prepared)) {
        return false;
    }

    const size_t padding = (size_t)window_size / 2U;
    const size_t frame_count =
        centered_frame_count(
            source->frame_count, hop_size);
    const size_t frame_stride =
        ((size_t)window_size + (size_t)hop_size - 1U) /
        (size_t)hop_size;
    const size_t largest_group =
        (frame_count + frame_stride - 1U) /
        frame_stride;
    const unsigned int worker_count =
        spectra_parallel_worker_count(
            largest_group,
            8U);
    float *window =
        (float *)calloc(window_size, sizeof(float));
    float *worker_real = (float *)calloc(
        (size_t)worker_count * window_size,
        sizeof(float));
    float *worker_imaginary = (float *)calloc(
        (size_t)worker_count * window_size,
        sizeof(float));
    float *weights =
        (float *)calloc(
            source->frame_count, sizeof(float));
    float *window_squared =
        (float *)calloc(window_size, sizeof(float));
    double *oscillator_real =
        (double *)calloc(window_size, sizeof(double));
    double *oscillator_imaginary =
        (double *)calloc(window_size, sizeof(double));
    RepeatedFftPlan fft_plan = {0};
    if (window == NULL || worker_real == NULL ||
        worker_imaginary == NULL || weights == NULL ||
        window_squared == NULL ||
        oscillator_real == NULL ||
        oscillator_imaginary == NULL ||
        !repeated_fft_plan_init(
            &fft_plan, window_size)) {
        free(window);
        free(worker_real);
        free(worker_imaginary);
        free(weights);
        free(window_squared);
        free(oscillator_real);
        free(oscillator_imaginary);
        repeated_fft_plan_free(&fft_plan);
        interleaved_buffer_free(&prepared);
        return false;
    }
    hann_window(window, window_size);

    const double phase_scale =
        2.0 * M_PI * (double)shift_hz /
        (double)source->sample_rate;
    for (unsigned int index = 0U;
         index < window_size;
         ++index) {
        window_squared[index] =
            window[index] * window[index];
        const double phase =
            phase_scale * (double)index;
        oscillator_real[index] = cos(phase);
        oscillator_imaginary[index] = sin(phase);
    }

    for (size_t frame = 0U;
         frame < frame_count;
         ++frame) {
        const size_t padded_start =
            frame * (size_t)hop_size;
        const unsigned int first_index =
            padded_start < padding
                ? (unsigned int)(
                      padding - padded_start)
                : 0U;
        const size_t source_start =
            padded_start + (size_t)first_index -
            padding;
        if (source_start >= source->frame_count ||
            first_index >= window_size) {
            continue;
        }
        const size_t available =
            source->frame_count - source_start;
        const unsigned int copy_count =
            (unsigned int)(
                available <
                        (size_t)(
                            window_size - first_index)
                    ? available
                    : (size_t)(
                          window_size - first_index));
        for (unsigned int offset = 0U;
             offset < copy_count;
             ++offset) {
            weights[source_start + (size_t)offset] +=
                window_squared[first_index + offset];
        }
    }

    bool completed = true;

    for (unsigned int channel = 0U;
         channel < source->channel_count && completed;
         ++channel) {
        for (size_t group = 0U;
             group < frame_stride && completed;
             ++group) {
            const size_t group_count =
                frame_count > group
                    ? (frame_count - group +
                       frame_stride - 1U) /
                          frame_stride
                    : 0U;
            FrequencyShiftFrameContext context = {
                .source = source,
                .output = &prepared,
                .window = window,
                .oscillator_real = oscillator_real,
                .oscillator_imaginary =
                    oscillator_imaginary,
                .fft_plan = &fft_plan,
                .worker_real = worker_real,
                .worker_imaginary = worker_imaginary,
                .phase_scale = phase_scale,
                .frame_offset = group,
                .frame_stride = frame_stride,
                .frame_count = frame_count,
                .channel = channel,
                .window_size = window_size,
                .hop_size = hop_size,
            };
            spectra_parallel_for_limited(
                group_count,
                8U,
                worker_count,
                process_frequency_shift_frame_range,
                &context);
            const size_t completed_groups =
                (size_t)channel * frame_stride +
                group + 1U;
            completed = report_progress(
                progress_callback,
                progress_context,
                (float)completed_groups /
                    ((float)source->channel_count *
                     (float)frame_stride) *
                    0.94f);
        }
    }

    if (completed) {
        NormalizeOutputContext normalize_context = {
            .output = &prepared,
            .weights = weights,
        };
        spectra_parallel_for(
            source->frame_count,
            65536U,
            normalize_output_range,
            &normalize_context);
    }

    free(window);
    free(worker_real);
    free(worker_imaginary);
    free(weights);
    free(window_squared);
    free(oscillator_real);
    free(oscillator_imaginary);
    repeated_fft_plan_free(&fft_plan);

    if (!completed ||
        !report_progress(
            progress_callback, progress_context, 1.0f)) {
        interleaved_buffer_free(&prepared);
        return false;
    }
    interleaved_buffer_free(output);
    *output = prepared;
    return true;
}

static float smooth_eq_band_weight(
    float frequency,
    const SpectralEqBand *band) {
    if (band == NULL || !band->enabled ||
        !isfinite(band->low_hz) ||
        !isfinite(band->high_hz) ||
        !isfinite(band->gain_db) ||
        band->high_hz <= band->low_hz ||
        frequency < band->low_hz ||
        frequency > band->high_hz) {
        return 0.0f;
    }

    const float width = band->high_hz - band->low_hz;
    const float transition = fmaxf(1.0f, width * 0.12f);
    const float distance_from_edge = fminf(
        frequency - band->low_hz,
        band->high_hz - frequency);
    if (distance_from_edge >= transition) {
        return 1.0f;
    }

    const float phase = fmaxf(
        0.0f,
        fminf(1.0f, distance_from_edge / transition));
    return 0.5f - 0.5f * cosf((float)M_PI * phase);
}

float spectral_eq_response_db(
    float frequency,
    const SpectralEqBand *bands,
    size_t band_count) {
    if (!isfinite(frequency) ||
        (band_count > 0U && bands == NULL)) {
        return 0.0f;
    }
    float total_db = 0.0f;
    for (size_t index = 0U; index < band_count; ++index) {
        total_db += bands[index].gain_db *
                    smooth_eq_band_weight(
                        frequency, &bands[index]);
    }
    return fmaxf(-72.0f, fminf(48.0f, total_db));
}

static float eq_gain_for_frequency(
    float frequency,
    const SpectralEqBand *bands,
    size_t band_count) {
    const float total_db = spectral_eq_response_db(
        frequency, bands, band_count);
    return powf(10.0f, total_db / 20.0f);
}

typedef struct {
    const InterleavedBuffer *source;
    InterleavedBuffer *output;
    const float *window;
    const float *bin_gains;
    const RepeatedFftPlan *fft_plan;
    float *worker_real;
    float *worker_imaginary;
    size_t frame_offset;
    size_t frame_stride;
    size_t frame_count;
    unsigned int channel;
    unsigned int window_size;
    unsigned int hop_size;
} EqFrameContext;

static void process_eq_frame_range(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context_pointer) {
    EqFrameContext *context =
        (EqFrameContext *)context_pointer;
    float *real = context->worker_real +
        (size_t)worker_index * context->window_size;
    float *imaginary = context->worker_imaginary +
        (size_t)worker_index * context->window_size;
    const unsigned int final_bin =
        context->window_size / 2U;
    const size_t padding =
        (size_t)context->window_size / 2U;

    for (size_t item = begin; item < end; ++item) {
        const size_t frame =
            context->frame_offset +
            item * context->frame_stride;
        if (frame >= context->frame_count) {
            continue;
        }
        load_centered_frame(
            context->source,
            context->channel,
            frame,
            context->window_size,
            context->hop_size,
            context->window,
            real,
            imaginary);
        repeated_fft(context->fft_plan, real, imaginary);

        for (unsigned int bin = 0U;
             bin <= final_bin;
             ++bin) {
            const float gain = context->bin_gains[bin];
            real[bin] *= gain;
            imaginary[bin] *= gain;
            if (bin > 0U && bin < final_bin) {
                const unsigned int mirror =
                    context->window_size - bin;
                real[mirror] *= gain;
                imaginary[mirror] *= gain;
            }
        }
        inverse_fft_real(
            context->fft_plan, real, imaginary);

        const size_t padded_start =
            frame * (size_t)context->hop_size;
        const unsigned int first_index =
            padded_start < padding
                ? (unsigned int)(padding - padded_start)
                : 0U;
        const size_t source_start =
            padded_start + (size_t)first_index - padding;
        if (source_start >= context->source->frame_count ||
            first_index >= context->window_size) {
            continue;
        }
        const size_t available =
            context->source->frame_count - source_start;
        const unsigned int copy_count =
            (unsigned int)(
                available <
                        (size_t)(
                            context->window_size - first_index)
                    ? available
                    : (size_t)(
                          context->window_size - first_index));
        for (unsigned int offset = 0U;
             offset < copy_count;
             ++offset) {
            const unsigned int fft_index =
                first_index + offset;
            const size_t source_index =
                source_start + (size_t)offset;
            context->output->samples[
                source_index *
                    (size_t)context->source->channel_count +
                (size_t)context->channel] +=
                real[fft_index] *
                context->window[fft_index];
        }
    }
}

bool spectral_range_equalize(
    const InterleavedBuffer *source,
    const SpectralEqBand *bands,
    size_t band_count,
    unsigned int window_size,
    unsigned int hop_size,
    InterleavedBuffer *output,
    SpectralEffectProgressCallback progress_callback,
    void *progress_context) {
    if (!valid_source(source) || output == NULL ||
        !valid_transform(window_size, hop_size) ||
        (band_count > 0U && bands == NULL)) {
        return false;
    }

    const float nyquist =
        (float)source->sample_rate * 0.5f;
    for (size_t index = 0U; index < band_count; ++index) {
        if (!isfinite(bands[index].low_hz) ||
            !isfinite(bands[index].high_hz) ||
            !isfinite(bands[index].gain_db) ||
            bands[index].low_hz < 0.0f ||
            bands[index].high_hz > nyquist ||
            bands[index].high_hz <=
                bands[index].low_hz ||
            bands[index].gain_db < -48.0f ||
            bands[index].gain_db > 24.0f) {
            return false;
        }
    }

    InterleavedBuffer prepared = {0};
    if (!allocate_interleaved(
            source->frame_count,
            source->sample_rate,
            source->channel_count,
            &prepared)) {
        return false;
    }

    const size_t padding = (size_t)window_size / 2U;
    const size_t frame_count = centered_frame_count(
        source->frame_count, hop_size);
    const size_t frame_stride =
        ((size_t)window_size + (size_t)hop_size - 1U) /
        (size_t)hop_size;
    const size_t largest_group =
        (frame_count + frame_stride - 1U) /
        frame_stride;
    const unsigned int worker_count =
        spectra_parallel_worker_count(
            largest_group,
            8U);

    float *window =
        (float *)calloc(window_size, sizeof(float));
    float *window_squared =
        (float *)calloc(window_size, sizeof(float));
    float *worker_real = (float *)calloc(
        (size_t)worker_count * window_size,
        sizeof(float));
    float *worker_imaginary = (float *)calloc(
        (size_t)worker_count * window_size,
        sizeof(float));
    float *weights =
        (float *)calloc(
            source->frame_count, sizeof(float));
    float *bin_gains =
        (float *)calloc(
            (size_t)window_size / 2U + 1U,
            sizeof(float));
    RepeatedFftPlan fft_plan = {0};
    if (window == NULL || window_squared == NULL ||
        worker_real == NULL || worker_imaginary == NULL ||
        weights == NULL || bin_gains == NULL ||
        !repeated_fft_plan_init(
            &fft_plan, window_size)) {
        free(window);
        free(window_squared);
        free(worker_real);
        free(worker_imaginary);
        free(weights);
        free(bin_gains);
        repeated_fft_plan_free(&fft_plan);
        interleaved_buffer_free(&prepared);
        return false;
    }

    hann_window(window, window_size);
    for (unsigned int index = 0U;
         index < window_size;
         ++index) {
        window_squared[index] =
            window[index] * window[index];
    }
    const unsigned int final_bin = window_size / 2U;
    for (unsigned int bin = 0U;
         bin <= final_bin;
         ++bin) {
        const float frequency =
            (float)bin * (float)source->sample_rate /
            (float)window_size;
        bin_gains[bin] = eq_gain_for_frequency(
            frequency, bands, band_count);
    }

    for (size_t frame = 0U;
         frame < frame_count;
         ++frame) {
        const size_t padded_start =
            frame * (size_t)hop_size;
        const unsigned int first_index =
            padded_start < padding
                ? (unsigned int)(padding - padded_start)
                : 0U;
        const size_t source_start =
            padded_start + (size_t)first_index -
            padding;
        if (source_start >= source->frame_count ||
            first_index >= window_size) {
            continue;
        }
        const size_t available =
            source->frame_count - source_start;
        const unsigned int copy_count =
            (unsigned int)(
                available <
                        (size_t)(
                            window_size - first_index)
                    ? available
                    : (size_t)(
                          window_size - first_index));
        for (unsigned int offset = 0U;
             offset < copy_count;
             ++offset) {
            weights[source_start + (size_t)offset] +=
                window_squared[first_index + offset];
        }
    }

    bool completed = true;
    for (unsigned int channel = 0U;
         channel < source->channel_count && completed;
         ++channel) {
        for (size_t group = 0U;
             group < frame_stride && completed;
             ++group) {
            const size_t group_count =
                frame_count > group
                    ? (frame_count - group +
                       frame_stride - 1U) /
                          frame_stride
                    : 0U;
            EqFrameContext context = {
                .source = source,
                .output = &prepared,
                .window = window,
                .bin_gains = bin_gains,
                .fft_plan = &fft_plan,
                .worker_real = worker_real,
                .worker_imaginary = worker_imaginary,
                .frame_offset = group,
                .frame_stride = frame_stride,
                .frame_count = frame_count,
                .channel = channel,
                .window_size = window_size,
                .hop_size = hop_size,
            };
            spectra_parallel_for_limited(
                group_count,
                8U,
                worker_count,
                process_eq_frame_range,
                &context);
            const size_t completed_groups =
                (size_t)channel * frame_stride +
                group + 1U;
            completed = report_progress(
                progress_callback,
                progress_context,
                (float)completed_groups /
                    ((float)source->channel_count *
                     (float)frame_stride) *
                    0.94f);
        }
    }

    if (completed) {
        NormalizeOutputContext normalize_context = {
            .output = &prepared,
            .weights = weights,
        };
        spectra_parallel_for(
            source->frame_count,
            65536U,
            normalize_output_range,
            &normalize_context);
    }

    free(window);
    free(window_squared);
    free(worker_real);
    free(worker_imaginary);
    free(weights);
    free(bin_gains);
    repeated_fft_plan_free(&fft_plan);

    if (!completed ||
        !report_progress(
            progress_callback,
            progress_context,
            1.0f)) {
        interleaved_buffer_free(&prepared);
        return false;
    }

    interleaved_buffer_free(output);
    *output = prepared;
    return true;
}
