#ifndef SPECTRA_RECONSTRUCTION_CACHE_H
#define SPECTRA_RECONSTRUCTION_CACHE_H

#include "dsp/dsp_types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RECONSTRUCTION_CACHE_CAPACITY 6U

typedef enum {
    RECONSTRUCTION_CACHE_GLOBAL = 1,
    RECONSTRUCTION_CACHE_STFT = 2
} ReconstructionCacheMode;

typedef struct {
    ReconstructionCacheMode mode;
    int component_count;
    unsigned int channel_count;
} ReconstructionCacheKey;

typedef struct {
    bool occupied;
    ReconstructionCacheKey key;
    InterleavedBuffer audio;
    float retained_energy;
    uint64_t last_used;
} ReconstructionCacheEntry;

typedef struct {
    ReconstructionCacheEntry entries[RECONSTRUCTION_CACHE_CAPACITY];
    size_t byte_limit;
    size_t bytes_used;
    uint64_t use_counter;
    unsigned int hit_count;
    unsigned int miss_count;
} ReconstructionCache;

void reconstruction_cache_init(ReconstructionCache *cache,
                               size_t byte_limit);
void reconstruction_cache_free(ReconstructionCache *cache);
bool reconstruction_cache_store_move(ReconstructionCache *cache,
                                     ReconstructionCacheKey key,
                                     InterleavedBuffer *audio,
                                     float retained_energy);
bool reconstruction_cache_take(ReconstructionCache *cache,
                               ReconstructionCacheKey key,
                               InterleavedBuffer *audio,
                               float *retained_energy);
bool reconstruction_cache_contains(const ReconstructionCache *cache,
                                   ReconstructionCacheKey key);
size_t reconstruction_cache_entry_count(const ReconstructionCache *cache);

#endif
