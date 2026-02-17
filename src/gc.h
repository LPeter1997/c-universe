/**
 * gc.h is a single-header C mark-and-sweep garbage collection library.
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

#ifndef GC_LOG
#   define GC_LOG(...)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct GC_HashBucket;

typedef struct GC_World {
    double sweep_factor;
    bool paused;

    // Main data-structure of the opaque allocation elements
    struct {
        struct GC_HashBucket* buckets;
        size_t buckets_length;
        size_t entry_count;
    } hash_map;

    // Sections of global data that needs scanning
    struct {
        // Layout is [start1_inclusive, end1_exclusive, start2_inclusive, end2_exclusive, ...]
        void** endpoints;
        // The number of endpoint PAIRS stored
        size_t length;
    } global_sections;

    // Bottom of the stack
    void* stack_bottom;
} GC_World;

GC_DEF void gc_start(GC_World* gc);
GC_DEF void gc_stop(GC_World* gc);
GC_DEF void gc_pause(GC_World* gc);
GC_DEF void gc_resume(GC_World* gc);
GC_DEF void gc_run(GC_World* gc);

GC_DEF void gc_pin(GC_World* gc, void* mem);
GC_DEF void gc_unpin(GC_World* gc, void* mem);

GC_DEF void* gc_alloc(GC_World* gc, size_t size);
GC_DEF void* gc_realloc(GC_World* gc, void* mem, size_t size);
GC_DEF void gc_free(GC_World* gc, void* mem);

#ifdef __cplusplus
}
#endif

#endif /* GC_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef GC_IMPLEMENTATION

#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GC_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

enum : uint8_t {
    GC_FLAG_NONE = 0,
    GC_FLAG_MARKED = 1 << 0,
    GC_FLAG_PINNED = 1 << 1,
};

typedef struct GC_Allocation {
    void* base_address;
    size_t size;
    uint8_t flags;
} GC_Allocation;

static void gc_add_global_section(GC_World* gc, char const* name, void* start, void* end) {
    size_t addIndex = gc->global_sections.length * 2;
    ++gc->global_sections.length;
    gc->global_sections.endpoints = (void**)GC_REALLOC(gc->global_sections.endpoints, sizeof(void*) * gc->global_sections.length * 2);
    GC_ASSERT(gc->global_sections.endpoints != NULL, "failed to reallocate section array");
    gc->global_sections.endpoints[addIndex + 0] = start;
    gc->global_sections.endpoints[addIndex + 1] = end;
    GC_LOG("global section '%s' added (start: %p, end: %p)", name, start, end);
}

// Platform-specific ///////////////////////////////////////////////////////////

#if defined(_WIN32)
// NOTE: For now this ties us to Windows 8 or later, maybe look into eliminating it
// We need this for the stack size
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0602
#include <Windows.h>
#include <processthreadsapi.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

static void* gc_compute_stack_bottom() {
#if defined(_WIN32)
    ULONG_PTR low, high;
    GetCurrentThreadStackLimits(&low, &high);
    return (void*)low;
#elif defined(__linux__) || defined(__APPLE__)
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    size_t stack_size;
    void* stack_addr;
    pthread_attr_getstack(&attr, &stack_addr, &stack_size);
    void* stack_bottom = (char*)stack_addr + stack_size; // top of stack memory
    pthread_attr_destroy(&attr);
    return stack_bottom;
#else
    #error "unsupported platform"
#endif
}

#if defined(_WIN32)
    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#elif defined(__linux__) || defined(__APPLE__)
    extern char __data_start;
    extern char __data_end;
    extern char __bss_start;
    extern char __bss_end;
#endif

static void gc_collect_global_sections(GC_World* gc) {
#if defined(_WIN32)
    // Base of the module (executable or DLL)
    uintptr_t base = (uintptr_t)&__ImageBase;
    // Get the DOS header
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    // Iterate sections
    IMAGE_SECTION_HEADER* sectionHeaders = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        IMAGE_SECTION_HEADER* s = &sectionHeaders[i];
        // Look for .data or .bss (uninitialized data is usually in .bss)
        if ((memcmp(s->Name, ".data", 5) == 0)
         || (memcmp(s->Name, ".bss", 4) == 0)) {
            uintptr_t start = base + s->VirtualAddress;
            uintptr_t end = start + s->Misc.VirtualSize;
            gc_add_global_section(gc, (char const*)s->Name, (void*)start, (void*)end);
        }
    }
#elif defined(__linux__) || defined(__APPLE__)
    gc_add_global_section(gc, ".data", (void*)__data_start, (void*)__data_end);
    gc_add_global_section(gc, ".bss", (void*)__bss_start, (void*)__bss_end);
#else
    #error "unsupported platform"
#endif
}

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
    return (double)gc->hash_map.entry_count / gc->hash_map.buckets_length;
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

static void gc_resize_hash_map_to_number_of_buckets(GC_World* gc, size_t newLength) {
    if (gc->hash_map.buckets_length == newLength) return;
    // Allocate a new bucket array
    GC_HashBucket* newBuckets = (GC_HashBucket*)GC_REALLOC(NULL, sizeof(GC_HashBucket) * newLength);
    GC_ASSERT(newBuckets != NULL, "failed to allocate bucket array for GC hash map upscaling");
    // Initialize the new bucket arrays
    memset(newBuckets, 0, sizeof(GC_HashBucket) * newLength);
    // Add each item from each bucket to the new bucket array, essentially redistributing
    for (size_t i = 0; i < gc->hash_map.buckets_length; ++i) {
        GC_HashBucket* oldBucket = &gc->hash_map.buckets[i];
        for (size_t j = 0; j < oldBucket->length; ++j) {
            GC_HashEntry entry = oldBucket->entries[j];
            // Compute the new bucket index
            size_t newBucketIndex = entry.hash_code % newLength;
            gc_add_to_hash_bucket(&newBuckets[newBucketIndex], entry);
        }
        // The old bucket's elements have been redistributed, free it up
        GC_FREE(oldBucket->entries);
    }
    // Free the old bucket, replace entries
    GC_FREE(gc->hash_map.buckets);
    gc->hash_map.buckets = newBuckets;
    gc->hash_map.buckets_length = newLength;
}

static void gc_grow_hash_map(GC_World* gc) {
    // Let's double the size of the bucket array
    size_t newLength = gc->hash_map.buckets_length * 2;
    if (newLength < 8) newLength = 8;
    gc_resize_hash_map_to_number_of_buckets(gc, newLength);
}

static void gc_shrink_hash_map(GC_World* gc) {
    // Lets halve the size of the bucket array
    size_t newLength = gc->hash_map.buckets_length / 2;
    if (newLength < 8) return;
    gc_resize_hash_map_to_number_of_buckets(gc, newLength);
}

static void gc_add_allocation_to_hash_map(GC_World* gc, GC_Allocation allocation) {
    // Grow if needed
    double loadFactor = gc_hash_map_load_factor(gc);
    if (loadFactor > GC_HashTable_UpsizeLoadFactor || gc->hash_map.buckets_length == 0) gc_grow_hash_map(gc);

    // Emplace
    GC_HashEntry entry = {
        .allocation = allocation,
        .hash_code = gc_hash_code(allocation.base_address),
    };
    size_t bucketIndex = entry.hash_code % gc->hash_map.buckets_length;
    gc_add_to_hash_bucket(&gc->hash_map.buckets[bucketIndex], entry);
    ++gc->hash_map.entry_count;
}

static void gc_remove_allocation_from_hash_map(GC_World* gc, void* baseAddress) {
    if (gc->hash_map.buckets_length == 0) return;

    // First, compute what bucket the element would be in
    size_t bucketIndex = gc_hash_code(baseAddress) % gc->hash_map.buckets_length;
    // Look for the index within the bucket
    GC_HashBucket* bucket = &gc->hash_map.buckets[bucketIndex];
    for (size_t i = 0; i < bucket->length; ++i) {
        if (bucket->entries[i].allocation.base_address == baseAddress) {
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
    if (gc->hash_map.buckets_length == 0) return NULL;

    // First, compute what bucket the element would be in
    size_t bucketIndex = gc_hash_code(baseAddress) % gc->hash_map.buckets_length;
     // Search within bucket
    GC_HashBucket* bucket = &gc->hash_map.buckets[bucketIndex];
    for (size_t i = 0; i < bucket->length; ++i) {
        GC_Allocation* allocation = &bucket->entries[i].allocation;
        // Found
        if (allocation->base_address == baseAddress) return allocation;
    }

    // Not found
    return NULL;
}

// Mark ////////////////////////////////////////////////////////////////////////

static void* gc_align_address_forward(uint8_t* address) {
    uintptr_t misalign = (uintptr_t)address % sizeof(uintptr_t);
    if (misalign > 0) address += sizeof(uintptr_t) - misalign;
    return address;
}

static uint8_t* gc_align_address_backward(uint8_t* address) {
    return address - (uintptr_t)address % sizeof(uintptr_t);
}

static void gc_mark_address(GC_World* gc, void* addr);

static void gc_mark_values_in_address_range(GC_World* gc, void* startAddress, void* endAddress) {
    startAddress = gc_align_address_forward(startAddress);
    endAddress = gc_align_address_backward(endAddress);
    for (uintptr_t* addr = (uintptr_t*)startAddress; addr < (uintptr_t*)endAddress; ++addr) {
        void* referencedAddr = (void*)*addr;
        gc_mark_address(gc, referencedAddr);
    }
}

static void gc_mark_address(GC_World* gc, void* addr) {
    GC_Allocation* allocation = gc_get_allocation_from_hash_map(gc, addr);
    // If not belonging to an allocation, nothing to do
    if (allocation == NULL) return;
    // If already marked, nothing to do
    if ((allocation->flags & GC_FLAG_MARKED) != 0) return;
    // First off, mark
    allocation->flags |= GC_FLAG_MARKED;
    // Then we can mark each address within this allocated region
    gc_mark_values_in_address_range(gc, (uint8_t*)allocation->base_address, (uint8_t*)allocation->base_address + allocation->size);
}

static void gc_mark_pinned(GC_World* gc) {
    // Simply enumerate the allocations and mark everything with the pinned flag
    for (size_t i = 0; i < gc->hash_map.buckets_length; ++i) {
        GC_HashBucket* bucket = &gc->hash_map.buckets[i];
        for (size_t j = 0; j < bucket->length; ++j) {
            GC_Allocation* allocation = &bucket->entries[j].allocation;
            if ((allocation->flags & GC_FLAG_PINNED) != 0) gc_mark_address(gc, allocation->base_address);
        }
    }
}

static void gc_mark_stack(GC_World* gc) {
    // Getting stack top + spilling
    jmp_buf env;
    void* stackTop = NULL;
    // spill registers into jmp_buf
    if (setjmp(env) == 0) {
        stackTop = (void*)&env;
    }

    GC_ASSERT(stackTop != NULL, "failed to retrieve stack top, not scanning stack is fatal");

    if (stackTop > gc->stack_bottom) {
        // Stack grows down
        GC_LOG("scanning downwards-growing stack (start: %p, end: %p)", gc->stack_bottom, stackTop);
        gc_mark_values_in_address_range(gc, gc->stack_bottom, stackTop);
    }
    else {
        // Stack grows up
        GC_LOG("scanning upwards-growing stack (start: %p, end: %p)", stackTop, gc->stack_bottom);
        gc_mark_values_in_address_range(gc, stackTop, gc->stack_bottom);
    }
}

static void gc_mark_globals(GC_World* gc) {
    GC_LOG("TODO: gc_mark_globals");
}

static void gc_mark(GC_World* gc) {
    gc_mark_pinned(gc);
    gc_mark_stack(gc);
    gc_mark_globals(gc);
}

// API functions ///////////////////////////////////////////////////////////////

void gc_start(GC_World* gc) {
    gc->stack_bottom = gc_compute_stack_bottom();
    gc_collect_global_sections(gc);
}

void gc_stop(GC_World* gc) {
    GC_LOG("TODO: gc_stop");
}

void gc_pause(GC_World* gc) {
    GC_LOG("TODO: gc_pause");
}

void gc_resume(GC_World* gc) {
    GC_LOG("TODO: gc_resume");
}

void gc_run(GC_World* gc) {
    gc_mark(gc);
    GC_LOG("TODO: gc_run rest");
}

void gc_pin(GC_World* gc, void* mem) {
    GC_LOG("TODO: gc_pin");
}

void gc_unpin(GC_World* gc, void* mem) {
    GC_LOG("TODO: gc_unpin");
}

void* gc_alloc(GC_World* gc, size_t size) {
    GC_LOG("TODO: gc_alloc");
}

void* gc_realloc(GC_World* gc, void* mem, size_t size) {
    GC_LOG("TODO: gc_realloc");
}

void gc_free(GC_World* gc, void* mem) {
    GC_LOG("TODO: gc_free");
}

#ifdef __cplusplus
}
#endif

#undef GC_ASSERT

#endif /* GC_IMPLEMENTATION */
