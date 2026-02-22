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

#include <assert.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

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

// Dynamic array ///////////////////////////////////////////////////////////////

#ifndef COLLECTIONS_ASSERT
    #define COLLECTIONS_ASSERT(condition, message) assert(((void)message, condition))
#endif

#define DynamicArray(T) \
    struct { \
        T* elements; \
        size_t length; \
        size_t capacity; \
    }

#define DynamicArray_free(array) \
    do { \
        COLLECTIONS_FREE((array).elements); \
        (array).elements = NULL; \
        (array).length = 0; \
        (array).capacity = 0; \
    } while (false)

#define DynamicArray_length(array) ((array).length)

#define DynamicArray_at(array, index) (array).elements[(index)]

#define DynamicArray_clear(array) \
    do { \
        (array).length = 0; \
    } while (false)

#define DynamicArray_reserve(array, new_capacity) \
    do { \
        if ((new_capacity) > (array).capacity) { \
            size_t newCap = (array).capacity == 0 ? 8 : (array).capacity * 2; \
            while (newCap < (new_capacity)) newCap *= 2; \
            void* newElements = COLLECTIONS_REALLOC((array).elements, newCap * sizeof(*(array).elements)); \
            COLLECTIONS_ASSERT(newElements != NULL, "failed to allocate memory for dynamic array"); \
            (array).elements = newElements; \
            (array).capacity = newCap; \
        } \
    } while (false)

#define DynamicArray_append(array, element) \
    do { \
        DynamicArray_reserve((array), (array).length + 1); \
        (array).elements[(array).length++] = (element); \
    } while (false)

#define DynamicArray_insert(array, index, element) \
    do { \
        COLLECTIONS_ASSERT((index) <= (array).length, "index out of bounds for dynamic array insertion"); \
        DynamicArray_reserve((array), (array).length + 1); \
        memmove(&(array).elements[(index) + 1], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        (array).elements[index] = (element); \
        ++(array).length; \
    } while (false)

#define DynamicArray_remove(array, index) \
    do { \
        COLLECTIONS_ASSERT((index) < (array).length, "index out of bounds for dynamic array removal"); \
        memmove(&(array).elements[index], &(array).elements[(index) + 1], ((array).length - (index) - 1) * sizeof(*(array).elements)); \
        --(array).length; \
    } while (false)

#define DynamicArray_insert_range(array, index, elements, count) \
    do { \
        COLLECTIONS_ASSERT((index) <= (array).length, "index out of bounds for dynamic array range insertion"); \
        DynamicArray_reserve((array), (array).length + (count)); \
        memmove(&(array).elements[(index) + (count)], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        memcpy(&(array).elements[index], (elements), (count) * sizeof(*(array).elements)); \
        (array).length += (count); \
    } while (false)

#define DynamicArray_remove_range(array, index, count) \
    do { \
        COLLECTIONS_ASSERT((index) < (array).length, "index out of bounds for dynamic array range removal"); \
        COLLECTIONS_ASSERT((index) + (count) <= (array).length, "range out of bounds for dynamic array range removal"); \
        memmove(&(array).elements[index], &(array).elements[(index) + (count)], ((array).length - (index) - (count)) * sizeof(*(array).elements)); \
        (array).length -= (count); \
    } while (false)

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
