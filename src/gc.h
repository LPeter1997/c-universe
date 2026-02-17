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
struct GC_GlobalSection;

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
        struct GC_GlobalSection* sections;
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

enum {
    GC_FLAG_NONE = 0,
    GC_FLAG_MARKED = 1 << 0,
    GC_FLAG_PINNED = 1 << 1,
};

typedef struct GC_Allocation {
    void* base_address;
    size_t size;
    uint8_t flags;
} GC_Allocation;

typedef struct GC_GlobalSection {
    char const* name;
    void* start;
    void* end;
} GC_GlobalSection;

static void gc_add_global_section(GC_World* gc, GC_GlobalSection section) {
    ++gc->global_sections.length;
    gc->global_sections.sections = (GC_GlobalSection*)GC_REALLOC(gc->global_sections.sections, sizeof(GC_GlobalSection) * gc->global_sections.length);
    GC_ASSERT(gc->global_sections.sections != NULL, "failed to reallocate section array");
    gc->global_sections.sections[gc->global_sections.length - 1] = section;
    GC_LOG("global section '%s' added (start: %p, end: %p)", section.name, section.start, section.end);
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
    return (void*)high;
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
            gc_add_global_section(gc, (GC_GlobalSection){
                .name = (char const*)s->Name,
                .start = (void*)start,
                .end = (void*)end,
            });
        }
    }
#elif defined(__linux__) || defined(__APPLE__)
    gc_add_global_section(gc, (GC_GlobalSection){
        .name = ".data",
        .start = (void*)__data_start,
        .end = (void*)__data_end,
    });
    gc_add_global_section(gc, (GC_GlobalSection){
        .name = ".bss",
        .start = (void*)__bss_start,
        .end = (void*)__bss_end,
    });
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

static void gc_resize_hash_map(GC_World* gc, size_t newLength) {
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
    gc_resize_hash_map(gc, newLength);
}

static void gc_shrink_hash_map(GC_World* gc) {
    // Lets halve the size of the bucket array
    size_t newLength = gc->hash_map.buckets_length / 2;
    if (newLength < 8) return;
    gc_resize_hash_map(gc, newLength);
}

static void gc_shrink_hash_map_if_needed(GC_World* gc) {
    double loadFactor = gc_hash_map_load_factor(gc);
    if (loadFactor < GC_HashTable_DownsizeLoadFactor) gc_shrink_hash_map(gc);
}

static void gc_add_to_hash_map(GC_World* gc, GC_Allocation allocation) {
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

static bool gc_remove_from_hash_map(GC_World* gc, void* baseAddress, GC_Allocation* outAllocation) {
    if (gc->hash_map.buckets_length == 0) return false;

    // First, compute what bucket the element would be in
    size_t bucketIndex = gc_hash_code(baseAddress) % gc->hash_map.buckets_length;
    // Look for the index within the bucket
    GC_HashBucket* bucket = &gc->hash_map.buckets[bucketIndex];
    bool found = false;
    for (size_t i = 0; i < bucket->length; ++i) {
        GC_Allocation* allocation = &bucket->entries[i].allocation;
        if (allocation->base_address == baseAddress) {
            // Found
            gc_remove_from_hash_bucket_at(bucket, i);
            --gc->hash_map.entry_count;
            if (outAllocation != NULL) *outAllocation = *allocation;
            found = true;
            break;
        }
    }

    if (!found) return false;

    gc_shrink_hash_map_if_needed(gc);
    return true;
}

static GC_Allocation* gc_get_from_hash_map(GC_World* gc, void* baseAddress) {
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
    GC_Allocation* allocation = gc_get_from_hash_map(gc, addr);
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
            if ((allocation->flags & GC_FLAG_PINNED) != 0) {
                GC_LOG("marking pinned allocation (base: %p, size: %zu)", allocation->base_address, allocation->size);
                gc_mark_address(gc, allocation->base_address);
            }
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

    GC_ASSERT(stackTop != NULL, "failed to retrieve stack top, not marking stack is fatal");

    if (gc->stack_bottom > stackTop) {
        // Stack grows down
        GC_LOG("marking downwards-growing stack (start: %p, end: %p)", stackTop, gc->stack_bottom);
        gc_mark_values_in_address_range(gc, stackTop, gc->stack_bottom);
    }
    else {
        // Stack grows up
        GC_LOG("marking upwards-growing stack (start: %p, end: %p)", gc->stack_bottom, stackTop);
        gc_mark_values_in_address_range(gc, gc->stack_bottom, stackTop);
    }
}

static void gc_mark_globals(GC_World* gc) {
    for (size_t i = 0; i < gc->global_sections.length; ++i) {
        GC_GlobalSection* section = &gc->global_sections.sections[i];
        GC_LOG("scanning global section '%s' (start: %p, end: %p)", section->name, section->start, section->end);
        gc_mark_values_in_address_range(gc, section->start, section->end);
    }
}

static void gc_mark(GC_World* gc) {
    GC_LOG("starting mark phase");
    gc_mark_pinned(gc);
    gc_mark_stack(gc);
    gc_mark_globals(gc);
    GC_LOG("mark phase completed");
}

// Sweep ///////////////////////////////////////////////////////////////////////

void gc_sweep(GC_World* gc) {
    // We go through the entries, whatever is marked we just unflag and whatever was not marked is freed
    for (size_t i = 0; i < gc->hash_map.buckets_length; ++i) {
        GC_HashBucket* bucket = &gc->hash_map.buckets[i];
        for (size_t j = 0; j < bucket->length; ) {
            GC_Allocation* allocation = &bucket->entries[j].allocation;
            if ((allocation->flags & GC_FLAG_MARKED) != 0) {
                // Marked, don't free, just clear flag
                allocation->flags &= ~GC_FLAG_MARKED;
                ++j;
                continue;
            }
            // Entry is unmarked, we need to free it
            GC_LOG("sweep found unmarked allocation (address: %p size: %zu), freeing it", allocation->base_address, allocation->size);
            GC_FREE(allocation->base_address);
            // NOTE: We are allowed to remove like this, this won't trigger shrinking
            gc_remove_from_hash_bucket_at(bucket, j);
            // Since we removed, we are off-by-one, don't increment j
        }
    }

    // Compact hash map
    gc_shrink_hash_map_if_needed(gc);
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
    GC_LOG("pausing garbage collection");
    gc->paused = true;
}

void gc_resume(GC_World* gc) {
    GC_LOG("resuming garbage collection");
    gc->paused = false;
}

void gc_run(GC_World* gc) {
    gc_mark(gc);
    // TODO: We shouldn't always sweep
    gc_sweep(gc);
}

void gc_pin(GC_World* gc, void* mem) {
    GC_LOG("TODO: gc_pin");
}

void gc_unpin(GC_World* gc, void* mem) {
    GC_LOG("TODO: gc_unpin");
}

void* gc_alloc(GC_World* gc, size_t size) {
    void* mem = GC_REALLOC(NULL, size);
    if (mem == NULL) {
        GC_LOG("gc_alloc failed to allocate memory of size %zu", size);
        return NULL;
    }

    GC_LOG("allocated memory (address: %p, size: %zu)", mem, size);
    GC_Allocation allocation = {
        .base_address = mem,
        .size = size,
        .flags = GC_FLAG_NONE,
    };
    gc_add_to_hash_map(gc, allocation);
    return mem;
}

void* gc_realloc(GC_World* gc, void* mem, size_t size) {
    if (mem == NULL) return gc_alloc(gc, size);

    GC_Allocation* allocation = gc_get_from_hash_map(gc, mem);
    if (allocation == NULL) {
        GC_LOG("gc_realloc called with address %p, has no corresponding allocation", mem);
        return NULL;
    }
    size_t oldSize = allocation->size;

    // Call out to reallocation
    void* reMem = GC_REALLOC(mem, size);
    if (reMem == NULL) {
        GC_LOG("gc_realloc with memory %p failed to reallocate memory of size %zu to %zu", mem, oldSize, size);
        return NULL;
    }

    if (reMem == mem) {
        // We are in luck, we can just resize the allocation
        GC_LOG("gc_realloc called with address %p was resized in-place from %zu to %zu", mem, oldSize, size);
        allocation->size = size;
        return mem;
    }

    // We need to remove and readd
    // NOTE: We could optimize by getting the bucket index and element index from the getter
    // but that feels a bit of an overkill
    gc_remove_from_hash_map(gc, mem, NULL);
    GC_LOG("reallocated memory (old address: %p, old size: %zu, new address: %p, new size: %zu)", mem, oldSize, reMem, size);
    GC_Allocation newAllocation = {
        .base_address = reMem,
        .size = size,
        .flags = GC_FLAG_NONE,
    };
    gc_add_to_hash_map(gc, newAllocation);
    return reMem;
}

void gc_free(GC_World* gc, void* mem) {
    GC_Allocation removedAllocation;
    if (!gc_remove_from_hash_map(gc, mem, &removedAllocation)) {
        GC_LOG("gc_free called with address %p has no corresponding allocation", mem);
        return;
    }

    GC_LOG("maunally freeing allocation (address: %p, size: %zu)", removedAllocation.base_address, removedAllocation.size);
    GC_FREE(removedAllocation.base_address);
}

#ifdef __cplusplus
}
#endif

#undef GC_ASSERT

#endif /* GC_IMPLEMENTATION */
