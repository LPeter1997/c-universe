/**
 * arena.h is a single-header library for providing an arena allocator in C.
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
#ifndef ARENA_H
#define ARENA_H

#include <cstddef>
#include <stddef.h>

#ifdef ARENA_STATIC
#   define ARENA_DEF static
#else
#   define ARENA_DEF extern
#endif

#ifndef ARENA_REALLOC
#   define ARENA_REALLOC realloc
#endif
#ifndef ARENA_FREE
#   define ARENA_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Arena_Block;

typedef enum Area_Growth {
    // Each block is the same size
    AREA_GROWTH_UNIFORM,
    // Each block is double the size of the previous block
    AREA_GROWTH_DOUBLE,
} Area_Growth;

typedef struct Arena {
    size_t version;
    struct {
        struct Arena_Block* elements;
        size_t length;
        size_t capacity;
    } blocks;
    size_t current_block_index;
    size_t block_size;
    size_t max_block_size;
    size_t large_threshold;
    Area_Growth growth;
} Arena;

typedef struct Arena_Mark {
    size_t version;
    size_t block_index;
    size_t block_size;
    size_t offset;
} Arena_Mark;

ARENA_DEF void* arena_alloc(Arena* arena, size_t size);
ARENA_DEF void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment);
ARENA_DEF void arena_destroy(Arena* arena);
ARENA_DEF Arena_Mark arena_mark(Arena* arena);
ARENA_DEF void arena_reset(Arena* arena, Arena_Mark mark);

#ifdef __cplusplus
}
#endif

#endif /* ARENA_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARENA_IMPLEMENTATION

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define ARENA_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Arena_Block {
    void* memory;
    size_t size;
    size_t offset;
} Arena_Block;

static void arena_push_block(Arena* arena, Arena_Block* block) {
    block->next = arena->block;
    arena->block = block;
    ++arena->block_count;
}

void* arena_alloc(Arena* arena, size_t size) {
    // NOTE: alignmax_t is C11, for C99 this is good enough
    return arena_alloc_aligned(arena, size, alignof(intmax_t));
}

void* arena_alloc_aligned(Arena* arena, size_t size, size_t alignment) {
    // If the requested size is larger than the large threshold, we allocate a separate block for it
    if (arena->large_threshold != 0 && size >= arena->large_threshold) {
        // For a large block we do not fragment or care about growth strategy, it's gonna be an "exact fit" block
        // We also don't worry about alignment requirements, we assume the underlying allocator will give us maximum alignment
        void* memory = ARENA_REALLOC(NULL, size);
        if (memory == NULL) return NULL;

        Arena_Block* block = (Arena_Block*)ARENA_REALLOC(NULL, sizeof(Arena_Block));
        ARENA_ASSERT(block != NULL, "failed to allocate memory for arena block");

        block->memory = memory;
        block->size = size;
        // We mark the block as fully used, so it won't be used for future allocations
        block->offset = size;
        arena_push_block(arena, block);
        return memory;
    }

    // TODO
}

void arena_destroy(Arena* arena) {
    // TODO
}

Arena_Mark arena_mark(Arena* arena) {
    // TODO
}

void arena_reset(Arena* arena, Arena_Mark mark) {
    // TODO
}

#ifdef __cplusplus
}
#endif

#undef ARENA_ASSERT

#endif /* ARENA_IMPLEMENTATION */
