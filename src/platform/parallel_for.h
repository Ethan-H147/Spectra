#ifndef SPECTRA_PLATFORM_PARALLEL_FOR_H
#define SPECTRA_PLATFORM_PARALLEL_FOR_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*SpectraParallelRangeFunction)(
    size_t begin,
    size_t end,
    unsigned int worker_index,
    void *context);

typedef struct {
    volatile long value;
} SpectraAtomicBoolean;

void spectra_atomic_boolean_init(
    SpectraAtomicBoolean *atomic_value,
    bool value);
bool spectra_atomic_boolean_load(
    const SpectraAtomicBoolean *atomic_value);
void spectra_atomic_boolean_store(
    SpectraAtomicBoolean *atomic_value,
    bool value);

unsigned int spectra_hardware_thread_count(void);
unsigned int spectra_parallel_thread_limit(void);
unsigned int spectra_effective_worker_count(void);
unsigned int spectra_parallel_worker_count(
    size_t item_count,
    size_t minimum_items_per_worker);
void spectra_set_parallel_thread_limit(unsigned int thread_limit);

bool spectra_parallel_for(
    size_t item_count,
    size_t minimum_items_per_worker,
    SpectraParallelRangeFunction function,
    void *context);

bool spectra_parallel_for_limited(
    size_t item_count,
    size_t minimum_items_per_worker,
    unsigned int maximum_worker_count,
    SpectraParallelRangeFunction function,
    void *context);

#endif
