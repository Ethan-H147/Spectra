#include "audio/reconstruction_cache.h"
#include "platform/background_task.h"
#include "platform/parallel_for.h"

#include <stdio.h>
#include <stdlib.h>

#define ASSERT_TRUE(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "FAIL: %s\n", message); \
            return 1; \
        } \
    } while (0)

typedef struct {
    int completed_steps;
} WorkerFixture;

typedef struct {
    unsigned int *visits;
    SpectraAtomicBoolean *saw_nonzero_worker;
} ParallelFixture;

static void complete_worker(BackgroundTaskControl *control,
                            void *context) {
    WorkerFixture *fixture = (WorkerFixture *)context;
    for (int step = 0; step < 10000; step++) {
        if (background_task_cancel_requested(control)) return;
        fixture->completed_steps++;
        if ((step % 250) == 0) {
            background_task_report_progress(
                control, (float)step / 10000.0f);
        }
    }
    background_task_report_progress(control, 1.0f);
}

static void cancellable_worker(BackgroundTaskControl *control,
                               void *context) {
    WorkerFixture *fixture = (WorkerFixture *)context;
    while (!background_task_cancel_requested(control)) {
        fixture->completed_steps++;
    }
}

static void visit_parallel_range(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context) {
    ParallelFixture *fixture = (ParallelFixture *)context;
    if (worker_index > 0U &&
        fixture->saw_nonzero_worker != NULL) {
        spectra_atomic_boolean_store(
            fixture->saw_nonzero_worker,
            true);
    }
    for (size_t index = begin; index < end; ++index) {
        fixture->visits[index]++;
    }
}

static InterleavedBuffer make_audio(size_t frames,
                                    unsigned int channels,
                                    float value) {
    InterleavedBuffer audio = {
        .frame_count = frames,
        .sample_rate = 48000U,
        .channel_count = channels,
    };
    size_t sample_count = frames * (size_t)channels;
    audio.samples =
        (float *)calloc(sample_count, sizeof(float));
    if (audio.samples != NULL) {
        for (size_t index = 0U; index < sample_count; index++) {
            audio.samples[index] = value;
        }
    }
    return audio;
}

static int test_background_completion(void) {
    BackgroundTask task;
    background_task_init(&task);
    WorkerFixture fixture = {0};
    ASSERT_TRUE(background_task_start(
                    &task, complete_worker, &fixture),
                "background task should start");
    ASSERT_TRUE(background_task_has_work(&task),
                "started task should own worker state");
    background_task_join(&task);
    ASSERT_TRUE(fixture.completed_steps == 10000,
                "joining should wait for background work to finish");
    ASSERT_TRUE(!background_task_has_work(&task),
                "joining should release worker state");
    return 0;
}

static int test_background_cancellation(void) {
    BackgroundTask task;
    background_task_init(&task);
    WorkerFixture fixture = {0};
    ASSERT_TRUE(background_task_start(
                    &task, cancellable_worker, &fixture),
                "cancellable background task should start");
    background_task_cancel_and_join(&task);
    ASSERT_TRUE(!background_task_has_work(&task),
                "cancel and join should release worker state");
    return 0;
}

static int test_parallel_for(void) {
    SpectraAtomicBoolean flag;
    spectra_atomic_boolean_init(&flag, false);
    ASSERT_TRUE(!spectra_atomic_boolean_load(&flag),
                "atomic boolean should initialize false");
    spectra_atomic_boolean_store(&flag, true);
    ASSERT_TRUE(spectra_atomic_boolean_load(&flag),
                "atomic boolean should publish cancellation safely");

    const size_t item_count = 4096U;
    unsigned int *visits = (unsigned int *)calloc(
        item_count,
        sizeof(*visits));
    ASSERT_TRUE(visits != NULL,
                "parallel test should allocate visit counters");
    ParallelFixture fixture = {.visits = visits};

    spectra_set_parallel_thread_limit(1U);
    ASSERT_TRUE(spectra_effective_worker_count() == 1U,
                "single-thread mode should remain available");
    ASSERT_TRUE(spectra_parallel_for(
                    item_count,
                    64U,
                    visit_parallel_range,
                    &fixture),
                "single-thread parallel-for fallback should run");
    for (size_t index = 0U; index < item_count; ++index) {
        ASSERT_TRUE(visits[index] == 1U,
                    "single-thread fallback should visit each item once");
        visits[index] = 0U;
    }

    spectra_set_parallel_thread_limit(0U);
    ASSERT_TRUE(spectra_effective_worker_count() >= 1U &&
                    spectra_effective_worker_count() <=
                        spectra_hardware_thread_count(),
                "automatic workers should respect hardware capacity");
    ASSERT_TRUE(spectra_parallel_for(
                    item_count,
                    64U,
                    visit_parallel_range,
                    &fixture),
                "automatic parallel-for should run");
    for (size_t index = 0U; index < item_count; ++index) {
        ASSERT_TRUE(visits[index] == 1U,
                    "parallel execution should visit each item once");
        visits[index] = 0U;
    }

    SpectraAtomicBoolean saw_nonzero_worker;
    spectra_atomic_boolean_init(
        &saw_nonzero_worker, false);
    fixture.saw_nonzero_worker = &saw_nonzero_worker;
    ASSERT_TRUE(spectra_parallel_for_limited(
                    item_count,
                    64U,
                    1U,
                    visit_parallel_range,
                    &fixture),
                "operation-local worker caps should run");
    ASSERT_TRUE(!spectra_atomic_boolean_load(
                    &saw_nonzero_worker),
                "an operation-local cap should prevent extra workers");
    for (size_t index = 0U; index < item_count; ++index) {
        ASSERT_TRUE(visits[index] == 1U,
                    "limited execution should visit each item once");
    }
    free(visits);
    return 0;
}

static int test_reconstruction_cache(void) {
    ReconstructionCache cache;
    reconstruction_cache_init(&cache, 128U);
    InterleavedBuffer first = make_audio(8U, 2U, 0.25f);
    ReconstructionCacheKey first_key = {
        RECONSTRUCTION_CACHE_GLOBAL, 5, 2U};
    ASSERT_TRUE(reconstruction_cache_store_move(
                    &cache, first_key, &first, 0.5f),
                "cache should accept a moved stereo reconstruction");
    ASSERT_TRUE(first.samples == NULL &&
                    reconstruction_cache_contains(
                        &cache, first_key),
                "cache insertion should transfer ownership");

    InterleavedBuffer restored = {0};
    float retained_energy = 0.0f;
    ASSERT_TRUE(reconstruction_cache_take(
                    &cache,
                    first_key,
                    &restored,
                    &retained_energy),
                "cached reconstruction should be reusable");
    ASSERT_TRUE(restored.samples != NULL &&
                    restored.channel_count == 2U &&
                    retained_energy == 0.5f &&
                    cache.hit_count == 1U,
                "cache hit should restore audio and metadata");
    ASSERT_TRUE(!reconstruction_cache_contains(
                    &cache, first_key),
                "taking an entry should transfer it out of the cache");
    interleaved_buffer_free(&restored);

    for (int index = 0; index < 8; index++) {
        InterleavedBuffer audio =
            make_audio(8U, 1U, (float)index);
        ReconstructionCacheKey key = {
            RECONSTRUCTION_CACHE_STFT, index + 1, 1U};
        ASSERT_TRUE(reconstruction_cache_store_move(
                        &cache, key, &audio, 0.1f),
                    "cache should store successive reconstructions");
    }
    ASSERT_TRUE(reconstruction_cache_entry_count(&cache) <=
                    RECONSTRUCTION_CACHE_CAPACITY &&
                    cache.bytes_used <= cache.byte_limit,
                "cache should enforce both entry and memory limits");

    InterleavedBuffer oversized =
        make_audio(64U, 1U, 1.0f);
    ASSERT_TRUE(!reconstruction_cache_store_move(
                    &cache,
                    (ReconstructionCacheKey){
                        RECONSTRUCTION_CACHE_GLOBAL, 99, 1U},
                    &oversized,
                    1.0f) &&
                    oversized.samples != NULL &&
                    cache.bytes_used <= cache.byte_limit,
                "an oversized entry should stay with the caller");
    interleaved_buffer_free(&oversized);

    InterleavedBuffer mono = make_audio(8U, 1U, 0.5f);
    ReconstructionCacheKey mono_key = {
        RECONSTRUCTION_CACHE_GLOBAL, 5, 1U};
    ASSERT_TRUE(reconstruction_cache_store_move(
                    &cache, mono_key, &mono, 0.25f),
                "cache should store a mono variant separately");
    ASSERT_TRUE(reconstruction_cache_contains(
                    &cache, mono_key) &&
                    !reconstruction_cache_contains(
                        &cache, first_key),
                "cache identity should include output channel count");
    reconstruction_cache_free(&cache);
    return 0;
}

int main(void) {
    if (test_background_completion() != 0) return 1;
    if (test_background_cancellation() != 0) return 1;
    if (test_parallel_for() != 0) return 1;
    if (test_reconstruction_cache() != 0) return 1;
    puts("All runtime tests passed.");
    return 0;
}
