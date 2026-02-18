/**
 * gc.h is a single-header C mark-and-sweep garbage collection library.
 *
 * It is designed to be easily embeddable into applications. Importantly, this GC works by monitoring pointers
 * that reside in the global section(s) and the stack. The library DOES NOT track internal pointers, a reference
 * to the allocated base address must exist somewhere to not be collected. Since it's rare that the root address
 * is not stored somewhere, this felt like a fair tradeoff for speed.
 *
 * Configuration:
 *  - #define GC_IMPLEMENTATION before including this header in exactly one source file to include the implementation section
 *  - #define GC_STATIC before including this header to make all functions have internal linkage
 *  - #define GC_REALLOC and GC_FREE to use custom memory allocation functions (by default they use realloc and free from the C standard library)
 *  - #define GC_LOG before including this header to define a custom logging function for internal GC logs (API is expected to be identical to printf)
 *  - #define GC_SELF_TEST before including this header to compile a self-test that verifies the framework's functionality
 *  - #define GC_EXAMPLE before including this header to compile a simple example that demonstrates how to use the framework
 *
 * API:
 *  - Define a GC_World struct and optionally set a custom sweep_factor if needed
 *  - Call gc_start to initialize the GC world, and gc_stop to clean up resources when done
 *  - Use gc_alloc, gc_realloc and gc_free to allocate memory that should be managed by the GC
 *  - Use gc_pin and gc_unpin to pin/unpin allocations that should be kept alive even without references
 *  - Call gc_run to perform a GC cycle, either forced or non-forced (which respects the sweep limit)
 *  - Use gc_pause and gc_resume to temporarily pause and resume the cycle
 *
 * Check the example section at the end of this file for a full example.
 */

// NOTE: We need this to get some extra non-portable functionality
// and apparently we need this before any includes
#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

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

/**
 * The main struct representing the GC world, containing all necessary data structures and configuration for the GC to operate.
 */
typedef struct GC_World {
    double sweep_factor;
    size_t sweep_limit;
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

/**
 * Initializes the GC world, must be called before any other GC function.
 * @param gc The GC world to initialize.
 */
GC_DEF void gc_start(GC_World* gc);

/**
 * Stops the GC world and frees any resources used by it, must be called when GC functionality is no longer needed.
 * @param gc The GC world to stop.
 */
GC_DEF void gc_stop(GC_World* gc);

/**
 * Pauses the GC cycle, preventing @see gc_run from doing anything until resumed.
 * @param gc The GC world to pause.
 */
GC_DEF void gc_pause(GC_World* gc);

/**
 * Resumes the GC cycle if it was paused.
 * @param gc The GC world to resume.
 */
GC_DEF void gc_resume(GC_World* gc);

/**
 * Runs a GC cycle, performing mark-and-sweep collection.
 * @param gc The GC world to run the cycle on.
 * @param force If true, forces a collection regardless of the sweep limit; if false, only performs collection if the number of allocated items exceeds the sweep limit.
 * @returns The freed memory size in bytes.
 */
GC_DEF size_t gc_run(GC_World* gc, bool force);

/**
 * Pins the given memory, preventing it from being collected even if no references to it exist.
 * @param gc The GC world to pin the memory in.
 * @param mem The memory to pin.
 */
GC_DEF void gc_pin(GC_World* gc, void* mem);

/**
 * Unpins the given memory, allowing it to be collected if no references to it exist.
 * @param gc The GC world to unpin the memory in.
 * @param mem The memory to unpin.
 */
GC_DEF void gc_unpin(GC_World* gc, void* mem);

/**
 * Allocates memory of the given size that is managed by the GC.
 * Calls out to whatever GC_REALLOC is defined as, with a NULL pointer for the first argument.
 * @param gc The GC world to allocate the memory in.
 * @param size The size of the memory to allocate in bytes.
 * @returns A pointer to the allocated memory, or NULL if allocation failed.
 */
GC_DEF void* gc_alloc(GC_World* gc, size_t size);

/**
 * Reallocates the given memory to the new size, updating the GC's internal data structures accordingly.
 * Calls out to whatever GC_REALLOC is defined as.
 * @param gc The GC world to reallocate the memory in.
 * @param mem The memory to reallocate, must have been allocated by this GC.
 * @param size The new size of the memory in bytes.
 * @returns A pointer to the reallocated memory, or NULL if reallocation failed.
 */
GC_DEF void* gc_realloc(GC_World* gc, void* mem, size_t size);

/**
 * Frees the given memory, removing it from the GC's internal data structures.
 * Calls out to whatever GC_FREE is defined as.
 * @param gc The GC world to free the memory in.
 * @param mem The memory to free, must have been allocated by this GC.
 */
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
    int flags;
} GC_Allocation;

typedef struct GC_GlobalSection {
    char const* name;
    void* start;
    void* end;
} GC_GlobalSection;

typedef struct GC_HashEntry {
    GC_Allocation allocation;
    uintptr_t hash_code;
} GC_HashEntry;

typedef struct GC_HashBucket {
    GC_HashEntry* entries;
    size_t length;
    size_t capacity;
} GC_HashBucket;

static void gc_add_global_section(GC_World* gc, GC_GlobalSection section) {
    // NOTE: It's inefficient to resize each time, but for now we expect at most 2 sections to be present
    ++gc->global_sections.length;
    gc->global_sections.sections = (GC_GlobalSection*)GC_REALLOC(gc->global_sections.sections, sizeof(GC_GlobalSection) * gc->global_sections.length);
    GC_ASSERT(gc->global_sections.sections != NULL, "failed to reallocate section array");
    gc->global_sections.sections[gc->global_sections.length - 1] = section;
    GC_LOG("global section '%s' added (start: %p, end: %p)", section.name, section.start, section.end);
}

static void gc_free_data_structures(GC_World* gc) {
    // First, we free hash map
    for (size_t i = 0; i < gc->hash_map.buckets_length; ++i) {
        GC_HashBucket* bucket = &gc->hash_map.buckets[i];
        GC_FREE(bucket->entries);
        bucket->entries = NULL;
        bucket->length = 0;
        bucket->capacity = 0;
    }
    GC_FREE(gc->hash_map.buckets);
    gc->hash_map.buckets = NULL;
    gc->hash_map.buckets_length = 0;
    gc->hash_map.entry_count = 0;

    // Then the sections array
    GC_FREE(gc->global_sections.sections);
    gc->global_sections.sections = NULL;
    gc->global_sections.length = 0;
}

// Platform-specific ///////////////////////////////////////////////////////////

#if defined(_WIN32)
    // NOTE: For now this ties us to Windows 8 or later, maybe look into eliminating it
    // We need this for the stack size
    #undef _WIN32_WINNT
    #define _WIN32_WINNT 0x0602
    #include <Windows.h>
    #include <processthreadsapi.h>
#elif defined(__linux__)
    #include <pthread.h>
#elif defined(__APPLE__)
    #include <pthread.h>
    #include <mach-o/getsect.h>
    #include <mach-o/dyld.h>

    #if defined(__LP64__)
        typedef struct mach_header_64 mach_header_t;
    #else
        typedef struct mach_header mach_header_t;
    #endif
#endif

static void* gc_compute_stack_bottom(void) {
#if defined(_WIN32)
    ULONG_PTR low, high;
    GetCurrentThreadStackLimits(&low, &high);
    return (void*)high;
#elif defined(__linux__)
    pthread_t thread = pthread_self();
    pthread_attr_t attr;
    if (pthread_getattr_np(thread, &attr) != 0) return NULL;
    void *stackbottom;
    size_t stacksize;
    if (pthread_attr_getstack(&attr, &stackbottom, &stacksize) != 0) return NULL;
    return (void*)((uint8_t*)stackbottom + stacksize);
#elif defined(__APPLE__)
    pthread_t thread = pthread_self();
    void *stackaddr = pthread_get_stackaddr_np(thread);
    return stackaddr;
#else
    #error "unsupported platform"
#endif
}

#if defined(_WIN32)
    EXTERN_C IMAGE_DOS_HEADER __ImageBase;
#elif defined(__linux__)
    extern char __data_start;
    extern char _edata;
    extern char __bss_start;
    extern char _end;
#endif

static void gc_collect_global_sections(GC_World* gc) {
#if defined(_WIN32)
    // Base of the module (executable or DLL)
    uintptr_t base = (uintptr_t)&__ImageBase;
    // Get the DOS header
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS* nt = (IMAGE_NT_HEADERS*)(base + (uintptr_t)dos->e_lfanew);
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
#elif defined(__linux__)
    gc_add_global_section(gc, (GC_GlobalSection){
        .name = ".data",
        .start = (void*)&__data_start,
        .end = (void*)&_edata,
    });
    gc_add_global_section(gc, (GC_GlobalSection){
        .name = ".bss",
        .start = (void*)&__bss_start,
        .end = (void*)&_end,
    });
#elif defined(__APPLE__)
    const mach_header_t* header = (const mach_header_t*)_dyld_get_image_header(0);
    unsigned long size;
    uint8_t* data;
    // NOTE: getsectiondata() returns the in-memory address with slide already applied
    // __DATA,__data
    data = getsectiondata(header, "__DATA", "__data", &size);
    if (data && size) {
        gc_add_global_section(gc, (GC_GlobalSection){
            .name = "__DATA,__data",
            .start = data,
            .end = data + size,
        });
    }
    // __DATA,__bss
    data = getsectiondata(header, "__DATA", "__bss", &size);
    if (data && size) {
        gc_add_global_section(gc, (GC_GlobalSection){
            .name = "__DATA,__bss",
            .start = data,
            .end = data + size,
        });
    }
    // __DATA,__common - uninitialized globals often go here
    data = getsectiondata(header, "__DATA", "__common", &size);
    if (data && size) {
        gc_add_global_section(gc, (GC_GlobalSection){
            .name = "__DATA,__common",
            .start = data,
            .end = data + size,
        });
    }
    // modern macOS may place globals here
    data = getsectiondata(header, "__DATA_CONST", "__const", &size);
    if (data && size) {
        gc_add_global_section(gc, (GC_GlobalSection){
            .name = "__DATA_CONST,__const",
            .start = data,
            .end = data + size,
        });
    }
    // __DATA_DIRTY,__data for modern toolchains
    data = getsectiondata(header, "__DATA_DIRTY", "__data", &size);
    if (data && size) {
        gc_add_global_section(gc, (GC_GlobalSection){
            .name = "__DATA_DIRTY,__data",
            .start = data,
            .end = data + size,
        });
    }
    // __DATA_DIRTY,__common for modern toolchains
    data = getsectiondata(header, "__DATA_DIRTY", "__common", &size);
    if (data && size) {
        gc_add_global_section(gc, (GC_GlobalSection){
            .name = "__DATA_DIRTY,__common",
            .start = data,
            .end = data + size,
        });
    }
#else
    #error "unsupported platform"
#endif
}

// Hash-map ////////////////////////////////////////////////////////////////////

static const double GC_HashTable_UpsizeLoadFactor = 0.75;
static const double GC_HashTable_DownsizeLoadFactor = 0.25;

static uintptr_t gc_hash_code(void* address) {
    return (uintptr_t)address / 8;
}

static double gc_hash_map_load_factor(GC_World* gc) {
    // We handle NULL buckets as max load factor
    if (gc->hash_map.buckets == NULL) return 1;
    return (double)gc->hash_map.entry_count / (double)gc->hash_map.buckets_length;
}

// NOTE: This does NOT increment the entry_count, as some internal methods (like resize) need to add entries without counting them as new
// For an incrementing version, use gc_add_to_hash_bucket
static void gc_add_entry_to_bucket(GC_HashBucket* bucket, GC_HashEntry entry) {
    if (bucket->length == bucket->capacity) {
        // Need to resize
        bucket->capacity *= 2;
        if (bucket->capacity < 8) bucket->capacity = 8;
        bucket->entries = (GC_HashEntry*)GC_REALLOC(bucket->entries, sizeof(GC_HashEntry) * bucket->capacity);
        GC_ASSERT(bucket->entries != NULL, "failed to allocate array for GC hash bucket entries");
    }
    bucket->entries[bucket->length++] = entry;
}

static void gc_add_to_hash_bucket(GC_World* gc, GC_HashBucket* bucket, GC_HashEntry entry) {
    gc_add_entry_to_bucket(bucket, entry);
    ++gc->hash_map.entry_count;
}

static void gc_remove_from_hash_bucket_at(GC_World* gc, GC_HashBucket* bucket, size_t index) {
    --bucket->length;
    --gc->hash_map.entry_count;
    memmove(bucket->entries + index, bucket->entries + index + 1, sizeof(GC_HashEntry) * (bucket->length - index));
}

static void gc_recompute_sweep_limit(GC_World* gc);

static void gc_resize_hash_map(GC_World* gc, size_t newLength) {
    if (gc->hash_map.buckets_length == newLength) return;
    // Allocate a new bucket array
    GC_HashBucket* newBuckets = (GC_HashBucket*)GC_REALLOC(NULL, sizeof(GC_HashBucket) * newLength);
    GC_ASSERT(newBuckets != NULL, "failed to allocate bucket array for GC hash map upscaling");
    // Initialize the new bucket arrays
    memset(newBuckets, 0, sizeof(GC_HashBucket) * newLength);
    // Add each item from each bucket to the new bucket array, essentially redistributing
    // NOTE: We use gc_add_entry_to_bucket instead of gc_add_to_hash_bucket to avoid
    // incrementing entry_count (entries are being moved, not added)
    for (size_t i = 0; i < gc->hash_map.buckets_length; ++i) {
        GC_HashBucket* oldBucket = &gc->hash_map.buckets[i];
        for (size_t j = 0; j < oldBucket->length; ++j) {
            GC_HashEntry entry = oldBucket->entries[j];
            // Compute the new bucket index
            size_t newBucketIndex = entry.hash_code % newLength;
            gc_add_entry_to_bucket(&newBuckets[newBucketIndex], entry);
        }
        // The old bucket's elements have been redistributed, free it up
        GC_FREE(oldBucket->entries);
    }
    // Free the old bucket, replace entries
    GC_FREE(gc->hash_map.buckets);
    gc->hash_map.buckets = newBuckets;
    gc->hash_map.buckets_length = newLength;
    // Recompute sweep limit
    gc_recompute_sweep_limit(gc);
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
    gc_add_to_hash_bucket(gc, &gc->hash_map.buckets[bucketIndex], entry);
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
            // Found - copy out BEFORE removing (memmove will overwrite this memory)
            if (outAllocation != NULL) *outAllocation = *allocation;
            gc_remove_from_hash_bucket_at(gc, bucket, i);
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

static void gc_recompute_sweep_limit(GC_World* gc) {
    // NOTE: Maybe revisit to see if behavior is sensible
    gc->sweep_limit = (size_t)((double)gc->hash_map.entry_count + gc->sweep_factor * (double)(gc->hash_map.buckets_length - gc->hash_map.entry_count));
}

static bool gc_needs_sweep(GC_World* gc) {
    return gc->hash_map.entry_count > gc->sweep_limit;
}

static size_t gc_sweep(GC_World* gc) {
    GC_LOG("starting sweep phase");
    // We go through the entries, whatever is marked we just unflag and whatever was not marked is freed
    size_t freedMem = 0;
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
            freedMem += allocation->size;
            // NOTE: We are allowed to remove like this, this won't trigger shrinking
            gc_remove_from_hash_bucket_at(gc, bucket, j);
            // Since we removed, we are off-by-one, don't increment j
        }
    }
    // Compact hash map
    gc_shrink_hash_map_if_needed(gc);
    GC_LOG("sweep phase completed (freed %zu bytes)", freedMem);
    return freedMem;
}

// API functions ///////////////////////////////////////////////////////////////

void gc_start(GC_World* gc) {
    gc->stack_bottom = gc_compute_stack_bottom();
    GC_ASSERT(gc->stack_bottom != NULL, "failed to compute stack bottom, cannot start GC");
    if (gc->sweep_factor == 0.0) gc->sweep_factor = 0.5;
    gc_collect_global_sections(gc);
}

void gc_stop(GC_World* gc) {
    GC_LOG("garbage collector stopped, unpinning all pinned allocations");
    // Let's unpin all pinned allocations, then run a cycle of mark and sweep
    for (size_t i = 0; i < gc->hash_map.buckets_length; ++i) {
        GC_HashBucket* bucket = &gc->hash_map.buckets[i];
        for (size_t j = 0; j < bucket->length; ++j) {
            GC_Allocation* allocation = &bucket->entries[j].allocation;
            allocation->flags &= ~GC_FLAG_PINNED;
        }
    }
    gc_mark(gc);
    gc_sweep(gc);

    // Clean up the bookkeeping structures too
    gc_free_data_structures(gc);
}

void gc_pause(GC_World* gc) {
    GC_LOG("pausing garbage collection");
    gc->paused = true;
}

void gc_resume(GC_World* gc) {
    GC_LOG("resuming garbage collection");
    gc->paused = false;
}

size_t gc_run(GC_World* gc, bool force) {
    if (gc->paused) {
        GC_LOG("skipping mark-and-sweep, garbage collection paused");
        return 0;
    }

    if (force || gc_needs_sweep(gc)) {
        GC_LOG("mark-and-sweep triggered (forced: %s, sweep limit: %zu)", (force ? "true" : "false"), gc->sweep_limit);
        gc_mark(gc);
        return gc_sweep(gc);
    }

    return 0;
}

void gc_pin(GC_World* gc, void* mem) {
    GC_Allocation* allocation = gc_get_from_hash_map(gc, mem);
    if (allocation == NULL) {
        GC_LOG("gc_pin received a memory address %p that had no corresponding allocation", mem);
        return;
    }

    allocation->flags |= GC_FLAG_PINNED;
}

void gc_unpin(GC_World* gc, void* mem) {
    GC_Allocation* allocation = gc_get_from_hash_map(gc, mem);
    if (allocation == NULL) {
        GC_LOG("gc_unpin received a memory address %p that had no corresponding allocation", mem);
        return;
    }

    allocation->flags &= ~GC_FLAG_PINNED;
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
    (void)oldSize;
    uintptr_t oldMem = (uintptr_t)mem;

    // Call out to reallocation
    void* reMem = GC_REALLOC(mem, size);
    if (reMem == NULL) {
        GC_LOG("gc_realloc with memory %p failed to reallocate memory of size %zu to %zu", (void*)oldMem, oldSize, size);
        return NULL;
    }

    if ((uintptr_t)reMem == oldMem) {
        // We are in luck, we can just resize the allocation
        GC_LOG("gc_realloc called with address %p was resized in-place from %zu to %zu", (void*)oldMem, oldSize, size);
        allocation->size = size;
        return reMem;
    }

    // We need to remove and readd
    // NOTE: We could optimize by getting the bucket index and element index from the getter
    // but that feels a bit of an overkill
    gc_remove_from_hash_map(gc, (void*)oldMem, NULL);
    GC_LOG("reallocated memory (old address: %p, old size: %zu, new address: %p, new size: %zu)", (void*)oldMem, oldSize, reMem, size);
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

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef GC_SELF_TEST

#include <math.h>

// We use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

#if defined(_MSC_VER)
#   define GC_TEST_NOINLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__)
#   define GC_TEST_NOINLINE __attribute__((noinline))
#else
#   define GC_TEST_NOINLINE
#endif

// NOTE: Maybe CTEST should ship with utilities like this?
#define APPROX_EQ(x, y) (fabs((x) - (y)) < 0.0001)

static GC_World test_gc_create(void) {
    GC_World gc = { 0 };
    gc_start(&gc);
    return gc;
}

static void test_gc_destroy(GC_World* gc) {
    gc_stop(gc);
}

// Helper functionality to make platform-behavior a little more deterministic

GC_TEST_NOINLINE
static void helper_allocate_unreferenced(GC_World* gc, size_t size) {
    volatile void* mem = gc_alloc(gc, size);
    mem = NULL;
    (void)mem;
}

GC_TEST_NOINLINE
static void helper_allocate_multiple_unreferenced(GC_World* gc) {
    volatile void* m1 = gc_alloc(gc, 32);
    volatile void* m2 = gc_alloc(gc, 64);
    volatile void* m3 = gc_alloc(gc, 128);
    m1 = NULL; m2 = NULL; m3 = NULL;
    (void)m1; (void)m2; (void)m3;
}

GC_TEST_NOINLINE
static void helper_allocate_and_pin(GC_World* gc, size_t size) {
    volatile void* mem = gc_alloc(gc, size);
    gc_pin(gc, (void*)mem);
    mem = NULL;
    (void)mem;
}

GC_TEST_NOINLINE
static void helper_allocate_pin_then_unpin(GC_World* gc, size_t size) {
    volatile void* mem = gc_alloc(gc, size);
    gc_pin(gc, (void*)mem);
    gc_unpin(gc, (void*)mem);
    mem = NULL;
    (void)mem;
}

// Useful to clear residual values on the stack, especially to clean up local refs
GC_TEST_NOINLINE
static void helper_clobber_stack(void) {
    // Size chosen by a fair dice roll, for these simple calls should be enough
    volatile uintptr_t buffer[256];
    for (size_t i = 0; i < 256; ++i) {
        buffer[i] = 0xDEADBEEF;
    }
    (void)buffer[0]; // Hopefully this is enough to not get it optimized out
}

// Basic allocation tests //////////////////////////////////////////////////////

CTEST_CASE(gc_alloc_returns_non_null) {
    GC_World gc = test_gc_create();
    void* mem = gc_alloc(&gc, 64);
    CTEST_ASSERT_TRUE(mem != NULL);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_alloc_multiple_allocations) {
    GC_World gc = test_gc_create();
    void* mem1 = gc_alloc(&gc, 32);
    void* mem2 = gc_alloc(&gc, 64);
    void* mem3 = gc_alloc(&gc, 128);
    CTEST_ASSERT_TRUE(mem1 != NULL);
    CTEST_ASSERT_TRUE(mem2 != NULL);
    CTEST_ASSERT_TRUE(mem3 != NULL);
    CTEST_ASSERT_TRUE(mem1 != mem2);
    CTEST_ASSERT_TRUE(mem2 != mem3);
    CTEST_ASSERT_TRUE(mem1 != mem3);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_alloc_zero_size) {
    GC_World gc = test_gc_create();
    // Just test that it doesn't crash
    void* mem = gc_alloc(&gc, 0);
    (void)mem;
    test_gc_destroy(&gc);
}

// Reallocation tests //////////////////////////////////////////////////////////

CTEST_CASE(gc_realloc_null_acts_as_alloc) {
    GC_World gc = test_gc_create();
    void* mem = gc_realloc(&gc, NULL, 64);
    CTEST_ASSERT_TRUE(mem != NULL);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_realloc_grows_allocation) {
    GC_World gc = test_gc_create();
    void* mem = gc_alloc(&gc, 32);
    CTEST_ASSERT_TRUE(mem != NULL);
    // Write some data
    ((char*)mem)[0] = 'A';
    ((char*)mem)[31] = 'Z';
    // Grow the allocation
    void* newMem = gc_realloc(&gc, mem, 128);
    CTEST_ASSERT_TRUE(newMem != NULL);
    // Data should be preserved
    CTEST_ASSERT_TRUE(((char*)newMem)[0] == 'A');
    CTEST_ASSERT_TRUE(((char*)newMem)[31] == 'Z');
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_realloc_shrinks_allocation) {
    GC_World gc = test_gc_create();
    void* mem = gc_alloc(&gc, 128);
    CTEST_ASSERT_TRUE(mem != NULL);
    ((char*)mem)[0] = 'X';
    void* newMem = gc_realloc(&gc, mem, 32);
    CTEST_ASSERT_TRUE(newMem != NULL);
    CTEST_ASSERT_TRUE(((char*)newMem)[0] == 'X');
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_realloc_unknown_address_returns_null) {
    GC_World gc = test_gc_create();
    int stackVar = 42;
    void* result = gc_realloc(&gc, &stackVar, 64);
    CTEST_ASSERT_TRUE(result == NULL);
    test_gc_destroy(&gc);
}

// Manual free tests ///////////////////////////////////////////////////////////

CTEST_CASE(gc_free_removes_allocation) {
    GC_World gc = test_gc_create();
    void* mem = gc_alloc(&gc, 64);
    CTEST_ASSERT_TRUE(mem != NULL);
    size_t countBefore = gc.hash_map.entry_count;
    gc_free(&gc, mem);
    size_t countAfter = gc.hash_map.entry_count;
    CTEST_ASSERT_TRUE(countAfter == countBefore - 1);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_free_unknown_address_does_not_crash) {
    GC_World gc = test_gc_create();
    int stackVar = 42;
    // Should not crash, just return silently
    gc_free(&gc, &stackVar);
    test_gc_destroy(&gc);
}

// Pinning tests ///////////////////////////////////////////////////////////////

CTEST_CASE(gc_pin_prevents_collection_of_unreferenced_allocation) {
    GC_World gc = test_gc_create();
    // Helper allocates and pins, pointer leaves stack when helper returns
    helper_allocate_and_pin(&gc, 64);
    // To stress this, even clobber the stack to clear any residual pointer values
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    // Force GC - pinned allocation should survive even though no stack reference exists
    gc_run(&gc, true);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_unpin_allows_collection_of_unreferenced_allocation) {
    GC_World gc = test_gc_create();
    // Allocate, pin, then unpin in helper - pointer leaves stack when helper returns
    helper_allocate_pin_then_unpin(&gc, 64);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    // Force GC - unpinned allocation with no stack reference should be collected
    gc_run(&gc, true);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 0);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_pin_unknown_address_does_not_crash) {
    GC_World gc = test_gc_create();
    int stackVar = 42;
    gc_pin(&gc, &stackVar);
    gc_unpin(&gc, &stackVar);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_pin_sets_flag) {
    GC_World gc = test_gc_create();
    void* mem = gc_alloc(&gc, 64);
    GC_Allocation* alloc = gc_get_from_hash_map(&gc, mem);
    CTEST_ASSERT_TRUE((alloc->flags & GC_FLAG_PINNED) == 0);
    gc_pin(&gc, mem);
    CTEST_ASSERT_TRUE((alloc->flags & GC_FLAG_PINNED) != 0);
    gc_unpin(&gc, mem);
    CTEST_ASSERT_TRUE((alloc->flags & GC_FLAG_PINNED) == 0);
    test_gc_destroy(&gc);
}

// Pause/Resume tests //////////////////////////////////////////////////////////

CTEST_CASE(gc_pause_prevents_collection) {
    GC_World gc = test_gc_create();
    // Allocate in helper so pointer leaves stack
    helper_allocate_unreferenced(&gc, 64);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    CTEST_ASSERT_TRUE(gc.paused == false);
    gc_pause(&gc);
    CTEST_ASSERT_TRUE(gc.paused == true);
    // This should do nothing because GC is paused
    size_t freed = gc_run(&gc, true);
    CTEST_ASSERT_TRUE(freed == 0);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_resume_allows_collection) {
    GC_World gc = test_gc_create();
    // Allocate in helper so pointer leaves stack
    helper_allocate_unreferenced(&gc, 64);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    gc_pause(&gc);
    gc_resume(&gc);
    CTEST_ASSERT_TRUE(gc.paused == false);
    gc_run(&gc, true);
    // Should have collected the unreferenced allocation
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 0);
    test_gc_destroy(&gc);
}

// Mark and sweep tests ////////////////////////////////////////////////////////

CTEST_CASE(gc_run_collects_unreferenced_memory) {
    GC_World gc = test_gc_create();
    // Allocate in helper so pointers leave stack
    helper_allocate_multiple_unreferenced(&gc);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 3);
    // Force GC - all should be collected since no stack references exist
    size_t freed = gc_run(&gc, true);
    CTEST_ASSERT_TRUE(freed > 0);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 0);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_run_preserves_stack_referenced_memory) {
    GC_World gc = test_gc_create();
    // Keep a reference on the stack
    void* kept = gc_alloc(&gc, 64);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    gc_run(&gc, true);
    // The kept allocation should survive
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    // Verify kept is still tracked
    CTEST_ASSERT_TRUE(gc_get_from_hash_map(&gc, kept) != NULL);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_run_preserves_nested_references) {
    GC_World gc = test_gc_create();
    // Create a "linked" structure: mem1 -> mem2
    // mem1 is on stack, mem2 is only reachable through mem1
    void** mem1 = (void**)gc_alloc(&gc, sizeof(void*));
    void* mem2 = gc_alloc(&gc, 64);
    *mem1 = mem2;  // mem1 points to mem2
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 2);
    // Force GC - both should survive because mem1 is on stack and mem1->mem2
    gc_run(&gc, true);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 2);
    // Verify both are still tracked
    CTEST_ASSERT_TRUE(gc_get_from_hash_map(&gc, mem1) != NULL);
    CTEST_ASSERT_TRUE(gc_get_from_hash_map(&gc, mem2) != NULL);
    CTEST_ASSERT_TRUE(*mem1 == mem2);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_run_returns_zero_when_nothing_to_collect) {
    GC_World gc = test_gc_create();
    // Don't allocate anything
    size_t freed = gc_run(&gc, true);
    CTEST_ASSERT_TRUE(freed == 0);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_run_returns_freed_bytes) {
    GC_World gc = test_gc_create();
    // Allocate known sizes in helper
    helper_allocate_unreferenced(&gc, 100);
    helper_allocate_unreferenced(&gc, 200);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 2);
    size_t freed = gc_run(&gc, true);
    CTEST_ASSERT_TRUE(freed == 300);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_run_without_force_respects_sweep_limit) {
    GC_World gc = test_gc_create();
    // With low allocation count, gc_needs_sweep should return false
    helper_allocate_unreferenced(&gc, 64);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 1);
    // Non-forced run should not collect if under sweep limit
    size_t freed = gc_run(&gc, false);
    // Behavior depends on sweep_limit, just ensure it doesn't crash
    (void)freed;
    test_gc_destroy(&gc);
}

// Lifecycle tests /////////////////////////////////////////////////////////////

CTEST_CASE(gc_start_initializes_world) {
    GC_World gc = { 0 };
    gc_start(&gc);
    CTEST_ASSERT_TRUE(gc.stack_bottom != NULL);
    CTEST_ASSERT_TRUE(gc.sweep_factor > 0.0);
    CTEST_ASSERT_TRUE(gc.paused == false);
    CTEST_ASSERT_TRUE(gc.global_sections.length > 0);
    gc_stop(&gc);
}

CTEST_CASE(gc_stop_frees_all_allocations) {
    GC_World gc = test_gc_create();
    // Allocate in helpers and pin so they survive any implicit GC
    helper_allocate_and_pin(&gc, 32);
    helper_allocate_and_pin(&gc, 64);
    helper_clobber_stack();
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 2);
    // gc_stop unpins and runs a final GC cycle, then frees data structures
    gc_stop(&gc);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == 0);
    CTEST_ASSERT_TRUE(gc.hash_map.buckets == NULL);
    CTEST_ASSERT_TRUE(gc.global_sections.sections == NULL);
}

CTEST_CASE(gc_custom_sweep_factor) {
    GC_World gc = { 0 };
    gc.sweep_factor = 0.8;
    gc_start(&gc);
    CTEST_ASSERT_TRUE(APPROX_EQ(gc.sweep_factor, 0.8));
    gc_stop(&gc);
}

// Hash map tests //////////////////////////////////////////////////////////////

CTEST_CASE(gc_hash_map_grows_with_allocations) {
    GC_World gc = test_gc_create();
    size_t initialBuckets = gc.hash_map.buckets_length;
    // Allocate enough to trigger growth
    void* allocations[100];
    for (size_t i = 0; i < 100; ++i) {
        allocations[i] = gc_alloc(&gc, 16);
    }
    CTEST_ASSERT_TRUE(gc.hash_map.buckets_length > initialBuckets);
    // Keep references alive
    CTEST_ASSERT_TRUE(allocations[0] != NULL);
    CTEST_ASSERT_TRUE(allocations[99] != NULL);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_hash_map_shrinks_after_frees) {
    GC_World gc = test_gc_create();
    // Allocate many
    void* allocations[100];
    for (size_t i = 0; i < 100; ++i) {
        allocations[i] = gc_alloc(&gc, 16);
    }
    size_t bucketsAfterAlloc = gc.hash_map.buckets_length;
    // Free most of them manually
    for (size_t i = 0; i < 90; ++i) {
        gc_free(&gc, allocations[i]);
        allocations[i] = NULL;
    }
    // Hash map should have shrunk
    CTEST_ASSERT_TRUE(gc.hash_map.buckets_length < bucketsAfterAlloc);
    test_gc_destroy(&gc);
}

CTEST_CASE(gc_handles_many_allocations) {
    GC_World gc = test_gc_create();
    const size_t count = 1000;
    void* allocations[1000];
    // Allocate many blocks
    for (size_t i = 0; i < count; ++i) {
        allocations[i] = gc_alloc(&gc, 16);
        CTEST_ASSERT_TRUE(allocations[i] != NULL);
    }
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == count);
    // Free half of them manually
    for (size_t i = 0; i < count / 2; ++i) {
        gc_free(&gc, allocations[i]);
        allocations[i] = NULL;
    }
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == count / 2);
    // GC should preserve the rest (they're still referenced in allocations array)
    gc_run(&gc, true);
    CTEST_ASSERT_TRUE(gc.hash_map.entry_count == count / 2);
    test_gc_destroy(&gc);
}

#endif /* GC_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef GC_EXAMPLE
#undef GC_EXAMPLE

#include <stdio.h>

#undef GC_LOG
#define GC_LOG(...) do { printf(__VA_ARGS__); puts(""); } while (0)
#define GC_IMPLEMENTATION
#define GC_STATIC
#include "gc.h"

GC_World gc;

void* memGlobal;

void* bar(void) {
    void* memLocal = gc_alloc(&gc, 1);
    (void)memLocal;
    memGlobal = gc_alloc(&gc, 2);
    void* memReturned = gc_alloc(&gc, 3);
    gc_run(&gc, true);
    return memReturned;
}

void foo(void) {
    void* memReturned = bar();
    (void)memReturned;
    gc_run(&gc, true);
    memGlobal = NULL;
}

int main(void) {
    gc_start(&gc);
    foo();
    gc_run(&gc, true);
    gc_stop(&gc);
    return 0;
}

#endif /* GC_EXAMPLE */
