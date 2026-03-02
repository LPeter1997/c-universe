/**
 * stacktrace.h is a single-header C library to capture and print stack traces.
 *
 * Platform support:
 *  - Windows + MSVC/Clang-cl: Uses CaptureStackBackTrace + DbgHelp
 *  - Windows + MinGW GCC: Uses CaptureStackBackTrace + addr2line
 *  - Linux (GCC, Clang): Uses backtrace() + dladdr()
 *  - macOS (GCC, Clang): Uses backtrace() + dladdr()
 *
 * Configuration:
 *  - STACKTRACE_IMPLEMENTATION: Define in ONE .c file before including
 *  - STACKTRACE_STATIC: Define to make all functions static
 *  - STACKTRACE_MAX_FRAMES: Max frames to capture (default: 64)
 *  - STACKTRACE_ASSERT(cond, msg): Custom assertion macro
 *
 * Compile flags:
 *  - Windows + MSVC: Link with dbghelp.lib (auto-linked via pragma)
 *  - Windows + MinGW: Compile with -g, ensure addr2line is in PATH
 *  - Linux: Compile with -rdynamic for symbol names, link with -ldl
 *  - macOS: Link with -ldl
 *
 * API:
 *  - stacktrace_capture(allocator): Captures the current stack trace
 *  - stacktrace_free(trace): Frees memory allocated for a stack trace
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
    #define STACKTRACE_ASSERT(condition, message) assert(((void)message, condition))
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

// Platform detection //////////////////////////////////////////////////////////

#if defined(_WIN32) || defined(_WIN64)
    #define STACKTRACE_PLATFORM_WINDOWS
    #if defined(__MINGW32__) || defined(__MINGW64__)
        #define STACKTRACE_COMPILER_MINGW
    #endif
#elif defined(__APPLE__) && defined(__MACH__)
    #define STACKTRACE_PLATFORM_MACOS
#elif defined(__linux__)
    #define STACKTRACE_PLATFORM_LINUX
#else
    #define STACKTRACE_PLATFORM_UNKNOWN
#endif

// Platform-specific includes //////////////////////////////////////////////////

#ifdef STACKTRACE_PLATFORM_WINDOWS
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #ifndef STACKTRACE_COMPILER_MINGW
        #include <dbghelp.h>
        #ifdef _MSC_VER
            #pragma comment(lib, "dbghelp.lib")
        #endif
    #endif
#elif defined(STACKTRACE_PLATFORM_LINUX) || defined(STACKTRACE_PLATFORM_MACOS)
    #include <execinfo.h>
    #include <dlfcn.h>
#endif

#include <stdio.h>

// Platform-specific capture ///////////////////////////////////////////////////

#ifndef STACKTRACE_MAX_FRAMES
    #define STACKTRACE_MAX_FRAMES 64
#endif

// MinGW-specific implementation using addr2line //////////////////////////////

#if defined(STACKTRACE_PLATFORM_WINDOWS) && defined(STACKTRACE_COMPILER_MINGW)

// Helper to parse addr2line output: "function_name\nfile:line\n"
static void stacktrace_parse_addr2line(StackTrace_Allocator* allocator,
                                        const char* output,
                                        char** func_name,
                                        char** file_name,
                                        size_t* line_num) {
    // Default values
    *func_name = NULL;
    *file_name = NULL;
    *line_num = 0;

    if (output == NULL || output[0] == '\0') {
        return;
    }

    // Find first newline (separates function name from file:line)
    const char* newline = strchr(output, '\n');
    if (newline == NULL) {
        return;
    }

    // Extract function name
    size_t func_len = (size_t)(newline - output);
    if (func_len > 0 && !(func_len == 1 && output[0] == '?')) {
        char* func = (char*)stacktrace_alloc_realloc(allocator, NULL, func_len + 1);
        memcpy(func, output, func_len);
        func[func_len] = '\0';
        *func_name = func;
    }

    // Parse file:line
    const char* file_start = newline + 1;
    const char* colon = strrchr(file_start, ':');
    if (colon != NULL && colon > file_start) {
        // Check if it's not just "??:0" or "??:?"
        size_t file_len = (size_t)(colon - file_start);
        if (!(file_len == 2 && file_start[0] == '?' && file_start[1] == '?')) {
            char* file = (char*)stacktrace_alloc_realloc(allocator, NULL, file_len + 1);
            memcpy(file, file_start, file_len);
            file[file_len] = '\0';
            *file_name = file;

            // Parse line number
            *line_num = (size_t)atoi(colon + 1);
        }
    }
}

static StackTrace stacktrace_capture_mingw(StackTrace_Allocator allocator) {
    StackTrace trace = {0};
    trace.allocator = allocator;
    stacktrace_init_allocator(&trace.allocator);

    void* stack[STACKTRACE_MAX_FRAMES];

    // Capture stack frames (skip this function)
    USHORT frames_count = CaptureStackBackTrace(1, STACKTRACE_MAX_FRAMES, stack, NULL);
    if (frames_count == 0) {
        return trace;
    }

    // Get executable path for addr2line
    char exe_path[MAX_PATH];
    DWORD path_len = GetModuleFileNameA(NULL, exe_path, MAX_PATH);
    if (path_len == 0 || path_len >= MAX_PATH) {
        // Can't get executable path, return addresses only
        trace.frames = (StackTrace_Frame*)stacktrace_alloc_realloc(&trace.allocator, NULL, frames_count * sizeof(StackTrace_Frame));
        trace.length = frames_count;
        for (USHORT i = 0; i < frames_count; i++) {
            char addr_str[32];
            snprintf(addr_str, sizeof(addr_str), "0x%llx", (unsigned long long)(uintptr_t)stack[i]);
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, addr_str);
            trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
            trace.frames[i].line_number = 0;
        }
        return trace;
    }

    // Get the module base address for relative address calculation
    // addr2line needs addresses relative to the image base when ASLR is enabled
    HMODULE exe_module = GetModuleHandleA(NULL);
    uintptr_t module_base = (uintptr_t)exe_module;

    // PE image base (default for 64-bit Windows is 0x140000000, for 32-bit is 0x400000)
    // addr2line expects addresses as they appear in the PE file, not runtime addresses
#ifdef _WIN64
    uintptr_t image_base = 0x140000000ULL;
#else
    uintptr_t image_base = 0x400000UL;
#endif

    // Allocate frames array
    trace.frames = (StackTrace_Frame*)stacktrace_alloc_realloc(&trace.allocator, NULL, frames_count * sizeof(StackTrace_Frame));
    trace.length = frames_count;

    for (USHORT i = 0; i < frames_count; i++) {
        uintptr_t address = (uintptr_t)stack[i];
        // Convert runtime address to file address for addr2line:
        // file_address = (runtime_address - runtime_base) + image_base
        uintptr_t file_addr = (address - module_base) + image_base;

        // Build addr2line command: addr2line -f -e <exe> <address>
        char cmd[MAX_PATH + 128];
        snprintf(cmd, sizeof(cmd), "addr2line -f -C -e \"%s\" 0x%llx", exe_path, (unsigned long long)file_addr);

        // Run addr2line and capture output
        FILE* pipe = _popen(cmd, "r");
        if (pipe != NULL) {
            char output[1024];
            size_t total_read = 0;
            while (total_read < sizeof(output) - 1) {
                size_t bytes = fread(output + total_read, 1, sizeof(output) - 1 - total_read, pipe);
                if (bytes == 0) break;
                total_read += bytes;
            }
            output[total_read] = '\0';
            _pclose(pipe);

            char* func_name = NULL;
            char* file_name = NULL;
            size_t line_num = 0;
            stacktrace_parse_addr2line(&trace.allocator, output, &func_name, &file_name, &line_num);

            if (func_name != NULL) {
                trace.frames[i].function_name = func_name;
            } else {
                char addr_str[32];
                snprintf(addr_str, sizeof(addr_str), "0x%llx", (unsigned long long)address);
                trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, addr_str);
            }

            if (file_name != NULL) {
                trace.frames[i].file_name = file_name;
                trace.frames[i].line_number = line_num;
            } else {
                trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
                trace.frames[i].line_number = 0;
            }
        } else {
            // addr2line failed, just use address
            char addr_str[32];
            snprintf(addr_str, sizeof(addr_str), "0x%llx", (unsigned long long)address);
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, addr_str);
            trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
            trace.frames[i].line_number = 0;
        }
    }

    return trace;
}

#endif /* STACKTRACE_PLATFORM_WINDOWS && STACKTRACE_COMPILER_MINGW */

// MSVC/Clang-cl Windows implementation using DbgHelp /////////////////////////

#if defined(STACKTRACE_PLATFORM_WINDOWS) && !defined(STACKTRACE_COMPILER_MINGW)

static StackTrace stacktrace_capture_windows(StackTrace_Allocator allocator) {
    StackTrace trace = {0};
    trace.allocator = allocator;
    stacktrace_init_allocator(&trace.allocator);

    void* stack[STACKTRACE_MAX_FRAMES];
    HANDLE process = GetCurrentProcess();

    // Initialize symbol handler
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(process, NULL, TRUE);

    // Capture stack frames (skip this function)
    USHORT frames_count = CaptureStackBackTrace(1, STACKTRACE_MAX_FRAMES, stack, NULL);
    if (frames_count == 0) {
        return trace;
    }

    // Allocate frames array
    trace.frames = (StackTrace_Frame*)stacktrace_alloc_realloc(&trace.allocator, NULL, frames_count * sizeof(StackTrace_Frame));
    trace.length = frames_count;

    // Buffer for symbol info (SYMBOL_INFO + space for name)
    char symbol_buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbol_buffer;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (USHORT i = 0; i < frames_count; i++) {
        DWORD64 address = (DWORD64)(stack[i]);
        DWORD displacement_line = 0;
        DWORD64 displacement_sym = 0;

        // Get function name
        if (SymFromAddr(process, address, &displacement_sym, symbol)) {
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, symbol->Name);
        } else {
            char unknown_func[32];
            snprintf(unknown_func, sizeof(unknown_func), "0x%llx", (unsigned long long)address);
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, unknown_func);
        }

        // Get file and line info
        if (SymGetLineFromAddr64(process, address, &displacement_line, &line)) {
            trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, line.FileName);
            trace.frames[i].line_number = line.LineNumber;
        } else {
            trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
            trace.frames[i].line_number = 0;
        }
    }

    return trace;
}

#endif /* STACKTRACE_PLATFORM_WINDOWS && !STACKTRACE_COMPILER_MINGW */

#if defined(STACKTRACE_PLATFORM_LINUX) || defined(STACKTRACE_PLATFORM_MACOS)

static StackTrace stacktrace_capture_posix(StackTrace_Allocator allocator) {
    StackTrace trace = {0};
    trace.allocator = allocator;
    stacktrace_init_allocator(&trace.allocator);

    void* stack[STACKTRACE_MAX_FRAMES];

    // Capture stack frames (skip this function)
    int frames_count = backtrace(stack, STACKTRACE_MAX_FRAMES);
    if (frames_count <= 1) {
        // No frames captured (or only this function)
        return trace;
    }

    // Skip the first frame (this function)
    int start_frame = 1;
    int actual_count = frames_count - start_frame;

    // Allocate frames array
    trace.frames = (StackTrace_Frame*)stacktrace_alloc_realloc(&trace.allocator, NULL, (size_t)actual_count * sizeof(StackTrace_Frame));
    trace.length = (size_t)actual_count;

    // Get symbol names using backtrace_symbols for fallback
    char** symbols = backtrace_symbols(stack, frames_count);

    for (int i = 0; i < actual_count; i++) {
        int stack_idx = i + start_frame;
        Dl_info info;

        if (dladdr(stack[stack_idx], &info) && info.dli_sname != NULL) {
            // We got symbol info from dladdr
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, info.dli_sname);
        } else if (symbols != NULL && symbols[stack_idx] != NULL) {
            // Fall back to backtrace_symbols output
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, symbols[stack_idx]);
        } else {
            // No symbol info available
            char unknown_func[32];
            snprintf(unknown_func, sizeof(unknown_func), "%p", stack[stack_idx]);
            trace.frames[i].function_name = stacktrace_strdup(&trace.allocator, unknown_func);
        }

        // dladdr gives us the shared object path, but not source file/line
        // Source file and line info require debug info parsing (DWARF), which is complex
        // For now, we use the shared object name if available
        if (dladdr(stack[stack_idx], &info) && info.dli_fname != NULL) {
            trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, info.dli_fname);
        } else {
            trace.frames[i].file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
        }
        trace.frames[i].line_number = 0; // Line numbers require DWARF parsing
    }

    // Free the symbols array allocated by backtrace_symbols
    if (symbols != NULL) {
        free(symbols);
    }

    return trace;
}

#endif /* STACKTRACE_PLATFORM_LINUX || STACKTRACE_PLATFORM_MACOS */

// API /////////////////////////////////////////////////////////////////////////

StackTrace stacktrace_capture(StackTrace_Allocator allocator) {
#if defined(STACKTRACE_PLATFORM_WINDOWS) && defined(STACKTRACE_COMPILER_MINGW)
    return stacktrace_capture_mingw(allocator);
#elif defined(STACKTRACE_PLATFORM_WINDOWS)
    return stacktrace_capture_windows(allocator);
#elif defined(STACKTRACE_PLATFORM_LINUX) || defined(STACKTRACE_PLATFORM_MACOS)
    return stacktrace_capture_posix(allocator);
#else
    // Unsupported platform - return empty trace
    StackTrace trace = {0};
    trace.allocator = allocator;
    stacktrace_init_allocator(&trace.allocator);
    return trace;
#endif
}

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

static void bar(void) {
    StackTrace trace = stacktrace_capture((StackTrace_Allocator){0});
    for (size_t i = 0; i < trace.length; i++) {
        printf("%s at %s:%zu\n", trace.frames[i].function_name, trace.frames[i].file_name, trace.frames[i].line_number);
    }
    stacktrace_free(&trace);
}

static void foo(void) {
    bar();
}

int main(void) {
    bar();
    foo();
    return 0;
}

#endif /* STACKTRACE_EXAMPLE */
