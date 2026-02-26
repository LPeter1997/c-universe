/**
 * spv.h is a single-header library for generating SPIR-V binary code.
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
#ifndef SPV_H
// IMPORTANT: We do NOT define SPV_H here, as the generated sections will be dumped at the end of this file, which will define it
//#define SPV_H

#include <stddef.h>
#include <stdint.h>

#ifdef SPV_STATIC
    #define SPV_DEF static
#else
    #define SPV_DEF extern
#endif

#ifndef SPV_ASSERT
    #define SPV_ASSERT(condition, message) ((void)message, (condition))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Spv_ChunkEncoder {

} Spv_ChunkEncoder;

#ifdef __cplusplus
}
#endif

#endif /* SPV_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef SPV_IMPLEMENTATION

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Implementation goes here

#ifdef __cplusplus
}
#endif

#endif /* SPV_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef SPV_SELF_TEST

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

CTEST_CASE(sample_test) {
    CTEST_ASSERT_FAIL("TODO");
}

#endif /* SPV_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef SPV_EXAMPLE
#undef SPV_EXAMPLE

#include <stdio.h>

#define SPV_IMPLEMENTATION
#define SPV_STATIC
#include "spv.h"

int main(void) {
    // TODO: Example usage goes here
    return 0;
}

#endif /* SPV_EXAMPLE */

////////////////////////////////////////////////////////////////////////////////
// Generated section                                                          //
////////////////////////////////////////////////////////////////////////////////
// TODO
