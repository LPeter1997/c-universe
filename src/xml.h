/**
 * xml.h is a single-header TODO.
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
#ifndef XML_H
#define XML_H

#include <stddef.h>

#ifdef XML_STATIC
    #define XML_DEF static
#else
    #define XML_DEF extern
#endif

#ifndef XML_ASSERT
    #define XML_ASSERT(condition, message) ((void)message, (condition))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Xml_Allocator {
    // Context pointer that will be passed to the realloc and free functions
    void* context;
    // A function pointer for reallocating memory, with the same semantics as the standard realloc but with an additional context parameter
    void*(*realloc)(void* ctx, void* ptr, size_t new_size);
    // A function pointer for freeing memory, with the same semantics as the standard free but with an additional context parameter
    void(*free)(void* ctx, void* ptr);
} Xml_Allocator;

// TODO: Public API declarations

#ifdef __cplusplus
}
#endif

#endif /* XML_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef XML_IMPLEMENTATION

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Xml_Parser {
    // TODO
} Xml_Parser;

#ifdef __cplusplus
}
#endif

#endif /* XML_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef XML_SELF_TEST

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

CTEST_CASE(sample_test) {
    CTEST_ASSERT_FAIL("TODO");
}

#endif /* XML_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef XML_EXAMPLE
#undef XML_EXAMPLE

#include <stdio.h>

#define XML_IMPLEMENTATION
#define XML_STATIC
#include "xml.h"

int main(void) {
    // TODO: Example usage goes here
    return 0;
}

#endif /* XML_EXAMPLE */
