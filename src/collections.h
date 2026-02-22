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

#define DynamicArray_remove(array, index) DynamicArray_remove_range((array), (index), 1)

#define DynamicArray_insert_range(array, index, ins_elements, count) \
    do { \
        COLLECTIONS_ASSERT((index) <= (array).length, "index out of bounds for dynamic array range insertion"); \
        DynamicArray_reserve((array), (array).length + (count)); \
        memmove(&(array).elements[(index) + (count)], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        memcpy(&(array).elements[index], (ins_elements), (count) * sizeof(*(array).elements)); \
        (array).length += (count); \
    } while (false)

#define DynamicArray_remove_range(array, index, count) \
    do { \
        COLLECTIONS_ASSERT((index) < (array).length, "index out of bounds for dynamic array range removal"); \
        COLLECTIONS_ASSERT((index) + (count) <= (array).length, "range out of bounds for dynamic array range removal"); \
        memmove(&(array).elements[index], &(array).elements[(index) + (count)], ((array).length - (index) - (count)) * sizeof(*(array).elements)); \
        (array).length -= (count); \
    } while (false)

// Hash table //////////////////////////////////////////////////////////////////

/*
#define HashTable(K, V, hash_fn, eq_fn) \
    struct { \
        struct { \
            struct { \
                K key; \
                V value; \
                size_t hash; \
            }* entries; \
            size_t length; \
            size_t capacity; \
        }* buckets; \
        size_t buckets_length; \
        size_t entry_count; \
        size_t (*hash_fn)(K); \
        bool (*eq_fn)(K, K); \
    }

#define HashTable_free(table) \
    do { \
        for (size_t i = 0; i < (table).buckets_length; ++i) { \
            COLLECTIONS_FREE((table).buckets[i].entries); \
        } \
        COLLECTIONS_FREE((table).buckets); \
        (table).buckets = NULL; \
        (table).buckets_length = 0; \
        (table).entry_count = 0; \
    } while (false)

#define HashTable_resize(table, new_buckets_length) \
    do { \
        struct { K key; V value; size_t hash; }* newBuckets = (struct { K key; V value; size_t hash; }*)COLLECTIONS_REALLOC(NULL, (new_buckets_length) * sizeof(*(newBuckets))); \
        COLLECTIONS_ASSERT(newBuckets != NULL, "failed to allocate memory for resized hash table buckets"); \
        memset(newBuckets, 0, (new_buckets_length) * sizeof(*(newBuckets))); \
        for (size_t i = 0; i < (table).buckets_length; ++i) { \
            for (size_t j = 0; j < (table).buckets[i].length; ++j) { \
                size_t newBucketIndex = (table).buckets[i].entries[j].hash % (new_buckets_length); \
                struct { K key; V value; size_t hash; } entry = (table).buckets[i].entries[j]; \
                if ((table).buckets[newBucketIndex].length == (table).buckets[newBucketIndex].capacity) { \
                    size_t newCap = (table).buckets[newBucketIndex].capacity == 0 ? 8 : (table).buckets[newBucketIndex].capacity * 2; \
                    void* newEntries = COLLECTIONS_REALLOC((table).buckets[newBucketIndex].entries, newCap * sizeof(*(table).buckets[newBucketIndex].entries)); \
                    COLLECTIONS_ASSERT(newEntries != NULL, "failed to allocate memory for resized hash table bucket entries"); \
                    (table).buckets[newBucketIndex].entries = newEntries; \
                    (table).buckets[newBucketIndex].capacity = newCap; \
                } \
                (table).buckets[newBucketIndex].entries[(table).buckets[newBucketIndex].length++] = entry; \
            } \
            COLLECTIONS_FREE((table).buckets[i].entries); \
        } \
        COLLECTIONS_FREE((table).buckets); \
        (table).buckets = newBuckets; \
        (table).buckets_length = (new_buckets_length); \
    } while (false)
*/

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

#include <stdlib.h>
#include <string.h>

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

// DynamicArray initialization tests ///////////////////////////////////////////

CTEST_CASE(dynamic_array_empty_on_init) {
    DynamicArray(int) arr = {0};
    CTEST_ASSERT_TRUE(arr.elements == NULL);
    CTEST_ASSERT_TRUE(arr.length == 0);
    CTEST_ASSERT_TRUE(arr.capacity == 0);
}

CTEST_CASE(dynamic_array_length_returns_zero_on_empty) {
    DynamicArray(int) arr = {0};
    CTEST_ASSERT_TRUE(DynamicArray_length(arr) == 0);
}

// DynamicArray_reserve tests //////////////////////////////////////////////////

CTEST_CASE(dynamic_array_reserve_allocates_memory) {
    DynamicArray(int) arr = {0};
    DynamicArray_reserve(arr, 16);
    CTEST_ASSERT_TRUE(arr.elements != NULL);
    CTEST_ASSERT_TRUE(arr.capacity >= 16);
    CTEST_ASSERT_TRUE(arr.length == 0);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_reserve_grows_exponentially) {
    DynamicArray(int) arr = {0};
    DynamicArray_reserve(arr, 1);
    size_t initialCapacity = arr.capacity;
    CTEST_ASSERT_TRUE(initialCapacity >= 8);
    DynamicArray_reserve(arr, 100);
    CTEST_ASSERT_TRUE(arr.capacity >= 100);
    CTEST_ASSERT_TRUE(arr.capacity > initialCapacity);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_reserve_no_shrink) {
    DynamicArray(int) arr = {0};
    DynamicArray_reserve(arr, 64);
    size_t cap = arr.capacity;
    DynamicArray_reserve(arr, 32);
    CTEST_ASSERT_TRUE(arr.capacity == cap);
    DynamicArray_free(arr);
}

// DynamicArray_append tests ///////////////////////////////////////////////////

CTEST_CASE(dynamic_array_append_single_element) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 42);
    CTEST_ASSERT_TRUE(arr.length == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 42);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_append_multiple_elements) {
    DynamicArray(int) arr = {0};
    for (int i = 0; i < 10; ++i) {
        DynamicArray_append(arr, i * 2);
    }
    CTEST_ASSERT_TRUE(arr.length == 10);
    for (int i = 0; i < 10; ++i) {
        CTEST_ASSERT_TRUE(DynamicArray_at(arr, i) == i * 2);
    }
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_append_triggers_growth) {
    DynamicArray(int) arr = {0};
    for (int i = 0; i < 100; ++i) {
        DynamicArray_append(arr, i);
    }
    CTEST_ASSERT_TRUE(arr.length == 100);
    CTEST_ASSERT_TRUE(arr.capacity >= 100);
    for (int i = 0; i < 100; ++i) {
        CTEST_ASSERT_TRUE(DynamicArray_at(arr, i) == i);
    }
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_append_with_struct_type) {
    typedef struct { int x; int y; } Point;
    DynamicArray(Point) arr = {0};
    DynamicArray_append(arr, ((Point){1, 2}));
    DynamicArray_append(arr, ((Point){3, 4}));
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0).x == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0).y == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1).x == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1).y == 4);
    DynamicArray_free(arr);
}

// DynamicArray_at tests ///////////////////////////////////////////////////////

CTEST_CASE(dynamic_array_at_read_access) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 10);
    DynamicArray_append(arr, 20);
    DynamicArray_append(arr, 30);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 10);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 20);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 30);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_at_write_access) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 10);
    DynamicArray_append(arr, 20);
    DynamicArray_at(arr, 0) = 100;
    DynamicArray_at(arr, 1) = 200;
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 100);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 200);
    DynamicArray_free(arr);
}

// DynamicArray_clear tests ////////////////////////////////////////////////////

CTEST_CASE(dynamic_array_clear_resets_length) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    size_t capacityBefore = arr.capacity;
    DynamicArray_clear(arr);
    CTEST_ASSERT_TRUE(arr.length == 0);
    CTEST_ASSERT_TRUE(arr.capacity == capacityBefore);
    CTEST_ASSERT_TRUE(arr.elements != NULL);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_clear_on_empty) {
    DynamicArray(int) arr = {0};
    DynamicArray_clear(arr);
    CTEST_ASSERT_TRUE(arr.length == 0);
}

CTEST_CASE(dynamic_array_clear_allows_reuse) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_clear(arr);
    DynamicArray_append(arr, 10);
    DynamicArray_append(arr, 20);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 10);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 20);
    DynamicArray_free(arr);
}

// DynamicArray_insert tests ///////////////////////////////////////////////////

CTEST_CASE(dynamic_array_insert_at_beginning) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    DynamicArray_insert(arr, 0, 1);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_at_middle) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 3);
    DynamicArray_insert(arr, 1, 2);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_at_end) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_insert(arr, 2, 3);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_into_empty) {
    DynamicArray(int) arr = {0};
    DynamicArray_insert(arr, 0, 42);
    CTEST_ASSERT_TRUE(arr.length == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 42);
    DynamicArray_free(arr);
}

// DynamicArray_remove tests ///////////////////////////////////////////////////

CTEST_CASE(dynamic_array_remove_from_beginning) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    DynamicArray_remove(arr, 0);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_from_middle) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    DynamicArray_remove(arr, 1);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_from_end) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    DynamicArray_remove(arr, 2);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_last_element) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 42);
    DynamicArray_remove(arr, 0);
    CTEST_ASSERT_TRUE(arr.length == 0);
    DynamicArray_free(arr);
}

// DynamicArray_insert_range tests /////////////////////////////////////////////

CTEST_CASE(dynamic_array_insert_range_at_beginning) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 4);
    DynamicArray_append(arr, 5);
    int values[] = {1, 2, 3};
    DynamicArray_insert_range(arr, 0, values, 3);
    CTEST_ASSERT_TRUE(arr.length == 5);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 3) == 4);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 4) == 5);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_range_at_middle) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 5);
    int values[] = {2, 3, 4};
    DynamicArray_insert_range(arr, 1, values, 3);
    CTEST_ASSERT_TRUE(arr.length == 5);
    for (int i = 0; i < 5; ++i) {
        CTEST_ASSERT_TRUE(DynamicArray_at(arr, i) == i + 1);
    }
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_range_at_end) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    int values[] = {3, 4, 5};
    DynamicArray_insert_range(arr, 2, values, 3);
    CTEST_ASSERT_TRUE(arr.length == 5);
    for (int i = 0; i < 5; ++i) {
        CTEST_ASSERT_TRUE(DynamicArray_at(arr, i) == i + 1);
    }
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_range_into_empty) {
    DynamicArray(int) arr = {0};
    int values[] = {1, 2, 3};
    DynamicArray_insert_range(arr, 0, values, 3);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_insert_range_zero_count) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    int values[] = {99};
    DynamicArray_insert_range(arr, 1, values, 0);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    DynamicArray_free(arr);
}

// DynamicArray_remove_range tests /////////////////////////////////////////////

CTEST_CASE(dynamic_array_remove_range_from_beginning) {
    DynamicArray(int) arr = {0};
    for (int i = 1; i <= 5; ++i) {
        DynamicArray_append(arr, i);
    }
    DynamicArray_remove_range(arr, 0, 2);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 4);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 5);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_range_from_middle) {
    DynamicArray(int) arr = {0};
    for (int i = 1; i <= 5; ++i) {
        DynamicArray_append(arr, i);
    }
    DynamicArray_remove_range(arr, 1, 3);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 5);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_range_from_end) {
    DynamicArray(int) arr = {0};
    for (int i = 1; i <= 5; ++i) {
        DynamicArray_append(arr, i);
    }
    DynamicArray_remove_range(arr, 3, 2);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_range_all_elements) {
    DynamicArray(int) arr = {0};
    for (int i = 1; i <= 5; ++i) {
        DynamicArray_append(arr, i);
    }
    DynamicArray_remove_range(arr, 0, 5);
    CTEST_ASSERT_TRUE(arr.length == 0);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_remove_range_single_element) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    DynamicArray_remove_range(arr, 1, 1);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 3);
    DynamicArray_free(arr);
}

// DynamicArray_free tests /////////////////////////////////////////////////////

CTEST_CASE(dynamic_array_free_resets_state) {
    DynamicArray(int) arr = {0};
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_free(arr);
    CTEST_ASSERT_TRUE(arr.elements == NULL);
    CTEST_ASSERT_TRUE(arr.length == 0);
    CTEST_ASSERT_TRUE(arr.capacity == 0);
}

CTEST_CASE(dynamic_array_free_on_empty) {
    DynamicArray(int) arr = {0};
    DynamicArray_free(arr);
    CTEST_ASSERT_TRUE(arr.elements == NULL);
    CTEST_ASSERT_TRUE(arr.length == 0);
    CTEST_ASSERT_TRUE(arr.capacity == 0);
}

// DynamicArray with different types ///////////////////////////////////////////

CTEST_CASE(dynamic_array_with_char_pointers) {
    DynamicArray(char*) arr = {0};
    DynamicArray_append(arr, "hello");
    DynamicArray_append(arr, "world");
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(strcmp(DynamicArray_at(arr, 0), "hello") == 0);
    CTEST_ASSERT_TRUE(strcmp(DynamicArray_at(arr, 1), "world") == 0);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_with_double) {
    DynamicArray(double) arr = {0};
    DynamicArray_append(arr, 1.5);
    DynamicArray_append(arr, 2.5);
    DynamicArray_append(arr, 3.5);
    CTEST_ASSERT_TRUE(arr.length == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 1.5);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 2.5);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3.5);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_with_size_t) {
    DynamicArray(size_t) arr = {0};
    DynamicArray_append(arr, (size_t)100);
    DynamicArray_append(arr, (size_t)200);
    CTEST_ASSERT_TRUE(arr.length == 2);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 100);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 200);
    DynamicArray_free(arr);
}

// Integration tests ///////////////////////////////////////////////////////////

CTEST_CASE(dynamic_array_mixed_operations) {
    DynamicArray(int) arr = {0};
    // Append some elements
    DynamicArray_append(arr, 1);
    DynamicArray_append(arr, 2);
    DynamicArray_append(arr, 3);
    // Insert at beginning
    DynamicArray_insert(arr, 0, 0);
    // Insert at end
    DynamicArray_insert(arr, 4, 4);
    CTEST_ASSERT_TRUE(arr.length == 5);
    for (int i = 0; i < 5; ++i) {
        CTEST_ASSERT_TRUE(DynamicArray_at(arr, i) == i);
    }
    // Remove from middle
    DynamicArray_remove(arr, 2);
    CTEST_ASSERT_TRUE(arr.length == 4);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 0);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 1) == 1);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 2) == 3);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 3) == 4);
    // Clear and verify reuse
    DynamicArray_clear(arr);
    CTEST_ASSERT_TRUE(arr.length == 0);
    DynamicArray_append(arr, 100);
    CTEST_ASSERT_TRUE(DynamicArray_at(arr, 0) == 100);
    DynamicArray_free(arr);
}

CTEST_CASE(dynamic_array_large_dataset) {
    DynamicArray(int) arr = {0};
    const int count = 10000;
    for (int i = 0; i < count; ++i) {
        DynamicArray_append(arr, i);
    }
    CTEST_ASSERT_TRUE(arr.length == (size_t)count);
    CTEST_ASSERT_TRUE(arr.capacity >= (size_t)count);
    // Verify all elements
    for (int i = 0; i < count; ++i) {
        CTEST_ASSERT_TRUE(DynamicArray_at(arr, i) == i);
    }
    DynamicArray_free(arr);
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
