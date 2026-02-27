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
    uint32_t* words;
    size_t offset;
    size_t capacity;
    Spv_Allocator allocator;
} Spv_SectionEncoder;

typedef struct Spv_ModuleEncoder {
    struct {
        Spv_SectionEncoder* elements;
        size_t length;
        size_t capacity;
    } chunks;
} Spv_ModuleEncoder;

void spv_reserve(Spv_SectionEncoder* encoder, size_t word_count);

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
    spv_reserve(encoder, encoder->offset + 1);
    encoder->words[encoder->offset++] = value;
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
    spv_reserve(encoder, encoder->offset + wordCount);
    memcpy(&encoder->words[encoder->offset], str, byteCount);
    encoder->offset += wordCount;
}

void spv_reserve(Spv_SectionEncoder* encoder, size_t word_count) {
    if (word_count <= encoder->capacity) return;

    size_t newCapacity = encoder->capacity == 0 ? 8 : encoder->capacity * 2;
    while (newCapacity < encoder->offset + word_count) newCapacity *= 2;

    uint32_t* newWords = (uint32_t*)spv_realloc(&encoder->allocator, encoder->words, newCapacity * sizeof(uint32_t));
    encoder->words = newWords;
    encoder->capacity = newCapacity;
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
