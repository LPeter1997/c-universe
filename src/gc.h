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

#ifdef GC_STATIC
#   define GC_DEF static
#else
#   define GC_DEF extern
#endif

// TODO

#ifdef __cplusplus
extern "C" {
#endif

// TODO

#ifdef __cplusplus
}
#endif

#endif /* GC_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef GC_IMPLEMENTATION

#include <assert.h>

#define GC_ASSERT(condition, message) assert(((void)message, condition))

// TODO

#undef GC_ASSERT

#endif /* GC_IMPLEMENTATION */
