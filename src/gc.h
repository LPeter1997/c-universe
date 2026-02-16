/**
 * gc.h is a single-header C garbage collection library.
 *
 * It is designed to be easily embeddable into applications.
 *
 * Configuration:
 *  - TODO
 *
 * API:
 *  - TODO
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef GC_H
#define GC_H

#include <stdbool.h>
#include <stddef.h>

#ifdef GC_STATIC
#   define GC_DEF static
#else
#   define GC_DEF extern
#endif

#ifndef GC_REALLOC
#   define GC_REALLOC realloc
#endif
#ifndef GC_FREE
#   define GC_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct GC_HashBucket;
struct GC_Allocation;

typedef struct GC_World {
    double sweep_factor;
    bool paused;

    // Main data-structure of the opaque allocation elements
    struct {
        struct GC_HashBucket* buckets;
        size_t buckets_capacity;
        size_t entry_count;
    } hash_map;
} GC_World;

GC_DEF void gc_start(GC_World* gc);
GC_DEF void gc_stop(GC_World* gc);
GC_DEF void gc_pause(GC_World* gc);
GC_DEF void gc_resume(GC_World* gc);
GC_DEF void gc_run(GC_World* gc);

GC_DEF void* gc_alloc(GC_World* gc, size_t size);
GC_DEF void* gc_realloc(GC_World* gc, void* mem, size_t size);
GC_DEF void gc_free(GC_World* gc, void* mem);

GC_DEF void gc_pin(GC_World* gc, void* mem);
GC_DEF void gc_unpin(GC_World* gc, void* mem);

#ifdef __cplusplus
}
#endif

#endif /* GC_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef GC_IMPLEMENTATION

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GC_ASSERT(condition, message) assert(((void)message, condition))

enum : uint8_t {
    GC_FLAG_NONE = 0,
    GC_FLAG_MARKED = 1 << 0,
    GC_FLAG_PINNED = 1 << 1,
};

typedef struct GC_Allocation {
    void* base;
    size_t size;
    uint8_t flags;
} GC_Allocation;

// Hash-map ////////////////////////////////////////////////////////////////////

typedef struct GC_HashEntry {
    GC_Allocation allocation;
    uintptr_t hash_code;
} GC_HashEntry;

typedef struct GC_HashBucket {
    GC_HashEntry* entries;
    size_t length;
    size_t capacity;
} GC_HashBucket;

static const double GC_HashTable_UpsizeLoadFactor = 0.75;
static const double GC_HashTable_DownsizeLoadFactor = 0.25;

static uintptr_t gc_hash_code(void* address) {
    return (uintptr_t)address / 8;
}

static double gc_hash_map_load_factor(GC_World* gc) {
    // We handle NULL buckets as max load factor
    if (gc->hash_map.buckets == NULL) return 1;
    return (double)gc->hash_map.entry_count / gc->hash_map.buckets_capacity;
}

static void gc_add_to_hash_bucket(GC_HashBucket* bucket, GC_HashEntry entry) {
    if (bucket->length == bucket->capacity) {
        // Need to resize
        bucket->capacity *= 2;
        if (bucket->capacity < 8) bucket->capacity = 8;
        bucket->entries = (GC_HashEntry*)GC_REALLOC(bucket->entries, sizeof(GC_HashEntry) * bucket->capacity);
        GC_ASSERT(bucket->entries != NULL, "failed to allocate array for GC hash bucket entries");
    }
    bucket->entries[bucket->length++] = entry;
}

static void gc_remove_from_hash_bucket_at(GC_HashBucket* bucket, size_t index) {
    --bucket->length;
    memmove(bucket->entries + index, bucket->entries + index + 1, sizeof(GC_HashEntry) * (bucket->length - index));
}

static void gc_resize_hash_map_to_number_of_buckets(GC_World* gc, size_t newCapacity) {
    if (gc->hash_map.buckets_capacity == newCapacity) return;
    // Allocate a new bucket array
    GC_HashBucket* newBuckets = (GC_HashBucket*)GC_REALLOC(NULL, sizeof(GC_HashBucket) * newCapacity);
    GC_ASSERT(newBuckets != NULL, "failed to allocate bucket array for GC hash map upscaling");
    // Initialize the new bucket arrays
    memset(newBuckets, 0, sizeof(GC_HashBucket) * newCapacity);
    // Add each item from each bucket to the new bucket array, essentially redistributing
    for (size_t i = 0; i < gc->hash_map.buckets_capacity; ++i) {
        GC_HashBucket* oldBucket = &gc->hash_map.buckets[i];
        for (size_t j = 0; j < oldBucket->length; ++j) {
            GC_HashEntry entry = oldBucket->entries[j];
            // Compute the new bucket index
            size_t newBucketIndex = entry.hash_code % newCapacity;
            gc_add_to_hash_bucket(&newBuckets[newBucketIndex], entry);
        }
        // The old bucket's elements have been redistributed, free it up
        GC_FREE(oldBucket->entries);
    }
    // Free the old bucket, replace entries
    GC_FREE(gc->hash_map.buckets);
    gc->hash_map.buckets = newBuckets;
    gc->hash_map.buckets_capacity = newCapacity;
}

static void gc_grow_hash_map(GC_World* gc) {
    // Let's double the size of the bucket array
    size_t newCapacity = gc->hash_map.buckets_capacity * 2;
    if (newCapacity < 8) newCapacity = 8;
    gc_resize_hash_map_to_number_of_buckets(gc, newCapacity);
}

static void gc_shrink_hash_map(GC_World* gc) {
    // Lets halve the size of the bucket array
    size_t newCapacity = gc->hash_map.buckets_capacity / 2;
    if (newCapacity < 8) return;
    gc_resize_hash_map_to_number_of_buckets(gc, newCapacity);
}

static void gc_add_allocation_to_hash_map(GC_World* gc, GC_Allocation allocation) {
    // Grow if needed
    double loadFactor = gc_hash_map_load_factor(gc);
    if (loadFactor > GC_HashTable_UpsizeLoadFactor || gc->hash_map.buckets_capacity == 0) gc_grow_hash_map(gc);

    // Emplace
    GC_HashEntry entry = {
        .allocation = allocation,
        .hash_code = gc_hash_code(allocation.base),
    };
    size_t bucketIndex = entry.hash_code % gc->hash_map.buckets_capacity;
    gc_add_to_hash_bucket(&gc->hash_map.buckets[bucketIndex], entry);
    ++gc->hash_map.entry_count;
}

static void gc_remove_allocation_from_hash_map(GC_World* gc, void* baseAddress) {
    if (gc->hash_map.buckets_capacity == 0) return;

    // First, compute what bucket the element would be in
    size_t bucketIndex = gc_hash_code(baseAddress) % gc->hash_map.buckets_capacity;
    // Look for the index within the bucket
    GC_HashBucket* bucket = &gc->hash_map.buckets[bucketIndex];
    for (size_t i = 0; i < bucket->length; ++i) {
        if (bucket->entries[i].allocation.base == baseAddress) {
            // Found
            gc_remove_from_hash_bucket_at(bucket, i);
            --gc->hash_map.entry_count;
            break;
        }
    }

    // Shrink, if needed
    double loadFactor = gc_hash_map_load_factor(gc);
    if (loadFactor < GC_HashTable_DownsizeLoadFactor) gc_shrink_hash_map(gc);
}

static GC_Allocation* gc_get_allocation_from_hash_map(GC_World* gc, void* baseAddress) {
    if (gc->hash_map.buckets_capacity == 0) return NULL;

    // First, compute what bucket the element would be in
    size_t bucketIndex = gc_hash_code(baseAddress) % gc->hash_map.buckets_capacity;
     // Search within bucket
    GC_HashBucket* bucket = &gc->hash_map.buckets[bucketIndex];
    for (size_t i = 0; i < bucket->length; ++i) {
        GC_Allocation* allocation = &bucket->entries[i].allocation;
        // Found
        if (allocation->base == baseAddress) return allocation;
    }

    // Not found
    return NULL;
}

// Mark and sweep //////////////////////////////////////////////////////////////

static void gc_mark_address(GC_World* gc, void* addr) {
    GC_Allocation* allocation = gc_get_allocation_from_hash_map(gc, addr);
    // If not belonging to an allocation, nothing to do
    if (allocation == NULL) return;
    // If already marked, nothing to do
    if ((allocation->flags & GC_FLAG_MARKED) != 0) return;
    // First off, mark
    allocation->flags |= GC_FLAG_MARKED;
    // Then we can mark each address within this allocated region
    uint8_t* startAddress = (uint8_t*)allocation->base;
    // NOTE: We make sure to only read addresses fully within
    uint8_t* endAddress = startAddress + allocation->size - (allocation->size % sizeof(uintptr_t));
    // Align start address
    uintptr_t misalign = (uintptr_t)startAddress % sizeof(uintptr_t);
    if (misalign > 0) startAddress += sizeof(uintptr_t) - misalign;
    // Mark every address referenced within
    for (uintptr_t* addr = (uintptr_t*)startAddress; addr < (uintptr_t*)endAddress; ++addr) {
        void* referencedAddr = (void*)*addr;
        gc_mark_address(gc, referencedAddr);
    }
}

static void gc_mark(GC_World* gc) {
    gc_mark_pinned(gc);
    gc_mark_stack(gc);
    gc_mark_globals(gc);
}

#undef GC_ASSERT

#endif /* GC_IMPLEMENTATION */
