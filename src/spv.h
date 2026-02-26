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

#include <stdbool.h>
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
    uint32_t* words;
    size_t offset;
    size_t capacity;
} Spv_ChunkEncoder;

typedef struct Spv_ModuleEncoder {
    struct {
        Spv_ChunkEncoder* elements;
        size_t length;
        size_t capacity;
    } chunks;
} Spv_ModuleEncoder;

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

static void spv_encode_u32(Spv_ChunkEncoder* encoder, uint32_t value) {
    // TODO
    (void)encoder;
    (void)value;
}

static void spv_encode_i32(Spv_ChunkEncoder* encoder, int32_t value) {
    // TODO
    (void)encoder;
    (void)value;
}

static void spv_encode_f32(Spv_ChunkEncoder* encoder, float value) {
    // TODO
    (void)encoder;
    (void)value;
}

static void spv_encode_string(Spv_ChunkEncoder* encoder, char const* str) {
    // TODO
    (void)encoder;
    (void)str;
}

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
