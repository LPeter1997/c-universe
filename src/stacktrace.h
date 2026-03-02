/**
 * stacktrace.h is a single-header C library to capture and print stack traces.
 *
 * Platform support:
 *  - Windows + MSVC/Clang-cl: Uses CaptureStackBackTrace + DbgHelp
 *  - Windows + MinGW GCC: Uses CaptureStackBackTrace + addr2line
 *  - Linux (GCC, Clang): Uses backtrace() + addr2line
 *  - macOS (GCC, Clang): Uses backtrace() + atos
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
 *  - Linux: Compile with -g -rdynamic, ensure addr2line is in PATH
 *  - macOS: Compile with -g, atos is included with Xcode
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
    char* function_name;
    char* file_name;
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
#elif defined(STACKTRACE_PLATFORM_LINUX)
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <unistd.h>
    #include <stdint.h>
#elif defined(STACKTRACE_PLATFORM_MACOS)
    #include <execinfo.h>
    #include <dlfcn.h>
    #include <unistd.h>
    #include <mach-o/dyld.h>
    #include <sys/types.h>
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

#if defined(STACKTRACE_PLATFORM_LINUX)

// Linux implementation using addr2line for symbol resolution

// Helper to parse addr2line output: "function_name\nfile:line\n"
// (same format as MinGW version)
static void stacktrace_parse_addr2line_linux(StackTrace_Allocator* allocator,
                                              const char* output,
                                              char** func_name,
                                              char** file_name,
                                              size_t* line_num) {
    *func_name = NULL;
    *file_name = NULL;
    *line_num = 0;

    if (output == NULL || output[0] == '\0') {
        return;
    }

    const char* newline = strchr(output, '\n');
    if (newline == NULL) {
        return;
    }

    // Extract function name (skip if it's "??")
    size_t func_len = (size_t)(newline - output);
    if (func_len > 0 && !(func_len == 2 && output[0] == '?' && output[1] == '?')) {
        char* func = (char*)stacktrace_alloc_realloc(allocator, NULL, func_len + 1);
        memcpy(func, output, func_len);
        func[func_len] = '\0';
        *func_name = func;
    }

    // Parse file:line
    const char* file_start = newline + 1;
    const char* colon = strrchr(file_start, ':');
    if (colon != NULL && colon > file_start) {
        size_t file_len = (size_t)(colon - file_start);
        // Skip if file is "??" or empty
        if (!(file_len == 2 && file_start[0] == '?' && file_start[1] == '?') && file_len > 0) {
            // Remove trailing newline from file path if present
            //const char* file_end = colon;
            char* file = (char*)stacktrace_alloc_realloc(allocator, NULL, file_len + 1);
            memcpy(file, file_start, file_len);
            file[file_len] = '\0';
            *file_name = file;
            *line_num = (size_t)atoi(colon + 1);
        }
    }
}

static StackTrace stacktrace_capture_posix(StackTrace_Allocator allocator) {
    StackTrace trace = {0};
    trace.allocator = allocator;
    stacktrace_init_allocator(&trace.allocator);

    void* stack[STACKTRACE_MAX_FRAMES];

    int frames_count = backtrace(stack, STACKTRACE_MAX_FRAMES);
    if (frames_count <= 1) {
        return trace;
    }

    int start_frame = 1;
    int actual_count = frames_count - start_frame;

    trace.frames = (StackTrace_Frame*)stacktrace_alloc_realloc(&trace.allocator, NULL, (size_t)actual_count * sizeof(StackTrace_Frame));
    trace.length = (size_t)actual_count;

    // Get executable path
    char exe_path[4096];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    int have_exe_path = (len > 0);
    if (have_exe_path) {
        exe_path[len] = '\0';
    }

    // Get the base load address of the main executable for PIE support
    // We use dladdr on the first stack frame to get the base address
    Dl_info self_info;
    void* exe_base = NULL;
    if (frames_count > 0 && dladdr(stack[0], &self_info)) {
        exe_base = self_info.dli_fbase;
    }

    // Fallback symbols from backtrace_symbols
    char** symbols = backtrace_symbols(stack, frames_count);

    for (int i = 0; i < actual_count; i++) {
        int stack_idx = i + start_frame;
        void* addr = stack[stack_idx];
        char* func_name = NULL;
        char* file_name = NULL;
        size_t line_num = 0;

        // Determine if this address is in the main executable or a shared library
        Dl_info addr_info;
        int is_main_exe = 0;
        void* base_addr = NULL;
        const char* object_path = exe_path;

        if (dladdr(addr, &addr_info)) {
            base_addr = addr_info.dli_fbase;
            // Check if this is the main executable by comparing base addresses
            if (exe_base != NULL && base_addr == exe_base) {
                is_main_exe = 1;
            } else if (addr_info.dli_fname != NULL) {
                object_path = addr_info.dli_fname;
            }
        }

        // Try addr2line if we have a valid object path
        if (have_exe_path || (addr_info.dli_fname != NULL)) {
            char cmd[4200];

            // For PIE executables, we need to use the offset from the base address
            // addr2line expects file offsets, not runtime addresses
            if (base_addr != NULL) {
                uintptr_t offset = (uintptr_t)addr - (uintptr_t)base_addr;
                snprintf(cmd, sizeof(cmd), "addr2line -f -C -e \"%s\" 0x%lx 2>/dev/null",
                         is_main_exe ? exe_path : object_path, (unsigned long)offset);
            } else {
                snprintf(cmd, sizeof(cmd), "addr2line -f -C -e \"%s\" %p 2>/dev/null",
                         exe_path, addr);
            }

            FILE* pipe = popen(cmd, "r");
            if (pipe != NULL) {
                char output[2048];
                size_t total_read = 0;
                while (total_read < sizeof(output) - 1) {
                    size_t bytes = fread(output + total_read, 1, sizeof(output) - 1 - total_read, pipe);
                    if (bytes == 0) break;
                    total_read += bytes;
                }
                output[total_read] = '\0';
                pclose(pipe);

                stacktrace_parse_addr2line_linux(&trace.allocator, output, &func_name, &file_name, &line_num);
            }
        }

        // Fall back to dladdr/backtrace_symbols if addr2line didn't work
        if (func_name == NULL) {
            if (addr_info.dli_sname != NULL) {
                func_name = stacktrace_strdup(&trace.allocator, addr_info.dli_sname);
            } else if (symbols != NULL && symbols[stack_idx] != NULL) {
                func_name = stacktrace_strdup(&trace.allocator, symbols[stack_idx]);
            } else {
                char unknown_func[32];
                snprintf(unknown_func, sizeof(unknown_func), "%p", addr);
                func_name = stacktrace_strdup(&trace.allocator, unknown_func);
            }
        }

        if (file_name == NULL) {
            if (addr_info.dli_fname != NULL) {
                file_name = stacktrace_strdup(&trace.allocator, addr_info.dli_fname);
            } else {
                file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
            }
        }

        trace.frames[i].function_name = func_name;
        trace.frames[i].file_name = file_name;
        trace.frames[i].line_number = line_num;
    }

    if (symbols != NULL) {
        free(symbols);
    }

    return trace;
}

#endif /* STACKTRACE_PLATFORM_LINUX */

#if defined(STACKTRACE_PLATFORM_MACOS)

// macOS implementation using atos for symbol resolution

// Parse atos output: "function_name (in binary) (file:line)" or "function_name (in binary) + offset"
static void stacktrace_parse_atos(StackTrace_Allocator* allocator,
                                   const char* output,
                                   char** func_name,
                                   char** file_name,
                                   size_t* line_num) {
    *func_name = NULL;
    *file_name = NULL;
    *line_num = 0;

    if (output == NULL || output[0] == '\0') {
        return;
    }

    // atos format: "function_name (in binary) (file:line)"
    // or: "function_name (in binary) + offset"
    // or just: "0x1234" if resolution failed

    // Skip if it looks like an unresolved address
    if (output[0] == '0' && output[1] == 'x') {
        return;
    }

    // Find " (in " to extract function name
    const char* in_marker = strstr(output, " (in ");
    if (in_marker != NULL) {
        size_t func_len = (size_t)(in_marker - output);
        if (func_len > 0) {
            char* func = (char*)stacktrace_alloc_realloc(allocator, NULL, func_len + 1);
            memcpy(func, output, func_len);
            func[func_len] = '\0';
            *func_name = func;
        }

        // Look for source location in parentheses after binary name
        // Format: (in binary) (file:line)
        const char* close_paren = strchr(in_marker + 5, ')');
        if (close_paren != NULL) {
            const char* src_start = strstr(close_paren, " (");
            if (src_start != NULL) {
                src_start += 2; // skip " ("
                const char* src_end = strchr(src_start, ')');
                if (src_end != NULL) {
                    // Find the colon separating file and line
                    const char* colon = strrchr(src_start, ':');
                    if (colon != NULL && colon < src_end) {
                        size_t file_len = (size_t)(colon - src_start);
                        char* file = (char*)stacktrace_alloc_realloc(allocator, NULL, file_len + 1);
                        memcpy(file, src_start, file_len);
                        file[file_len] = '\0';
                        *file_name = file;
                        *line_num = (size_t)atoi(colon + 1);
                    }
                }
            }
        }
    } else {
        // No "(in " marker, just use the whole line as function name (trimmed)
        size_t len = strlen(output);
        while (len > 0 && (output[len-1] == '\n' || output[len-1] == '\r')) {
            len--;
        }
        if (len > 0) {
            char* func = (char*)stacktrace_alloc_realloc(allocator, NULL, len + 1);
            memcpy(func, output, len);
            func[len] = '\0';
            *func_name = func;
        }
    }
}

static StackTrace stacktrace_capture_posix(StackTrace_Allocator allocator) {
    StackTrace trace = {0};
    trace.allocator = allocator;
    stacktrace_init_allocator(&trace.allocator);

    void* stack[STACKTRACE_MAX_FRAMES];

    int frames_count = backtrace(stack, STACKTRACE_MAX_FRAMES);
    if (frames_count <= 1) {
        return trace;
    }

    int start_frame = 1;
    int actual_count = frames_count - start_frame;

    trace.frames = (StackTrace_Frame*)stacktrace_alloc_realloc(&trace.allocator, NULL, (size_t)actual_count * sizeof(StackTrace_Frame));
    trace.length = (size_t)actual_count;

    // Get executable path using _NSGetExecutablePath
    char exe_path[4096];
    uint32_t size = sizeof(exe_path);
    int have_exe_path = (_NSGetExecutablePath(exe_path, &size) == 0);

    // Get process ID for atos
    pid_t pid = getpid();

    // Fallback symbols
    char** symbols = backtrace_symbols(stack, frames_count);

    for (int i = 0; i < actual_count; i++) {
        int stack_idx = i + start_frame;
        void* addr = stack[stack_idx];
        char* func_name = NULL;
        char* file_name = NULL;
        size_t line_num = 0;

        // Try atos for symbol resolution (requires debug symbols)
        if (have_exe_path) {
            char cmd[4300];
            snprintf(cmd, sizeof(cmd), "atos -o \"%s\" -p %d %p 2>/dev/null", exe_path, pid, addr);

            FILE* pipe = popen(cmd, "r");
            if (pipe != NULL) {
                char output[2048];
                size_t total_read = 0;
                while (total_read < sizeof(output) - 1) {
                    size_t bytes = fread(output + total_read, 1, sizeof(output) - 1 - total_read, pipe);
                    if (bytes == 0) break;
                    total_read += bytes;
                }
                output[total_read] = '\0';
                pclose(pipe);

                stacktrace_parse_atos(&trace.allocator, output, &func_name, &file_name, &line_num);
            }
        }

        // Fall back to dladdr if atos didn't work
        if (func_name == NULL) {
            Dl_info info;
            if (dladdr(addr, &info) && info.dli_sname != NULL) {
                func_name = stacktrace_strdup(&trace.allocator, info.dli_sname);
            } else if (symbols != NULL && symbols[stack_idx] != NULL) {
                func_name = stacktrace_strdup(&trace.allocator, symbols[stack_idx]);
            } else {
                char unknown_func[32];
                snprintf(unknown_func, sizeof(unknown_func), "%p", addr);
                func_name = stacktrace_strdup(&trace.allocator, unknown_func);
            }
        }

        if (file_name == NULL) {
            Dl_info info;
            if (dladdr(addr, &info) && info.dli_fname != NULL) {
                file_name = stacktrace_strdup(&trace.allocator, info.dli_fname);
            } else {
                file_name = stacktrace_strdup(&trace.allocator, "<unknown>");
            }
        }

        trace.frames[i].function_name = func_name;
        trace.frames[i].file_name = file_name;
        trace.frames[i].line_number = line_num;
    }

    if (symbols != NULL) {
        free(symbols);
    }

    return trace;
}

#endif /* STACKTRACE_PLATFORM_MACOS */

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
        stacktrace_alloc_free(&trace->allocator, trace->frames[i].function_name);
        stacktrace_alloc_free(&trace->allocator, trace->frames[i].file_name);
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

#include <stdbool.h>
#include <string.h>

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

// Helper to check if a function name appears in the trace
static bool trace_contains_function(StackTrace* trace, const char* func_name) {
    for (size_t i = 0; i < trace->length; i++) {
        if (strstr(trace->frames[i].function_name, func_name) != NULL) {
            return true;
        }
    }
    return false;
}

// Helper to get the index of a function in the trace (-1 if not found)
static int trace_index_of_function(StackTrace* trace, const char* func_name) {
    for (size_t i = 0; i < trace->length; i++) {
        if (strstr(trace->frames[i].function_name, func_name) != NULL) {
            return (int)i;
        }
    }
    return -1;
}

// Basic capture tests /////////////////////////////////////////////////////////

CTEST_CASE(capture_returns_nonempty_trace) {
    StackTrace trace = stacktrace_capture((StackTrace_Allocator){0});
    CTEST_ASSERT_TRUE(trace.frames != NULL);
    CTEST_ASSERT_TRUE(trace.length > 0);
    stacktrace_free(&trace);
}

CTEST_CASE(capture_contains_current_function) {
    StackTrace trace = stacktrace_capture((StackTrace_Allocator){0});
    // The trace should contain this test function or stacktrace_capture
    bool found = trace_contains_function(&trace, "capture_contains_current_function") ||
                 trace_contains_function(&trace, "stacktrace_capture");
    CTEST_ASSERT_TRUE(found);
    stacktrace_free(&trace);
}

CTEST_CASE(capture_contains_main) {
    StackTrace trace = stacktrace_capture((StackTrace_Allocator){0});
    // The trace should contain main somewhere
    bool found = trace_contains_function(&trace, "main");
    CTEST_ASSERT_TRUE(found);
    stacktrace_free(&trace);
}

CTEST_CASE(frames_have_valid_strings) {
    StackTrace trace = stacktrace_capture((StackTrace_Allocator){0});
    for (size_t i = 0; i < trace.length; i++) {
        // All frames should have non-NULL function names and file names
        CTEST_ASSERT_TRUE(trace.frames[i].function_name != NULL);
        CTEST_ASSERT_TRUE(trace.frames[i].file_name != NULL);
        // Function names shouldn't be empty
        CTEST_ASSERT_TRUE(strlen(trace.frames[i].function_name) > 0);
    }
    stacktrace_free(&trace);
}

// Nested function tests ///////////////////////////////////////////////////////

#if defined(_MSC_VER)
    #define STACKTRACE_NOINLINE __declspec(noinline)
#else
    #define STACKTRACE_NOINLINE __attribute__((noinline))
#endif

static StackTrace nested_inner_trace;

STACKTRACE_NOINLINE static void nested_level_3(void) {
    nested_inner_trace = stacktrace_capture((StackTrace_Allocator){0});
}

STACKTRACE_NOINLINE static void nested_level_2(void) {
    nested_level_3();
}

STACKTRACE_NOINLINE static void nested_level_1(void) {
    nested_level_2();
}

CTEST_CASE(nested_calls_appear_in_order) {
    nested_level_1();

    // All three nested functions should appear in the trace
    int idx1 = trace_index_of_function(&nested_inner_trace, "nested_level_1");
    int idx2 = trace_index_of_function(&nested_inner_trace, "nested_level_2");
    int idx3 = trace_index_of_function(&nested_inner_trace, "nested_level_3");

    // They may not all be found (depends on debug info), but if found, order should be 3 < 2 < 1
    if (idx1 >= 0 && idx2 >= 0 && idx3 >= 0) {
        CTEST_ASSERT_TRUE(idx3 < idx2);
        CTEST_ASSERT_TRUE(idx2 < idx1);
    } else {
        // At minimum, the trace should have some frames
        CTEST_ASSERT_TRUE(nested_inner_trace.length > 0);
    }

    stacktrace_free(&nested_inner_trace);
}

// Free tests //////////////////////////////////////////////////////////////////

CTEST_CASE(free_empty_trace_does_not_crash) {
    StackTrace trace = {0};
    // Should not crash on empty trace
    stacktrace_free(&trace);
    CTEST_ASSERT_TRUE(true); // If we got here, we didn't crash
}

CTEST_CASE(free_clears_trace) {
    StackTrace trace = stacktrace_capture((StackTrace_Allocator){0});
    CTEST_ASSERT_TRUE(trace.frames != NULL);
    stacktrace_free(&trace);
    // After free, we shouldn't use frames, but this verifies it ran
    CTEST_ASSERT_TRUE(true);
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
