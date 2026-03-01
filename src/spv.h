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

typedef struct Spv_Allocator {
    // Context pointer that will be passed to the realloc and free functions
    void* context;
    // A function pointer for reallocating memory, with the same semantics as the standard realloc but with an additional context parameter
    void*(*realloc)(void* ctx, void* ptr, size_t new_size);
    // A function pointer for freeing memory, with the same semantics as the standard free but with an additional context parameter
    void(*free)(void* ctx, void* ptr);
} Spv_Allocator;

typedef struct Spv_SectionEncoder {
    struct {
        uint32_t* elements;
        size_t length;
        size_t capacity;
    } words;
    Spv_Allocator allocator;
    Spv_Track track;
} Spv_SectionEncoder;

typedef struct Spv_ModuleEncoder {
    uint32_t header;
    struct {
        Spv_SectionEncoder capabilities;
        Spv_SectionEncoder extensions;
        Spv_SectionEncoder imports;

        Spv_SectionEncoder memory_model;

        Spv_SectionEncoder entry_points;
        Spv_SectionEncoder execution_modes;

        Spv_SectionEncoder debug;
        Spv_SectionEncoder annotations;

        Spv_SectionEncoder types;
        Spv_SectionEncoder constants;
        Spv_SectionEncoder globals;
        Spv_SectionEncoder functions;
    } sections;
    Spv_Allocator allocator;
} Spv_ModuleEncoder;

typedef struct Spv_Track {
    struct {
        uint32_t* elements;
        size_t length;
        size_t capacity;
    } capabilities;
    struct {
        int* elements;
        size_t length;
        size_t capacity;
    } extensions;
    Spv_Allocator allocator;
} Spv_Track;

SPV_DEF void spv_reserve(Spv_SectionEncoder* encoder, size_t word_count);

#ifdef __cplusplus
}
#endif

#endif /* SPV_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef SPV_IMPLEMENTATION

#include <assert.h>

#define SPV_ADD_TO_ARRAY(allocator, array, element) \
    do { \
        if ((array).length + 1 > (array).capacity) { \
            size_t newCapacity = ((array).capacity == 0) ? 8 : ((array).capacity * 2); \
            void* newElements = spv_realloc((allocator), (array).elements, newCapacity * sizeof(*(array).elements)); \
            (array).elements = newElements; \
            (array).capacity = newCapacity; \
        } \
        (array).elements[(array).length++] = element; \
    } while (false)

#ifdef __cplusplus
extern "C" {
#endif

// Allocation //////////////////////////////////////////////////////////////////

static void* spv_default_realloc(void* ctx, void* ptr, size_t new_size) {
    (void)ctx;
    return realloc(ptr, new_size);
}

static void spv_default_free(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}

static void spv_init_allocator(Spv_Allocator* allocator) {
    if (allocator->realloc != NULL || allocator->free != NULL) {
        SPV_ASSERT(allocator->realloc != NULL && allocator->free != NULL, "both realloc and free function pointers must be set in allocator");
        return;
    }
    allocator->realloc = spv_default_realloc;
    allocator->free = spv_default_free;
}

static void* spv_realloc(Spv_Allocator* allocator, void* ptr, size_t size) {
    spv_init_allocator(allocator);
    void* result = allocator->realloc(allocator->context, ptr, size);
    SPV_ASSERT(result != NULL, "failed to allocate memory");
    return result;
}

static void spv_free(Spv_Allocator* allocator, void* ptr) {
    spv_init_allocator(allocator);
    allocator->free(allocator->context, ptr);
}

// Encoding ////////////////////////////////////////////////////////////////////

static void spv_encode_u32(Spv_SectionEncoder* encoder, uint32_t value) {
    spv_reserve(encoder, encoder->words.length + 1);
    encoder->words.elements[encoder->words.length++] = value;
}

static void spv_encode_i32(Spv_SectionEncoder* encoder, int32_t value) {
    spv_encode_u32(encoder, (uint32_t)value);
}

static void spv_encode_f32(Spv_SectionEncoder* encoder, float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(float));
    spv_encode_u32(encoder, bits);
}

static void spv_encode_string(Spv_SectionEncoder* encoder, char const* str) {
    size_t byteCount = strlen(str) + 1;
    // we pad to 4 bytes
    size_t wordCount = (byteCount + 3) / 4;
    spv_reserve(encoder, encoder->words.length + wordCount);
    memcpy(&encoder->words.elements[encoder->words.length], str, byteCount);
    // Must 0 out the remaining padding bytes
    size_t paddingBytes = wordCount * 4 - byteCount;
    memset((uint8_t*)&encoder->words.elements[encoder->words.length] + byteCount, 0, paddingBytes);
    encoder->words.length += wordCount;
}

void spv_reserve(Spv_SectionEncoder* encoder, size_t word_count) {
    if (word_count <= encoder->words.capacity) return;

    size_t newCapacity = encoder->words.capacity == 0 ? 8 : encoder->words.capacity * 2;
    while (newCapacity < encoder->words.length + word_count) newCapacity *= 2;

    uint32_t* newWords = (uint32_t*)spv_realloc(&encoder->allocator, encoder->words.elements, newCapacity * sizeof(uint32_t));
    encoder->words.elements = newWords;
    encoder->words.capacity = newCapacity;
}

// Tracking utilities used by implementation ///////////////////////////////////

static void spv_track_capability(Spv_Track* track, int capability) {
    // First, we check if we already have this capability
    for (size_t i = 0; i < track->capabilities.length; ++i) {
        if (track->capabilities.elements[i] == capability) return;
    }
    SPV_ADD_TO_ARRAY(&track->allocator, track->capabilities, capability);
}

static void spv_track_extension(Spv_Track* track, int extension) {
    // First, we check if we already have this extension
    for (size_t i = 0; i < track->extensions.length; ++i) {
        if (track->extensions.elements[i] == extension) return;
    }
    SPV_ADD_TO_ARRAY(&track->allocator, track->extensions, extension);
}

#ifdef __cplusplus
}
#endif

#undef SPV_ADD_TO_ARRAY

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
