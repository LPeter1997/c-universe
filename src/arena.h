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

// TODO

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
