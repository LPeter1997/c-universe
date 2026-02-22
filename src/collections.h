/**
 * collections.h is a single-header TODO.
 *
 * Configuration:
 *  - TODO
 *
 * API:
 *  - TODO
 *
 * Check the example section at the end of this file for a full example.
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include <stddef.h>

#ifdef COLLECTIONS_STATIC
    #define COLLECTIONS_DEF static
#else
    #define COLLECTIONS_DEF extern
#endif

#ifndef COLLECTIONS_REALLOC
    #define COLLECTIONS_REALLOC realloc
#endif
#ifndef COLLECTIONS_FREE
    #define COLLECTIONS_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Public API declarations

#ifdef __cplusplus
}
#endif

#endif /* COLLECTIONS_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef COLLECTIONS_IMPLEMENTATION

#include <assert.h>

#define COLLECTIONS_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Implementation goes here

#ifdef __cplusplus
}
#endif

#undef COLLECTIONS_ASSERT

#endif /* COLLECTIONS_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef COLLECTIONS_SELF_TEST

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

CTEST_CASE(sample_test) {
    CTEST_ASSERT_FAIL("TODO");
}

#endif /* COLLECTIONS_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef COLLECTIONS_EXAMPLE
#undef COLLECTIONS_EXAMPLE

#include <stdio.h>

#define COLLECTIONS_IMPLEMENTATION
#define COLLECTIONS_STATIC
#include "collections.h"

int main(void) {
    // TODO: Example usage goes here
    return 0;
}

#endif /* COLLECTIONS_EXAMPLE */
