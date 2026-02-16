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

#ifndef GC_REALLOC_INTERNAL
#   define GC_REALLOC_INTERNAL realloc
#endif
#ifndef GC_FREE_INTERNAL
#   define GC_FREE_INTERNAL free
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GC_AddressMode {
    GC_ADDRESS_MODE_EXACT = 0,
    GC_ADDRESS_MODE_CONTAIN = 1,
} GC_AddressMode;

typedef struct GC_World {
    GC_AddressMode address_mode;
    double sweep_factor;
    // Main data-structure of the opaque allocation elements
    union {
        // Used for exact address matching
        struct {
            void* buckets;
            size_t buckets_length;
            size_t occupied_buckets_length;
            double upsize_load_factor;
            double downsize_load_factor;
        } __hash_map;
        // Used for address containment matching
        struct {
            void* items;
            size_t items_length;
            size_t items_capacity;
            double downsize_capacity_factor;
        } __interval_map;
    };
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

#define GC_ASSERT(condition, message) assert(((void)message, condition))

enum : uint8_t {
    GC_FLAG_NONE = 0,
    GC_FLAG_CREATED = 1 << 0,
    GC_FLAG_MARKED = 2 << 0,
    GC_FLAG_PINNED = 3 << 0,
};

typedef struct GC_Allocation {
    void* base;
    size_t size;
    uint8_t flags;
} GC_Allocation;

typedef struct GC_HashBucket {
    GC_Allocation* allocations;
    size_t length;
    size_t capacity;
} GC_HashBucket;

// static void __gc_add_allocation(GC_World* gc, )

#undef GC_ASSERT

#endif /* GC_IMPLEMENTATION */
