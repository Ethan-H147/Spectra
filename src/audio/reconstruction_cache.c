#include "audio/reconstruction_cache.h"

#include <limits.h>

static bool cache_key_equal(ReconstructionCacheKey left,
                            ReconstructionCacheKey right) {
    return left.mode == right.mode &&
           left.component_count == right.component_count &&
           left.channel_count == right.channel_count;
}

static size_t audio_bytes(const InterleavedBuffer *audio) {
    if (audio == NULL || audio->samples == NULL ||
        audio->frame_count == 0U || audio->channel_count == 0U ||
        audio->frame_count >
            SIZE_MAX / (size_t)audio->channel_count) {
        return 0U;
    }
    size_t sample_count =
        audio->frame_count * (size_t)audio->channel_count;
    if (sample_count > SIZE_MAX / sizeof(float)) {
        return 0U;
    }
    return sample_count * sizeof(float);
}

static void clear_entry(ReconstructionCache *cache,
                        ReconstructionCacheEntry *entry) {
    if (cache == NULL || entry == NULL || !entry->occupied) {
        return;
    }
    size_t bytes = audio_bytes(&entry->audio);
    if (bytes <= cache->bytes_used) {
        cache->bytes_used -= bytes;
    } else {
        cache->bytes_used = 0U;
    }
    interleaved_buffer_free(&entry->audio);
    *entry = (ReconstructionCacheEntry){0};
}

void reconstruction_cache_init(ReconstructionCache *cache,
                               size_t byte_limit) {
    if (cache == NULL) return;
    *cache = (ReconstructionCache){0};
    cache->byte_limit = byte_limit;
}

void reconstruction_cache_free(ReconstructionCache *cache) {
    if (cache == NULL) return;
    for (size_t index = 0U;
         index < RECONSTRUCTION_CACHE_CAPACITY;
         index++) {
        clear_entry(cache, &cache->entries[index]);
    }
    *cache = (ReconstructionCache){0};
}

static ReconstructionCacheEntry *find_entry(
    ReconstructionCache *cache,
    ReconstructionCacheKey key) {
    if (cache == NULL) return NULL;
    for (size_t index = 0U;
         index < RECONSTRUCTION_CACHE_CAPACITY;
         index++) {
        ReconstructionCacheEntry *entry = &cache->entries[index];
        if (entry->occupied && cache_key_equal(entry->key, key)) {
            return entry;
        }
    }
    return NULL;
}

static ReconstructionCacheEntry *oldest_entry(
    ReconstructionCache *cache) {
    ReconstructionCacheEntry *oldest = NULL;
    if (cache == NULL) return NULL;
    for (size_t index = 0U;
         index < RECONSTRUCTION_CACHE_CAPACITY;
         index++) {
        ReconstructionCacheEntry *entry = &cache->entries[index];
        if (!entry->occupied) continue;
        if (oldest == NULL ||
            entry->last_used < oldest->last_used) {
            oldest = entry;
        }
    }
    return oldest;
}

static ReconstructionCacheEntry *empty_entry(
    ReconstructionCache *cache) {
    if (cache == NULL) return NULL;
    for (size_t index = 0U;
         index < RECONSTRUCTION_CACHE_CAPACITY;
         index++) {
        if (!cache->entries[index].occupied) {
            return &cache->entries[index];
        }
    }
    return NULL;
}

bool reconstruction_cache_store_move(ReconstructionCache *cache,
                                     ReconstructionCacheKey key,
                                     InterleavedBuffer *audio,
                                     float retained_energy) {
    size_t bytes = audio_bytes(audio);
    if (cache == NULL || audio == NULL || bytes == 0U ||
        key.component_count <= 0 || key.channel_count == 0U ||
        key.channel_count != audio->channel_count) {
        return false;
    }
    if (cache->byte_limit > 0U && bytes > cache->byte_limit) {
        return false;
    }

    ReconstructionCacheEntry *entry = find_entry(cache, key);
    if (entry != NULL) {
        clear_entry(cache, entry);
    }

    while (cache->byte_limit > 0U &&
           cache->bytes_used > cache->byte_limit - bytes) {
        ReconstructionCacheEntry *oldest =
            oldest_entry(cache);
        if (oldest == NULL) break;
        clear_entry(cache, oldest);
    }

    entry = empty_entry(cache);
    if (entry == NULL) {
        entry = oldest_entry(cache);
        clear_entry(cache, entry);
    }
    if (entry == NULL) {
        return false;
    }

    cache->use_counter++;
    *entry = (ReconstructionCacheEntry){
        .occupied = true,
        .key = key,
        .audio = *audio,
        .retained_energy = retained_energy,
        .last_used = cache->use_counter,
    };
    cache->bytes_used += bytes;
    *audio = (InterleavedBuffer){0};
    return true;
}

bool reconstruction_cache_take(ReconstructionCache *cache,
                               ReconstructionCacheKey key,
                               InterleavedBuffer *audio,
                               float *retained_energy) {
    if (cache == NULL || audio == NULL) return false;
    ReconstructionCacheEntry *entry = find_entry(cache, key);
    if (entry == NULL) {
        cache->miss_count++;
        return false;
    }

    size_t bytes = audio_bytes(&entry->audio);
    interleaved_buffer_free(audio);
    *audio = entry->audio;
    if (retained_energy != NULL) {
        *retained_energy = entry->retained_energy;
    }
    entry->audio = (InterleavedBuffer){0};
    if (bytes <= cache->bytes_used) {
        cache->bytes_used -= bytes;
    } else {
        cache->bytes_used = 0U;
    }
    *entry = (ReconstructionCacheEntry){0};
    cache->hit_count++;
    return true;
}

bool reconstruction_cache_contains(const ReconstructionCache *cache,
                                   ReconstructionCacheKey key) {
    if (cache == NULL) return false;
    for (size_t index = 0U;
         index < RECONSTRUCTION_CACHE_CAPACITY;
         index++) {
        const ReconstructionCacheEntry *entry =
            &cache->entries[index];
        if (entry->occupied && cache_key_equal(entry->key, key)) {
            return true;
        }
    }
    return false;
}

size_t reconstruction_cache_entry_count(const ReconstructionCache *cache) {
    size_t count = 0U;
    if (cache == NULL) return 0U;
    for (size_t index = 0U;
         index < RECONSTRUCTION_CACHE_CAPACITY;
         index++) {
        if (cache->entries[index].occupied) count++;
    }
    return count;
}
