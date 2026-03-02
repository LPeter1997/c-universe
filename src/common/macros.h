// Common macros for the libraries
// Make sure to #include cleanup.h at the end of the file to remove all macro definitions

// foo
#ifndef LIBRARY_NAME_LOWER
    #error "LIBRARY_NAME_LOWER must be defined before including macros.h"
#endif
// Foo
#ifndef LIBRARY_NAME_CAPITALIZED
    #error "LIBRARY_NAME_CAPITALIZED must be defined before including macros.h"
#endif
// FOO
#ifndef LIBRARY_NAME_UPPER
    #error "LIBRARY_NAME_UPPER must be defined before including macros.h"
#endif

#define LIBRARY_CONCAT_IMPL(a, b) a ## _ ## b
#define LIBRARY_CONCAT(a, b) LIBRARY_CONCAT_IMPL(a, b)

// Used to reference library functions
#define LIBRARY_FUNC(name) LIBRARY_CONCAT(LIBRARY_NAME_LOWER, name)
// Used to reference library types
#define LIBRARY_TYPE(name) LIBRARY_CONCAT(LIBRARY_NAME_CAPITALIZED, name)
// Used to reference library macros
#define LIBRARY_MACRO(name) LIBRARY_CONCAT(LIBRARY_NAME_UPPER, name)

// References the library's assert macro
#define LIBRARY_ASSERT(cond, msg) LIBRARY_MACRO(ASSERT)(cond, msg)

// A semi-reliable way to get a unique name to avoid shadowing
#define LIBRARY_UNIQUE_NAME(name) __ ## LIBRARY_NAME_UPPER ## _ ## name ## _ ## __LINE__

// Dynamic array helpers
#define DYNARRAY_MEMBERS(T) \
    T* elements; \
    size_t length; \
    size_t capacity
#define DYNARRAY(T) struct { DYNARRAY_MEMBERS(T); }
#define DYNARRAY_LEN(array) ((array).length)
#define DYNARRAY_AT(array, index) ((array).elements[index])
#define DYNARRAY_LAST(array) DYNARRAY_AT(array, (array).length - 1)
#define DYNARRAY_RESERVE(allocator, array, new_capacity) \
    do { \
        if ((new_capacity) > (array).capacity) { \
            size_t LIBRARY_UNIQUE_NAME(new_cap) = (array).capacity == 0 ? 8 : (array).capacity * 2; \
            while (LIBRARY_UNIQUE_NAME(new_cap) < (new_capacity)) LIBRARY_UNIQUE_NAME(new_cap) *= 2; \
            void* LIBRARY_UNIQUE_NAME(new_elements) = LIBRARY_FUNC(allocator_realloc)(allocator, (array).elements, LIBRARY_UNIQUE_NAME(new_cap) * sizeof(*(array).elements)); \
            (array).elements = LIBRARY_UNIQUE_NAME(new_elements); \
            (array).capacity = LIBRARY_UNIQUE_NAME(new_cap); \
        } \
    } while (false)
#define DYNARRAY_PUSH(allocator, array, element) \
    do { \
        DYNARRAY_RESERVE(allocator, array, (array).length + 1); \
        (array).elements[(array).length++] = (element); \
    } while (false)
#define DYNARRAY_PUSH_RANGE(allocator, array, elements, count) DYNARRAY_INSERT_RANGE(allocator, array, (array).length, elements, count)
#define DYNARRAY_POP(array) DYNARRAY_AT(array, --(array).length)
#define DYNARRAY_INSERT(allocator, array, index, element) \
    do { \
        DYNARRAY_RESERVE(allocator, array, (array).length + 1); \
        memmove(&(array).elements[(index) + 1], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        (array).elements[index] = (element); \
        ++(array).length; \
    } while (false)
#define DYNARRAY_INSERT_RANGE(allocator, array, index, elements, count) \
    do { \
        DYNARRAY_RESERVE(allocator, array, (array).length + (count)); \
        memmove(&(array).elements[(index) + (count)], &(array).elements[index], ((array).length - (index)) * sizeof(*(array).elements)); \
        memcpy(&(array).elements[index], (elements), (count) * sizeof(*(array).elements)); \
        (array).length += (count); \
    } while (false)
#define DYNARRAY_REMOVE(array, index) DYNARRAY_REMOVE_RANGE(array, index, 1)
#define DYNARRAY_REMOVE_RANGE(array, index, count) \
    do { \
        memmove(&(array).elements[index], &(array).elements[(index) + (count)], ((array).length - (index) - (count)) * sizeof(*(array).elements)); \
        (array).length -= (count); \
    } while (false)
#define DYNARRAY_FREE(allocator, array) \
    do { \
        LIBRARY_FUNC(allocator_free)(allocator, (array).elements); \
        (array).elements = NULL; \
        (array).length = 0; \
        (array).capacity = 0; \
    } while (false)
