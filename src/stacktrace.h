/**
 * stacktrace.h is a single-header C library to print the current stack trace.
 *
 * Configuration:
 *  - TODO
 *
 * API:
 *  - TODO
 *
 * Check the example section at the end of this file for a full example.
 */

// NOTE: We need this to get some extra non-portable functionality
// and apparently we need this before any includes
#if defined(__linux__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
#endif

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef STACKTRACE_H
#define STACKTRACE_H

#include <stddef.h>

#ifdef STACKTRACE_STATIC
    #define STACKTRACE_DEF static
#else
    #define STACKTRACE_DEF extern
#endif

#ifndef STACKTRACE_ASSERT
    #define STACKTRACE_ASSERT(condition, message) ((void)message, (condition))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An allocator struct that allows customizing memory allocation for the stacktrace.
 */
typedef struct StackTrace_Allocator {
    // Context pointer that will be passed to the realloc and free functions
    void* context;
    // A function pointer for reallocating memory, with the same semantics as the standard realloc but with an additional context parameter
    void*(*realloc)(void* ctx, void* ptr, size_t new_size);
    // A function pointer for freeing memory, with the same semantics as the standard free but with an additional context parameter
    void(*free)(void* ctx, void* ptr);
} StackTrace_Allocator;

typedef struct StackTrace_Frame {
    const char* function_name;
    const char* file_name;
    size_t line_number;
} StackTrace_Frame;

typedef struct StackTrace {
    StackTrace_Frame* frames;
    size_t length;
    StackTrace_Allocator allocator;
} StackTrace;

StackTrace stacktrace_capture(StackTrace_Allocator allocator);
void stacktrace_free(StackTrace* trace);

#ifdef __cplusplus
}
#endif

#endif /* STACKTRACE_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef STACKTRACE_IMPLEMENTATION

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
    #include <windows.h>
    #include <dbghelp.h>
#else
    #error "unsupported platform"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Allocation //////////////////////////////////////////////////////////////////

static void* stacktrace_default_realloc(void* ctx, void* ptr, size_t new_size) {
    (void)ctx;
    return realloc(ptr, new_size);
}

static void stacktrace_default_free(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}

static void stacktrace_init_allocator(StackTrace_Allocator* allocator) {
    if (allocator->realloc != NULL || allocator->free != NULL) {
        STACKTRACE_ASSERT(allocator->realloc != NULL && allocator->free != NULL, "both realloc and free function pointers must be set in allocator");
        return;
    }
    allocator->realloc = stacktrace_default_realloc;
    allocator->free = stacktrace_default_free;
}

static void* stacktrace_alloc_realloc(StackTrace_Allocator* allocator, void* ptr, size_t size) {
    stacktrace_init_allocator(allocator);
    void* result = allocator->realloc(allocator->context, ptr, size);
    STACKTRACE_ASSERT(result != NULL, "failed to allocate memory");
    return result;
}

static void stacktrace_alloc_free(StackTrace_Allocator* allocator, void* ptr) {
    stacktrace_init_allocator(allocator);
    allocator->free(allocator->context, ptr);
}

static char* stacktrace_strdup(StackTrace_Allocator* allocator, const char* str) {
    size_t len = strlen(str);
    char* copy = stacktrace_alloc_realloc(allocator, NULL, len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

// API /////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
StackTrace stacktrace_capture(StackTrace_Allocator allocator) {
    HANDLE process = GetCurrentProcess();
    BOOL symInitSuccess = SymInitialize(process, NULL, TRUE);
    STACKTRACE_ASSERT(symInitSuccess, "SymInitialize failed");

    void* stack[MAXUSHORT];
    WORD frames = CaptureStackBackTrace(0, (DWORD)MAXUSHORT, stack, NULL);

    StackTrace trace;
    trace.allocator = allocator;
    trace.length = frames;
    trace.frames = stacktrace_alloc_realloc(&allocator, NULL, frames * sizeof(StackTrace_Frame));
    memset(trace.frames, 0, frames * sizeof(StackTrace_Frame));

    SYMBOL_INFO* symbol = (SYMBOL_INFO*)stacktrace_alloc_realloc(sizeof(SYMBOL_INFO) + 256);
    for (WORD i = 0; i < frames; i++) {
        DWORD64 address = (DWORD64)(stack[i]);
        memset(symbol, 0, sizeof(SYMBOL_INFO) + 256);

        symbol->MaxNameLen = 255;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

        if (SymFromAddr(process, address, 0, symbol)) {
            trace.frames[i].function_name = stacktrace_strdup(&allocator, symbol->Name);
        }

        IMAGEHLP_LINE64 line;
        DWORD displacement = 0;
        memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);

        if (SymGetLineFromAddr64(process, address, &displacement, &line)) {
            trace.frames[i].file_name = stacktrace_strdup(&allocator, line.FileName);
            trace.frames[i].line_number = line.LineNumber;
        }

    }
    stacktrace_alloc_free(&allocator, symbol);

    return trace;
}
#else
    #error "stacktrace_capture is not implemented for this platform"
#endif

void stacktrace_free(StackTrace* trace) {
    for (size_t i = 0; i < trace->length; i++) {
        stacktrace_alloc_free(&trace->allocator, (void*)trace->frames[i].function_name);
        stacktrace_alloc_free(&trace->allocator, (void*)trace->frames[i].file_name);
    }
    stacktrace_alloc_free(&trace->allocator, trace->frames);
}

#ifdef __cplusplus
}
#endif

#endif /* STACKTRACE_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef STACKTRACE_SELF_TEST

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

CTEST_CASE(sample_test) {
    CTEST_ASSERT_FAIL("TODO");
}

#endif /* STACKTRACE_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef STACKTRACE_EXAMPLE
#undef STACKTRACE_EXAMPLE

#include <stdio.h>

#define STACKTRACE_IMPLEMENTATION
#define STACKTRACE_STATIC
#include "stacktrace.h"

int main(void) {
    // TODO: Example usage goes here
    return 0;
}

#endif /* STACKTRACE_EXAMPLE */
