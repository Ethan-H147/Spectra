#include "platform/parallel_for.h"

#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

typedef struct {
    SpectraParallelRangeFunction function;
    void *context;
    size_t begin;
    size_t end;
    unsigned int worker_index;
#if defined(_WIN32)
    HANDLE thread;
#else
    pthread_t thread;
    bool started;
#endif
} ParallelWorker;

#if defined(_WIN32)
static volatile LONG configured_thread_limit = 0;
#else
static unsigned int configured_thread_limit = 0U;
#endif

void spectra_atomic_boolean_init(
    SpectraAtomicBoolean *atomic_value,
    bool value) {
    if (atomic_value == NULL) {
        return;
    }
    spectra_atomic_boolean_store(atomic_value, value);
}

bool spectra_atomic_boolean_load(
    const SpectraAtomicBoolean *atomic_value) {
    if (atomic_value == NULL) {
        return false;
    }
#if defined(_WIN32)
    return InterlockedCompareExchange(
               (volatile LONG *)&atomic_value->value,
               0,
               0) != 0;
#else
    return __atomic_load_n(
               &atomic_value->value,
               __ATOMIC_ACQUIRE) != 0L;
#endif
}

void spectra_atomic_boolean_store(
    SpectraAtomicBoolean *atomic_value,
    bool value) {
    if (atomic_value == NULL) {
        return;
    }
#if defined(_WIN32)
    InterlockedExchange(
        (volatile LONG *)&atomic_value->value,
        value ? 1L : 0L);
#else
    __atomic_store_n(
        &atomic_value->value,
        value ? 1L : 0L,
        __ATOMIC_RELEASE);
#endif
}

unsigned int spectra_hardware_thread_count(void) {
    static unsigned int cached_count = 0U;
    if (cached_count > 0U) {
        return cached_count;
    }
#if defined(_WIN32)
    SYSTEM_INFO information;
    GetSystemInfo(&information);
    cached_count =
        information.dwNumberOfProcessors > 0U
            ? (unsigned int)information.dwNumberOfProcessors
            : 1U;
#else
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    cached_count = count > 0L ? (unsigned int)count : 1U;
#endif
    return cached_count;
}

unsigned int spectra_parallel_thread_limit(void) {
#if defined(_WIN32)
    return (unsigned int)InterlockedCompareExchange(
        &configured_thread_limit,
        0,
        0);
#else
    return __atomic_load_n(
        &configured_thread_limit,
        __ATOMIC_RELAXED);
#endif
}

void spectra_set_parallel_thread_limit(unsigned int thread_limit) {
    const unsigned int hardware =
        spectra_hardware_thread_count();
    const unsigned int bounded =
        thread_limit > hardware ? hardware : thread_limit;
#if defined(_WIN32)
    InterlockedExchange(
        &configured_thread_limit,
        (LONG)bounded);
#else
    __atomic_store_n(
        &configured_thread_limit,
        bounded,
        __ATOMIC_RELAXED);
#endif
}

unsigned int spectra_effective_worker_count(void) {
    const unsigned int configured =
        spectra_parallel_thread_limit();
    if (configured > 0U) {
        return configured;
    }
    const unsigned int hardware =
        spectra_hardware_thread_count();
    const unsigned int responsive =
        hardware > 4U ? hardware - 1U : hardware;
    return responsive > 32U ? 32U : responsive;
}

unsigned int spectra_parallel_worker_count(
    size_t item_count,
    size_t minimum_items_per_worker) {
    if (item_count == 0U) {
        return 0U;
    }
    if (minimum_items_per_worker == 0U) {
        minimum_items_per_worker = 1U;
    }
    const size_t useful_workers =
        (item_count + minimum_items_per_worker - 1U) /
        minimum_items_per_worker;
    const unsigned int maximum_workers =
        spectra_effective_worker_count();
    const unsigned int worker_count =
        useful_workers < (size_t)maximum_workers
            ? (unsigned int)useful_workers
            : maximum_workers;
    return worker_count > 0U ? worker_count : 1U;
}

static void run_worker(ParallelWorker *worker) {
    worker->function(
        worker->begin,
        worker->end,
        worker->worker_index,
        worker->context);
}

#if defined(_WIN32)
static DWORD WINAPI worker_entry(void *context) {
    run_worker((ParallelWorker *)context);
    return 0U;
}
#else
static void *worker_entry(void *context) {
    run_worker((ParallelWorker *)context);
    return NULL;
}
#endif

bool spectra_parallel_for(
    size_t item_count,
    size_t minimum_items_per_worker,
    SpectraParallelRangeFunction function,
    void *context) {
    return spectra_parallel_for_limited(
        item_count,
        minimum_items_per_worker,
        spectra_hardware_thread_count(),
        function,
        context);
}

bool spectra_parallel_for_limited(
    size_t item_count,
    size_t minimum_items_per_worker,
    unsigned int maximum_worker_count,
    SpectraParallelRangeFunction function,
    void *context) {
    if (function == NULL) {
        return false;
    }
    if (item_count == 0U) {
        return true;
    }
    if (minimum_items_per_worker == 0U) {
        minimum_items_per_worker = 1U;
    }

    unsigned int worker_count =
        spectra_parallel_worker_count(
            item_count,
            minimum_items_per_worker);
    if (maximum_worker_count == 0U) {
        maximum_worker_count = 1U;
    }
    if (worker_count > maximum_worker_count) {
        worker_count = maximum_worker_count;
    }
    if (worker_count < 2U) {
        function(0U, item_count, 0U, context);
        return true;
    }

    ParallelWorker *workers = (ParallelWorker *)calloc(
        worker_count,
        sizeof(*workers));
    if (workers == NULL) {
        function(0U, item_count, 0U, context);
        return true;
    }

    const size_t base_count = item_count / worker_count;
    const size_t remainder = item_count % worker_count;
    size_t begin = 0U;
    for (unsigned int index = 0U;
         index < worker_count;
         ++index) {
        const size_t count =
            base_count + (index < remainder ? 1U : 0U);
        workers[index].function = function;
        workers[index].context = context;
        workers[index].begin = begin;
        workers[index].end = begin + count;
        workers[index].worker_index = index;
        begin += count;
    }

    for (unsigned int index = 1U;
         index < worker_count;
         ++index) {
#if defined(_WIN32)
        workers[index].thread = CreateThread(
            NULL,
            0U,
            worker_entry,
            &workers[index],
            0U,
            NULL);
#else
        workers[index].started =
            pthread_create(
                &workers[index].thread,
                NULL,
                worker_entry,
                &workers[index]) == 0;
#endif
    }

    run_worker(&workers[0]);
    for (unsigned int index = 1U;
         index < worker_count;
         ++index) {
#if defined(_WIN32)
        if (workers[index].thread == NULL) {
            run_worker(&workers[index]);
            continue;
        }
        WaitForSingleObject(workers[index].thread, INFINITE);
        CloseHandle(workers[index].thread);
#else
        if (!workers[index].started) {
            run_worker(&workers[index]);
            continue;
        }
        pthread_join(workers[index].thread, NULL);
#endif
    }

    free(workers);
    return true;
}
