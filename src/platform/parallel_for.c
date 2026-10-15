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
    bool stopping;
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
    /* Counts processors in every group, not only the first 64. */
    const DWORD count =
        GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    cached_count = count > 0U ? (unsigned int)count : 1U;
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

/* Runs worker 0 on the calling thread and the rest on short-lived threads.
 * Used when the shared pool is already serving another caller. */
static void run_with_new_threads(
    ParallelWorker *workers,
    unsigned int worker_count) {
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
}

/*
 * A process-wide pool of persistent worker threads, started on first use.
 * One caller at a time owns the pool; the caller always runs worker 0 itself
 * and then helps claim any task the pool threads have not started.
 */
#define POOL_MAXIMUM_THREADS 63U

typedef struct {
#if defined(_WIN32)
    SRWLOCK lock;
    CONDITION_VARIABLE work_available;
    CONDITION_VARIABLE work_finished;
#else
    pthread_mutex_t lock;
    pthread_cond_t work_available;
    pthread_cond_t work_finished;
#endif
    bool started;
    bool stopping;
    bool busy;
    unsigned int thread_count;
    ParallelWorker *tasks;
    unsigned int task_count;
    unsigned int next_task;
    unsigned int pending_tasks;
} WorkerPool;

#if defined(_WIN32)
static WorkerPool worker_pool = {
    .lock = SRWLOCK_INIT,
    .work_available = CONDITION_VARIABLE_INIT,
    .work_finished = CONDITION_VARIABLE_INIT,
};
#else
static WorkerPool worker_pool = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .work_available = PTHREAD_COND_INITIALIZER,
    .work_finished = PTHREAD_COND_INITIALIZER,
};
#endif

static void pool_lock(void) {
#if defined(_WIN32)
    AcquireSRWLockExclusive(&worker_pool.lock);
#else
    pthread_mutex_lock(&worker_pool.lock);
#endif
}

static void pool_unlock(void) {
#if defined(_WIN32)
    ReleaseSRWLockExclusive(&worker_pool.lock);
#else
    pthread_mutex_unlock(&worker_pool.lock);
#endif
}

static void pool_wait(bool for_work) {
#if defined(_WIN32)
    SleepConditionVariableSRW(
        for_work ? &worker_pool.work_available
                 : &worker_pool.work_finished,
        &worker_pool.lock,
        INFINITE,
        0U);
#else
    pthread_cond_wait(
        for_work ? &worker_pool.work_available
                 : &worker_pool.work_finished,
        &worker_pool.lock);
#endif
}

static void pool_wake_workers(void) {
#if defined(_WIN32)
    WakeAllConditionVariable(&worker_pool.work_available);
#else
    pthread_cond_broadcast(&worker_pool.work_available);
#endif
}

static void pool_wake_owner(void) {
#if defined(_WIN32)
    WakeAllConditionVariable(&worker_pool.work_finished);
#else
    pthread_cond_broadcast(&worker_pool.work_finished);
#endif
}

/* Claims and runs one unstarted task. Called with the lock held; returns
 * with the lock held. */
static bool pool_run_next_task(void) {
    if (!worker_pool.busy ||
        worker_pool.next_task >= worker_pool.task_count) {
        return false;
    }
    ParallelWorker *task =
        &worker_pool.tasks[worker_pool.next_task];
    ++worker_pool.next_task;
    pool_unlock();
    run_worker(task);
    pool_lock();
    --worker_pool.pending_tasks;
    if (worker_pool.pending_tasks == 0U) {
        pool_wake_owner();
    }
    return true;
}

static void pool_thread_loop(void) {
    pool_lock();
    while (!worker_pool.stopping) {
        if (!pool_run_next_task()) {
            pool_wait(true);
        }
    }
    pool_unlock();
}

#if defined(_WIN32)
static DWORD WINAPI pool_thread_entry(void *context) {
    (void)context;
    pool_thread_loop();
    return 0U;
}
#else
static void *pool_thread_entry(void *context) {
    (void)context;
    pool_thread_loop();
    return NULL;
}
#endif

/* Called with the lock held. */
static void pool_start_threads(void) {
    if (worker_pool.started) {
        return;
    }
    worker_pool.started = true;
    const unsigned int hardware = spectra_hardware_thread_count();
    unsigned int wanted = hardware > 1U ? hardware - 1U : 0U;
    if (wanted > POOL_MAXIMUM_THREADS) {
        wanted = POOL_MAXIMUM_THREADS;
    }
    for (unsigned int index = 0U; index < wanted; ++index) {
#if defined(_WIN32)
        HANDLE thread = CreateThread(
            NULL, 0U, pool_thread_entry, NULL, 0U, NULL);
        if (thread == NULL) {
            break;
        }
        CloseHandle(thread);
#else
        pthread_t thread;
        if (pthread_create(
                &thread, NULL, pool_thread_entry, NULL) != 0) {
            break;
        }
        pthread_detach(thread);
#endif
        ++worker_pool.thread_count;
    }
}

static bool run_on_pool(
    ParallelWorker *workers,
    unsigned int worker_count) {
    pool_lock();
    if (worker_pool.busy) {
        pool_unlock();
        return false;
    }
    pool_start_threads();
    if (worker_pool.thread_count == 0U) {
        pool_unlock();
        return false;
    }
    worker_pool.busy = true;
    worker_pool.tasks = workers;
    worker_pool.task_count = worker_count;
    worker_pool.next_task = 1U;
    worker_pool.pending_tasks = worker_count - 1U;
    pool_wake_workers();
    pool_unlock();

    run_worker(&workers[0]);

    pool_lock();
    while (pool_run_next_task()) {
    }
    while (worker_pool.pending_tasks > 0U) {
        pool_wait(false);
    }
    worker_pool.busy = false;
    worker_pool.tasks = NULL;
    worker_pool.task_count = 0U;
    worker_pool.next_task = 0U;
    pool_unlock();
    return true;
}

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

    if (!run_on_pool(workers, worker_count)) {
        run_with_new_threads(workers, worker_count);
    }

    free(workers);
    return true;
}
