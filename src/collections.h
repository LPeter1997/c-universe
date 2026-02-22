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

#define __COLLECTIONS_ID(name) __collections_ ## name ## _ ## __LINE__

/**
 * Defines a dynamic array of the given type.
 * @param T The type of the elements in the array.
 */
#define DynamicArray(T) \
    struct { \
        T* elements; \
        size_t length; \
        size_t capacity; \
    }

/**
 * Frees the memory allocated for the dynamic array and resets its state.
 * @param array The dynamic array to free.
 */
#define DynamicArray_free(array) \
    do { \
        COLLECTIONS_FREE((array).elements); \
        (array).elements = NULL; \
        (array).length = 0; \
        (array).capacity = 0; \
    } while (false)

/**
 * Gets the current length of the dynamic array.
 * @param array The dynamic array to query.
 * @return The current length of the dynamic array.
 */
#define DynamicArray_length(array) ((array).length)

/**
 * Gets the element at the specified index in the dynamic array.
 * Can be used as an lvalue to set the element at the specified index.
 * @param array The dynamic array to query.
 * @param index The index of the element to retrieve.
 * @return The element at the specified index.
 */
#define DynamicArray_at(array, index) (array).elements[(index)]

/**
 * Clears the dynamic array, setting its length to 0 but keeping the allocated buffer for future use.
 * @param array The dynamic array to clear.
 */
#define DynamicArray_clear(array) (array).length = 0

/**
 * Reserves capacity for at least the specified number of elements in the dynamic array, growing the allocated buffer if needed.
 * @param array The dynamic array to reserve capacity for.
 * @param new_capacity The minimum capacity to ensure for the dynamic array.
 */
#define DynamicArray_reserve(array, new_capacity) \
    do { \
        if ((new_capacity) > (array).capacity) { \
            size_t __COLLECTIONS_ID(new_cap) = (array).capacity == 0 ? 8 : (array).capacity * 2; \
            while (__COLLECTIONS_ID(new_cap) < (new_capacity)) __COLLECTIONS_ID(new_cap) *= 2; \
            void* __COLLECTIONS_ID(new_elements) = COLLECTIONS_REALLOC((array).elements, __COLLECTIONS_ID(new_cap) * sizeof(*(array).elements)); \
            COLLECTIONS_ASSERT(__COLLECTIONS_ID(new_elements) != NULL, "failed to allocate memory for dynamic array"); \
            (array).elements = __COLLECTIONS_ID(new_elements); \
            (array).capacity = __COLLECTIONS_ID(new_cap); \
        } \
    } while (false)

/**
 * Appends an element to the end of the dynamic array.
 * @param array The dynamic array to append to.
 * @param element The element to append to the dynamic array.
 */
#define DynamicArray_append(array, element) \
    do { \
        DynamicArray_reserve((array), (array).length + 1); \
        (array).elements[(array).length++] = (element); \
    } while (false)

/**
 * Inserts an element at the specified index in the dynamic array.
 * @param array The dynamic array to insert into.
 * @param index The index at which to insert the element, starting from 0. Must be less than or equal to the current length of the array.
 * @param element The element to insert into the dynamic array.
 */
#define DynamicArray_insert(array, index, element) \
    do { \
        COLLECTIONS_ASSERT((index) <= (array).length, "index out of bounds for dynamic array insertion"); \
        DynamicArray_reserve((array), (array).length + 1); \
        memmove(&(array).elements[(index) + 1], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        (array).elements[index] = (element); \
        ++(array).length; \
    } while (false)

/**
 * Removes the element at the specified index from the dynamic array.
 * @param array The dynamic array to remove from.
 * @param index The index of the element to remove, starting from 0.
 */
#define DynamicArray_remove(array, index) DynamicArray_remove_range((array), (index), 1)

/**
 * Inserts a range of elements at the specified index in the dynamic array.
 * @param array The dynamic array to insert into.
 * @param index The index at which to insert the elements, starting from 0. Must be less than or equal to the current length of the array.
 * @param ins_elements A pointer to the first element in the range of elements to insert into the dynamic array.
 * @param count The number of elements in the range to insert.
 */
#define DynamicArray_insert_range(array, index, ins_elements, count) \
    do { \
        COLLECTIONS_ASSERT((index) <= (array).length, "index out of bounds for dynamic array range insertion"); \
        DynamicArray_reserve((array), (array).length + (count)); \
        memmove(&(array).elements[(index) + (count)], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        memcpy(&(array).elements[index], (ins_elements), (count) * sizeof(*(array).elements)); \
        (array).length += (count); \
    } while (false)

/**
 * Removes a range of elements from the dynamic array.
 * @param array The dynamic array to remove from.
 * @param index The index of the first element in the range to remove, starting from 0.
 * @param count The number of elements in the range to remove.
 */
#define DynamicArray_remove_range(array, index, count) \
    do { \
        COLLECTIONS_ASSERT((index) < (array).length, "index out of bounds for dynamic array range removal"); \
        COLLECTIONS_ASSERT((index) + (count) <= (array).length, "range out of bounds for dynamic array range removal"); \
        memmove(&(array).elements[index], &(array).elements[(index) + (count)], ((array).length - (index) - (count)) * sizeof(*(array).elements)); \
        (array).length -= (count); \
    } while (false)

// Hash table //////////////////////////////////////////////////////////////////

/**
 * Defines a hash table with the given key and value types.
 * @param K The type of the keys in the hash table.
 * @param V The type of the values in the hash table.
 */
#define HashTable(K, V) \
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

/**
 * Frees the memory allocated for the hash table and resets its state.
 * @param table The hash table to free.
 */
#define HashTable_free(table) \
    do { \
        for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < (table).buckets_length; ++__COLLECTIONS_ID(i)) { \
            COLLECTIONS_FREE((table).buckets[__COLLECTIONS_ID(i)].entries); \
        } \
        COLLECTIONS_FREE((table).buckets); \
        (table).buckets = NULL; \
        (table).buckets_length = 0; \
        (table).entry_count = 0; \
    } while (false)

/**
 * Resizes the hash table to have the specified number of buckets, redistributing all existing entries into the new buckets.
 * @param table The hash table to resize.
 * @param new_buckets_length The new number of buckets for the hash table.
 */
#define HashTable_resize(table, new_buckets_length) \
    do { \
        size_t __COLLECTIONS_ID(new_len) = (new_buckets_length); \
        if (__COLLECTIONS_ID(new_len) != (table).buckets_length && __COLLECTIONS_ID(new_len) > 0) { \
            /* Allocate new buckets memory */ \
            void* __COLLECTIONS_ID(new_ptr) = COLLECTIONS_REALLOC(NULL, __COLLECTIONS_ID(new_len) * sizeof(*(table).buckets)); \
            COLLECTIONS_ASSERT(__COLLECTIONS_ID(new_ptr) != NULL, "failed to allocate memory for resized hash table buckets"); \
            memset(__COLLECTIONS_ID(new_ptr), 0, __COLLECTIONS_ID(new_len) * sizeof(*(table).buckets)); \
            /* Save old state */ \
            void* __COLLECTIONS_ID(old_ptr) = (table).buckets; \
            size_t __COLLECTIONS_ID(old_len) = (table).buckets_length; \
            /* Iterate old buckets while still pointing to old */ \
            for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < __COLLECTIONS_ID(old_len); ++__COLLECTIONS_ID(i)) { \
                for (size_t __COLLECTIONS_ID(j) = 0; __COLLECTIONS_ID(j) < (table).buckets[__COLLECTIONS_ID(i)].length; ++__COLLECTIONS_ID(j)) { \
                    /* Capture entry data from old bucket */ \
                    size_t __COLLECTIONS_ID(entry_hash) = (table).buckets[__COLLECTIONS_ID(i)].entries[__COLLECTIONS_ID(j)].hash; \
                    void* __COLLECTIONS_ID(entry_ptr) = &(table).buckets[__COLLECTIONS_ID(i)].entries[__COLLECTIONS_ID(j)]; \
                    size_t __COLLECTIONS_ID(entry_size) = sizeof(*(table).buckets[0].entries); \
                    /* Calculate new bucket index */ \
                    size_t __COLLECTIONS_ID(new_idx) = __COLLECTIONS_ID(entry_hash) % __COLLECTIONS_ID(new_len); \
                    /* Switch to new buckets to write */ \
                    (table).buckets = __COLLECTIONS_ID(new_ptr); \
                    (table).buckets_length = __COLLECTIONS_ID(new_len); \
                    /* Ensure new bucket has capacity */ \
                    if ((table).buckets[__COLLECTIONS_ID(new_idx)].length == (table).buckets[__COLLECTIONS_ID(new_idx)].capacity) { \
                        size_t __COLLECTIONS_ID(new_cap) = (table).buckets[__COLLECTIONS_ID(new_idx)].capacity == 0 ? 8 : (table).buckets[__COLLECTIONS_ID(new_idx)].capacity * 2; \
                        void* __COLLECTIONS_ID(new_entries) = COLLECTIONS_REALLOC((table).buckets[__COLLECTIONS_ID(new_idx)].entries, __COLLECTIONS_ID(new_cap) * sizeof(*(table).buckets[0].entries)); \
                        COLLECTIONS_ASSERT(__COLLECTIONS_ID(new_entries) != NULL, "failed to allocate memory for resized hash table bucket entries"); \
                        (table).buckets[__COLLECTIONS_ID(new_idx)].entries = __COLLECTIONS_ID(new_entries); \
                        (table).buckets[__COLLECTIONS_ID(new_idx)].capacity = __COLLECTIONS_ID(new_cap); \
                    } \
                    /* Copy entry to new bucket */ \
                    size_t __COLLECTIONS_ID(dest_idx) = (table).buckets[__COLLECTIONS_ID(new_idx)].length++; \
                    memcpy(&(table).buckets[__COLLECTIONS_ID(new_idx)].entries[__COLLECTIONS_ID(dest_idx)], __COLLECTIONS_ID(entry_ptr), __COLLECTIONS_ID(entry_size)); \
                    /* Switch back to old buckets for next iteration */ \
                    (table).buckets = __COLLECTIONS_ID(old_ptr); \
                    (table).buckets_length = __COLLECTIONS_ID(old_len); \
                } \
                /* Free old bucket entries */ \
                COLLECTIONS_FREE((table).buckets[__COLLECTIONS_ID(i)].entries); \
            } \
            /* Final switch to new buckets */ \
            COLLECTIONS_FREE(__COLLECTIONS_ID(old_ptr)); \
            (table).buckets = __COLLECTIONS_ID(new_ptr); \
            (table).buckets_length = __COLLECTIONS_ID(new_len); \
        } \
    } while (false)

/**
 * Calculates the load factor of the hash table, defined as the number of entries divided by the number of buckets.
 * @param table The hash table to calculate the load factor for.
 * @return The load factor of the hash table.
 */
#define HashTable_load_factor(table) ((table).buckets_length == 0 ? 1.0 : (double)(table).entry_count / (double)(table).buckets_length)

/**
 * Grows the hash table by doubling the number of buckets and redistributing entries.
 * @param table The hash table to grow.
 */
#define HashTable_grow(table) HashTable_resize((table), (table).buckets_length == 0 ? 8 : (table).buckets_length * 2)

/**
 * Shrinks the hash table by halving the number of buckets and redistributing entries.
 * @param table The hash table to shrink.
 */
#define HashTable_shrink(table) HashTable_resize((table), (table).buckets_length / 2)

/**
 * Retrieves a pointer to the value associated with the specified key in the hash table, or NULL if the key is not present in the table.
 * @param table The hash table to query.
 * @param searched_key The key to search for in the hash table.
 * @param result An output variable that will be set to point to the value associated with the specified key if found, or NULL if not found.
 */
#define HashTable_get(table, searched_key, result) \
    do { \
        if ((table).buckets_length > 0) { \
            size_t __COLLECTIONS_ID(hash) = (table).hash_fn(searched_key); \
            size_t __COLLECTIONS_ID(bucket_idx) = __COLLECTIONS_ID(hash) % (table).buckets_length; \
            for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < (table).buckets[__COLLECTIONS_ID(bucket_idx)].length; ++__COLLECTIONS_ID(i)) { \
                if ((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].hash == __COLLECTIONS_ID(hash) && (table).eq_fn((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].key, searched_key)) { \
                    (result) = &(table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].value; \
                    break; \
                } \
            } \
        } else { \
            (result) = NULL; \
        } \
    } while (false)

/**
 * Sets the value associated with the specified key in the hash table, adding a new entry if the key is not already present
 * or replacing the existing value if the key is already present.
 * @param table The hash table to modify.
 * @param in_key The key to set in the hash table.
 * @param in_value The value to associate with the key.
 */
#define HashTable_set(table, in_key, in_value) \
    do { \
        if (HashTable_load_factor(table) > 0.75 || (table).buckets_length == 0) { \
            HashTable_grow(table); \
        } \
        size_t __COLLECTIONS_ID(hash) = (table).hash_fn(in_key); \
        size_t __COLLECTIONS_ID(bucket_idx) = __COLLECTIONS_ID(hash) % (table).buckets_length; \
        bool __COLLECTIONS_ID(found) = false; \
        for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < (table).buckets[__COLLECTIONS_ID(bucket_idx)].length; ++__COLLECTIONS_ID(i)) { \
            if ((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].hash == __COLLECTIONS_ID(hash) && (table).eq_fn((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].key, in_key)) { \
                (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].value = in_value; \
                __COLLECTIONS_ID(found) = true; \
                break; \
            } \
        } \
        if (!__COLLECTIONS_ID(found)) { \
            if ((table).buckets[__COLLECTIONS_ID(bucket_idx)].length == (table).buckets[__COLLECTIONS_ID(bucket_idx)].capacity) { \
                size_t __COLLECTIONS_ID(new_cap) = (table).buckets[__COLLECTIONS_ID(bucket_idx)].capacity == 0 ? 8 : (table).buckets[__COLLECTIONS_ID(bucket_idx)].capacity * 2; \
                void* __COLLECTIONS_ID(new_entries) = COLLECTIONS_REALLOC((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries, __COLLECTIONS_ID(new_cap) * sizeof(*(table).buckets[__COLLECTIONS_ID(bucket_idx)].entries)); \
                COLLECTIONS_ASSERT(__COLLECTIONS_ID(new_entries) != NULL, "failed to allocate memory for hash table bucket entries during set"); \
                (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries = __COLLECTIONS_ID(new_entries); \
                (table).buckets[__COLLECTIONS_ID(bucket_idx)].capacity = __COLLECTIONS_ID(new_cap); \
            } \
            size_t __COLLECTIONS_ID(idx) = (table).buckets[__COLLECTIONS_ID(bucket_idx)].length++; \
            (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(idx)].key = in_key; \
            (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(idx)].value = in_value; \
            (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(idx)].hash = __COLLECTIONS_ID(hash); \
            ++(table).entry_count; \
        } \
    } while (false)

/**
 * Checks if the specified key is present in the hash table.
 * @param table The hash table to query.
 * @param searched_key The key to search for in the hash table.
 * @param result An output variable that will be set to true if the key is present in the hash table, or false if the key is not present.
 */
#define HashTable_contains(table, searched_key, result) \
    do { \
        (result) = false; \
        if ((table).buckets_length > 0) { \
            size_t __COLLECTIONS_ID(hash) = (table).hash_fn(searched_key); \
            size_t __COLLECTIONS_ID(bucket_idx) = __COLLECTIONS_ID(hash) % (table).buckets_length; \
            for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < (table).buckets[__COLLECTIONS_ID(bucket_idx)].length; ++__COLLECTIONS_ID(i)) { \
                if ((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].hash == __COLLECTIONS_ID(hash) && (table).eq_fn((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].key, searched_key)) { \
                    (result) = true; \
                    break; \
                } \
            } \
        } \
    } while (false)

/**
 * Removes the specified key from the hash table.
 * @param table The hash table to modify.
 * @param searched_key The key to remove from the hash table.
 */
#define HashTable_remove(table, searched_key) \
    do { \
        if ((table).buckets_length > 0) { \
            size_t __COLLECTIONS_ID(hash) = (table).hash_fn(searched_key); \
            size_t __COLLECTIONS_ID(bucket_idx) = __COLLECTIONS_ID(hash) % (table).buckets_length; \
            for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < (table).buckets[__COLLECTIONS_ID(bucket_idx)].length; ++__COLLECTIONS_ID(i)) { \
                if ((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].hash == __COLLECTIONS_ID(hash) && (table).eq_fn((table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)].key, searched_key)) { \
                    size_t __COLLECTIONS_ID(last_idx) = (table).buckets[__COLLECTIONS_ID(bucket_idx)].length - 1; \
                    if (__COLLECTIONS_ID(i) != __COLLECTIONS_ID(last_idx)) { \
                        (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(i)] = (table).buckets[__COLLECTIONS_ID(bucket_idx)].entries[__COLLECTIONS_ID(last_idx)]; \
                    } \
                    --(table).buckets[__COLLECTIONS_ID(bucket_idx)].length; \
                    --(table).entry_count; \
                    break; \
                } \
            } \
        } \
    } while (false)

/**
 * Clears all entries from the hash table, resetting the length of all buckets to 0 but keeping the allocated buffers for future use.
 * @param table The hash table to clear.
 */
#define HashTable_clear(table) \
    do { \
        for (size_t __COLLECTIONS_ID(i) = 0; __COLLECTIONS_ID(i) < (table).buckets_length; ++__COLLECTIONS_ID(i)) { \
            (table).buckets[__COLLECTIONS_ID(i)].length = 0; \
        } \
        (table).entry_count = 0; \
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

// HashTable helper functions //////////////////////////////////////////////////

static size_t test_hash_int(int key) {
    return (size_t)key;
}

static bool test_eq_int(int a, int b) {
    return a == b;
}

static size_t test_hash_string(const char* key) {
    size_t hash = 5381;
    while (*key) {
        hash = ((hash << 5) + hash) + (size_t)*key;
        ++key;
    }
    return hash;
}

static bool test_eq_string(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

// HashTable initialization tests //////////////////////////////////////////////

CTEST_CASE(hash_table_empty_on_init) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    CTEST_ASSERT_TRUE(table.buckets == NULL);
    CTEST_ASSERT_TRUE(table.buckets_length == 0);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
}

CTEST_CASE(hash_table_load_factor_on_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    CTEST_ASSERT_TRUE(HashTable_load_factor(table) == 1.0);
}

// HashTable_set and HashTable_get tests ///////////////////////////////////////

CTEST_CASE(hash_table_set_single_element) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 42, 100);
    CTEST_ASSERT_TRUE(table.entry_count == 1);
    int* result = NULL;
    HashTable_get(table, 42, result);
    CTEST_ASSERT_TRUE(result != NULL);
    CTEST_ASSERT_TRUE(*result == 100);
    HashTable_free(table);
}

CTEST_CASE(hash_table_set_multiple_elements) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_set(table, 3, 30);
    CTEST_ASSERT_TRUE(table.entry_count == 3);
    int* result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 10);
    result = NULL;
    HashTable_get(table, 2, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 20);
    result = NULL;
    HashTable_get(table, 3, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 30);
    HashTable_free(table);
}

CTEST_CASE(hash_table_get_nonexistent_key) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    int* result = NULL;
    HashTable_get(table, 999, result);
    CTEST_ASSERT_TRUE(result == NULL);
    HashTable_free(table);
}

CTEST_CASE(hash_table_get_from_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    int* result = NULL;
    HashTable_get(table, 42, result);
    CTEST_ASSERT_TRUE(result == NULL);
}

CTEST_CASE(hash_table_overwrite_existing_key) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 42, 100);
    HashTable_set(table, 42, 200);
    // Entry count should remain 1 since we're overwriting
    CTEST_ASSERT_TRUE(table.entry_count == 1);
    int* result = NULL;
    HashTable_get(table, 42, result);
    CTEST_ASSERT_TRUE(result != NULL);
    CTEST_ASSERT_TRUE(*result == 200);
    HashTable_free(table);
}

// HashTable_grow tests ////////////////////////////////////////////////////////

CTEST_CASE(hash_table_grow_from_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_grow(table);
    CTEST_ASSERT_TRUE(table.buckets_length == 8);
    CTEST_ASSERT_TRUE(table.buckets != NULL);
    HashTable_free(table);
}

CTEST_CASE(hash_table_grow_doubles_capacity) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_grow(table);
    CTEST_ASSERT_TRUE(table.buckets_length == 8);
    HashTable_grow(table);
    CTEST_ASSERT_TRUE(table.buckets_length == 16);
    HashTable_grow(table);
    CTEST_ASSERT_TRUE(table.buckets_length == 32);
    HashTable_free(table);
}

CTEST_CASE(hash_table_grow_preserves_entries) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_set(table, 3, 30);
    size_t originalCount = table.entry_count;
    HashTable_grow(table);
    CTEST_ASSERT_TRUE(table.entry_count == originalCount);
    int* result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 10);
    result = NULL;
    HashTable_get(table, 2, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 20);
    result = NULL;
    HashTable_get(table, 3, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 30);
    HashTable_free(table);
}

// HashTable_resize tests //////////////////////////////////////////////////////

CTEST_CASE(hash_table_resize_to_specific_size) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_resize(table, 16);
    CTEST_ASSERT_TRUE(table.buckets_length == 16);
    HashTable_free(table);
}

CTEST_CASE(hash_table_resize_preserves_entries) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 100, 1000);
    HashTable_set(table, 200, 2000);
    HashTable_resize(table, 32);
    CTEST_ASSERT_TRUE(table.entry_count == 2);
    int* result = NULL;
    HashTable_get(table, 100, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 1000);
    result = NULL;
    HashTable_get(table, 200, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 2000);
    HashTable_free(table);
}

// HashTable_shrink tests //////////////////////////////////////////////////////

CTEST_CASE(hash_table_shrink_halves_capacity) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_resize(table, 32);
    HashTable_shrink(table);
    CTEST_ASSERT_TRUE(table.buckets_length == 16);
    HashTable_shrink(table);
    CTEST_ASSERT_TRUE(table.buckets_length == 8);
    HashTable_free(table);
}

CTEST_CASE(hash_table_shrink_preserves_entries) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_resize(table, 64);
    HashTable_set(table, 5, 50);
    HashTable_set(table, 10, 100);
    HashTable_shrink(table);
    CTEST_ASSERT_TRUE(table.entry_count == 2);
    int* result = NULL;
    HashTable_get(table, 5, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 50);
    result = NULL;
    HashTable_get(table, 10, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 100);
    HashTable_free(table);
}

// HashTable_load_factor tests /////////////////////////////////////////////////

CTEST_CASE(hash_table_load_factor_calculation) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_resize(table, 8);
    CTEST_ASSERT_TRUE(HashTable_load_factor(table) == 0.0);
    HashTable_set(table, 1, 10);
    CTEST_ASSERT_TRUE(HashTable_load_factor(table) == 0.125); // 1/8
    HashTable_set(table, 2, 20);
    CTEST_ASSERT_TRUE(HashTable_load_factor(table) == 0.25);  // 2/8
    HashTable_set(table, 3, 30);
    HashTable_set(table, 4, 40);
    CTEST_ASSERT_TRUE(HashTable_load_factor(table) == 0.5);   // 4/8
    HashTable_free(table);
}

// HashTable_free tests ////////////////////////////////////////////////////////

CTEST_CASE(hash_table_free_resets_state) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_free(table);
    CTEST_ASSERT_TRUE(table.buckets == NULL);
    CTEST_ASSERT_TRUE(table.buckets_length == 0);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
}

CTEST_CASE(hash_table_free_on_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_free(table);
    CTEST_ASSERT_TRUE(table.buckets == NULL);
    CTEST_ASSERT_TRUE(table.buckets_length == 0);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
}

// HashTable with string keys //////////////////////////////////////////////////

CTEST_CASE(hash_table_with_string_keys) {
    HashTable(const char*, int) table = { .hash_fn = test_hash_string, .eq_fn = test_eq_string };
    HashTable_set(table, "apple", 1);
    HashTable_set(table, "banana", 2);
    HashTable_set(table, "cherry", 3);
    CTEST_ASSERT_TRUE(table.entry_count == 3);
    int* result = NULL;
    HashTable_get(table, "apple", result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 1);
    result = NULL;
    HashTable_get(table, "banana", result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 2);
    result = NULL;
    HashTable_get(table, "cherry", result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 3);
    result = NULL;
    HashTable_get(table, "durian", result);
    CTEST_ASSERT_TRUE(result == NULL);
    HashTable_free(table);
}

CTEST_CASE(hash_table_string_key_overwrite) {
    HashTable(const char*, int) table = { .hash_fn = test_hash_string, .eq_fn = test_eq_string };
    HashTable_set(table, "key", 100);
    HashTable_set(table, "key", 200);
    CTEST_ASSERT_TRUE(table.entry_count == 1);
    int* result = NULL;
    HashTable_get(table, "key", result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 200);
    HashTable_free(table);
}

// HashTable collision handling ////////////////////////////////////////////////

CTEST_CASE(hash_table_handles_collisions) {
    // Keys that will likely hash to same bucket with small bucket count
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_resize(table, 4); // Small bucket count to force collisions
    // These keys (0, 4, 8) will all hash to bucket 0 (mod 4)
    HashTable_set(table, 0, 100);
    HashTable_set(table, 4, 200);
    HashTable_set(table, 8, 300);
    CTEST_ASSERT_TRUE(table.entry_count == 3);
    int* result = NULL;
    HashTable_get(table, 0, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 100);
    result = NULL;
    HashTable_get(table, 4, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 200);
    result = NULL;
    HashTable_get(table, 8, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 300);
    HashTable_free(table);
}

// HashTable auto-grow tests ///////////////////////////////////////////////////

CTEST_CASE(hash_table_auto_grows_on_high_load) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    // Insert many elements to trigger auto-grow
    for (int i = 0; i < 20; ++i) {
        HashTable_set(table, i, i * 10);
    }
    CTEST_ASSERT_TRUE(table.entry_count == 20);
    CTEST_ASSERT_TRUE(HashTable_load_factor(table) <= 0.75);
    // Verify all entries are still accessible
    for (int i = 0; i < 20; ++i) {
        int* result = NULL;
        HashTable_get(table, i, result);
        CTEST_ASSERT_TRUE(result != NULL && *result == i * 10);
    }
    HashTable_free(table);
}

// HashTable integration tests /////////////////////////////////////////////////

CTEST_CASE(hash_table_large_dataset) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    const int count = 1000;
    for (int i = 0; i < count; ++i) {
        HashTable_set(table, i, i * 2);
    }
    CTEST_ASSERT_TRUE(table.entry_count == (size_t)count);
    // Verify all entries
    for (int i = 0; i < count; ++i) {
        int* result = NULL;
        HashTable_get(table, i, result);
        CTEST_ASSERT_TRUE(result != NULL && *result == i * 2);
    }
    HashTable_free(table);
}

CTEST_CASE(hash_table_negative_keys) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, -1, 100);
    HashTable_set(table, -100, 200);
    HashTable_set(table, -999, 300);
    CTEST_ASSERT_TRUE(table.entry_count == 3);
    int* result = NULL;
    HashTable_get(table, -1, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 100);
    result = NULL;
    HashTable_get(table, -100, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 200);
    result = NULL;
    HashTable_get(table, -999, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 300);
    HashTable_free(table);
}

CTEST_CASE(hash_table_modify_value_via_pointer) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 42, 100);
    int* result = NULL;
    HashTable_get(table, 42, result);
    CTEST_ASSERT_TRUE(result != NULL);
    *result = 999;
    result = NULL;
    HashTable_get(table, 42, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 999);
    HashTable_free(table);
}

// HashTable_contains tests ////////////////////////////////////////////////////

CTEST_CASE(hash_table_contains_existing_key) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 42, 100);
    HashTable_set(table, 10, 200);
    bool found = false;
    HashTable_contains(table, 42, found);
    CTEST_ASSERT_TRUE(found);
    found = false;
    HashTable_contains(table, 10, found);
    CTEST_ASSERT_TRUE(found);
    HashTable_free(table);
}

CTEST_CASE(hash_table_contains_nonexistent_key) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 42, 100);
    bool found = true;
    HashTable_contains(table, 999, found);
    CTEST_ASSERT_TRUE(!found);
    HashTable_free(table);
}

CTEST_CASE(hash_table_contains_on_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    bool found = true;
    HashTable_contains(table, 42, found);
    CTEST_ASSERT_TRUE(!found);
}

CTEST_CASE(hash_table_contains_with_string_keys) {
    HashTable(const char*, int) table = { .hash_fn = test_hash_string, .eq_fn = test_eq_string };
    HashTable_set(table, "hello", 1);
    HashTable_set(table, "world", 2);
    bool found = false;
    HashTable_contains(table, "hello", found);
    CTEST_ASSERT_TRUE(found);
    found = false;
    HashTable_contains(table, "world", found);
    CTEST_ASSERT_TRUE(found);
    found = true;
    HashTable_contains(table, "missing", found);
    CTEST_ASSERT_TRUE(!found);
    HashTable_free(table);
}

// HashTable_remove tests //////////////////////////////////////////////////////

CTEST_CASE(hash_table_remove_existing_key) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_set(table, 3, 30);
    CTEST_ASSERT_TRUE(table.entry_count == 3);
    HashTable_remove(table, 2);
    CTEST_ASSERT_TRUE(table.entry_count == 2);
    int* result = NULL;
    HashTable_get(table, 2, result);
    CTEST_ASSERT_TRUE(result == NULL);
    result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 10);
    result = NULL;
    HashTable_get(table, 3, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 30);
    HashTable_free(table);
}

CTEST_CASE(hash_table_remove_nonexistent_key) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    CTEST_ASSERT_TRUE(table.entry_count == 1);
    HashTable_remove(table, 999);
    CTEST_ASSERT_TRUE(table.entry_count == 1);
    int* result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 10);
    HashTable_free(table);
}

CTEST_CASE(hash_table_remove_on_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_remove(table, 42);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
}

CTEST_CASE(hash_table_remove_all_entries) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_set(table, 3, 30);
    HashTable_remove(table, 1);
    HashTable_remove(table, 2);
    HashTable_remove(table, 3);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
    int* result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result == NULL);
    HashTable_free(table);
}

CTEST_CASE(hash_table_remove_then_reinsert) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 42, 100);
    HashTable_remove(table, 42);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
    HashTable_set(table, 42, 200);
    CTEST_ASSERT_TRUE(table.entry_count == 1);
    int* result = NULL;
    HashTable_get(table, 42, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 200);
    HashTable_free(table);
}

CTEST_CASE(hash_table_remove_with_collisions) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_resize(table, 4);
    HashTable_set(table, 0, 100);
    HashTable_set(table, 4, 200);
    HashTable_set(table, 8, 300);
    HashTable_remove(table, 4);
    CTEST_ASSERT_TRUE(table.entry_count == 2);
    int* result = NULL;
    HashTable_get(table, 0, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 100);
    result = NULL;
    HashTable_get(table, 4, result);
    CTEST_ASSERT_TRUE(result == NULL);
    result = NULL;
    HashTable_get(table, 8, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 300);
    HashTable_free(table);
}

// HashTable_clear tests ///////////////////////////////////////////////////////

CTEST_CASE(hash_table_clear_removes_all_entries) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_set(table, 3, 30);
    size_t buckets_before = table.buckets_length;
    HashTable_clear(table);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
    CTEST_ASSERT_TRUE(table.buckets_length == buckets_before);
    CTEST_ASSERT_TRUE(table.buckets != NULL);
    int* result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result == NULL);
    HashTable_free(table);
}

CTEST_CASE(hash_table_clear_on_empty) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_clear(table);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
}

CTEST_CASE(hash_table_clear_allows_reuse) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    HashTable_set(table, 1, 10);
    HashTable_set(table, 2, 20);
    HashTable_clear(table);
    HashTable_set(table, 100, 1000);
    HashTable_set(table, 200, 2000);
    CTEST_ASSERT_TRUE(table.entry_count == 2);
    int* result = NULL;
    HashTable_get(table, 100, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 1000);
    result = NULL;
    HashTable_get(table, 200, result);
    CTEST_ASSERT_TRUE(result != NULL && *result == 2000);
    result = NULL;
    HashTable_get(table, 1, result);
    CTEST_ASSERT_TRUE(result == NULL);
    HashTable_free(table);
}

CTEST_CASE(hash_table_clear_with_many_entries) {
    HashTable(int, int) table = { .hash_fn = test_hash_int, .eq_fn = test_eq_int };
    for (int i = 0; i < 100; ++i) {
        HashTable_set(table, i, i * 10);
    }
    CTEST_ASSERT_TRUE(table.entry_count == 100);
    HashTable_clear(table);
    CTEST_ASSERT_TRUE(table.entry_count == 0);
    for (int i = 0; i < 100; ++i) {
        int* result = NULL;
        HashTable_get(table, i, result);
        CTEST_ASSERT_TRUE(result == NULL);
    }
    HashTable_free(table);
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
