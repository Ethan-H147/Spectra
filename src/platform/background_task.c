#include "platform/background_task.h"

#include <stdlib.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct {
    BackgroundTaskFunction function;
    void *context;
    bool running;
    bool complete;
    bool cancel_requested;
    float progress;
#if defined(_WIN32)
    HANDLE thread;
    CRITICAL_SECTION lock;
#else
    pthread_t thread;
    pthread_mutex_t lock;
#endif
} BackgroundTaskState;

struct BackgroundTaskControl {
    BackgroundTaskState *state;
};

static void state_lock(BackgroundTaskState *state) {
#if defined(_WIN32)
    EnterCriticalSection(&state->lock);
#else
    pthread_mutex_lock(&state->lock);
#endif
}

static void state_unlock(BackgroundTaskState *state) {
#if defined(_WIN32)
    LeaveCriticalSection(&state->lock);
#else
    pthread_mutex_unlock(&state->lock);
#endif
}

static void run_task(BackgroundTaskState *state) {
    BackgroundTaskControl control = {.state = state};
    state->function(&control, state->context);
    state_lock(state);
    state->running = false;
    state->complete = true;
    state_unlock(state);
}

#if defined(_WIN32)
static DWORD WINAPI background_thread_main(void *context) {
    run_task((BackgroundTaskState *)context);
    return 0U;
}
#else
static void *background_thread_main(void *context) {
    run_task((BackgroundTaskState *)context);
    return NULL;
}
#endif

void background_task_init(BackgroundTask *task) {
    if (task != NULL) {
        *task = (BackgroundTask){0};
    }
}

bool background_task_start(BackgroundTask *task,
                           BackgroundTaskFunction function,
                           void *context) {
    if (task == NULL || function == NULL || task->state != NULL) {
        return false;
    }

    BackgroundTaskState *state =
        (BackgroundTaskState *)calloc(1U, sizeof(*state));
    if (state == NULL) return false;
    state->function = function;
    state->context = context;
    state->running = true;

#if defined(_WIN32)
    InitializeCriticalSection(&state->lock);
    state->thread = CreateThread(NULL,
                                 0U,
                                 background_thread_main,
                                 state,
                                 0U,
                                 NULL);
    if (state->thread == NULL) {
        DeleteCriticalSection(&state->lock);
        free(state);
        return false;
    }
#else
    if (pthread_mutex_init(&state->lock, NULL) != 0) {
        free(state);
        return false;
    }
    if (pthread_create(&state->thread,
                       NULL,
                       background_thread_main,
                       state) != 0) {
        pthread_mutex_destroy(&state->lock);
        free(state);
        return false;
    }
#endif
    task->state = state;
    return true;
}

bool background_task_has_work(const BackgroundTask *task) {
    return task != NULL && task->state != NULL;
}

static bool read_flag(const BackgroundTask *task, bool complete) {
    if (task == NULL || task->state == NULL) return false;
    BackgroundTaskState *state =
        (BackgroundTaskState *)task->state;
    state_lock(state);
    bool value = complete ? state->complete : state->running;
    state_unlock(state);
    return value;
}

bool background_task_is_running(const BackgroundTask *task) {
    return read_flag(task, false);
}

bool background_task_is_complete(const BackgroundTask *task) {
    return read_flag(task, true);
}

float background_task_progress(const BackgroundTask *task) {
    if (task == NULL || task->state == NULL) return 0.0f;
    BackgroundTaskState *state =
        (BackgroundTaskState *)task->state;
    state_lock(state);
    float progress = state->progress;
    state_unlock(state);
    return progress;
}

void background_task_request_cancel(BackgroundTask *task) {
    if (task == NULL || task->state == NULL) return;
    BackgroundTaskState *state =
        (BackgroundTaskState *)task->state;
    state_lock(state);
    state->cancel_requested = true;
    state_unlock(state);
}

bool background_task_cancel_requested(
    BackgroundTaskControl *control) {
    if (control == NULL || control->state == NULL) return true;
    BackgroundTaskState *state = control->state;
    state_lock(state);
    bool requested = state->cancel_requested;
    state_unlock(state);
    return requested;
}

void background_task_report_progress(BackgroundTaskControl *control,
                                     float progress) {
    if (control == NULL || control->state == NULL) return;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    BackgroundTaskState *state = control->state;
    state_lock(state);
    state->progress = progress;
    state_unlock(state);
}

void background_task_join(BackgroundTask *task) {
    if (task == NULL || task->state == NULL) return;
    BackgroundTaskState *state =
        (BackgroundTaskState *)task->state;
#if defined(_WIN32)
    WaitForSingleObject(state->thread, INFINITE);
    CloseHandle(state->thread);
    DeleteCriticalSection(&state->lock);
#else
    pthread_join(state->thread, NULL);
    pthread_mutex_destroy(&state->lock);
#endif
    free(state);
    task->state = NULL;
}

void background_task_cancel_and_join(BackgroundTask *task) {
    background_task_request_cancel(task);
    background_task_join(task);
}
