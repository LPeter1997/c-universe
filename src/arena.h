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
    struct Arena_Block* block;
    size_t version;
    size_t block_count;
    size_t block_size;
    size_t max_block_size;
    size_t large_threshold;
    Area_Growth growth;
} Arena;

typedef struct Arena_Mark {
    struct Arena_Block* block;
    size_t version;
    size_t block_count;
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

#ifdef __cplusplus
extern "C" {
#endif

// TODO

#ifdef __cplusplus
}
#endif

#endif /* ARENA_IMPLEMENTATION */
