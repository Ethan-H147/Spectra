#ifndef SPECTRA_BACKGROUND_TASK_H
#define SPECTRA_BACKGROUND_TASK_H

#include <stdbool.h>

typedef struct BackgroundTaskControl BackgroundTaskControl;
typedef void (*BackgroundTaskFunction)(BackgroundTaskControl *control,
                                       void *context);

typedef struct {
    void *state;
} BackgroundTask;

void background_task_init(BackgroundTask *task);
bool background_task_start(BackgroundTask *task,
                           BackgroundTaskFunction function,
                           void *context);
bool background_task_has_work(const BackgroundTask *task);
bool background_task_is_running(const BackgroundTask *task);
bool background_task_is_complete(const BackgroundTask *task);
float background_task_progress(const BackgroundTask *task);
void background_task_request_cancel(BackgroundTask *task);
void background_task_join(BackgroundTask *task);
void background_task_cancel_and_join(BackgroundTask *task);

bool background_task_cancel_requested(
    BackgroundTaskControl *control);
void background_task_report_progress(BackgroundTaskControl *control,
                                     float progress);

#endif
