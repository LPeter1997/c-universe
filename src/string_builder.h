/**
 * string_builder.h is a single-header string building library for C.
 *
 * Configuration:
 *  - #define STRING_BUILDER_IMPLEMENTATION before including this header in exactly one source file to include the implementation section
 *  - #define STRING_BUILDER_STATIC before including this header to make all functions have internal linkage
 *  - #define STRING_BUILDER_ASSERT to use a custom assertion mechanism (by default it uses assert from the C standard library)
 *  - #define STRING_BUILDER_SELF_TEST before including this header to compile a self-test that verifies the library's functionality
 *  - #define STRING_BUILDER_EXAMPLE before including this header to compile a simple example that demonstrates how to use the library
 *
 * StringBuilder API:
 *  - Use sb_puts, sb_putsn, sb_putc and sb_format to append content to the builder
 *  - Use sb_insert, sb_insertn and sb_insertc to insert content at a specific position
 *  - Use sb_remove to remove a portion of the string, and sb_replace to replace all occurrences of a target string
 *  - Use sb_length to get the current length, and sb_char_at to access a character at a specific position
 *  - Use sb_contains and sb_containsc to check if the builder contains a string or character
 *  - Use sb_index_of and sb_index_ofc to find the position of a string or character (-1 if not found)
 *  - Use sb_to_cstr to get a heap-allocated C string with the current content of the builder, which must be freed by the caller
 *  - Use sb_clear to clear the content of the builder without freeing the allocated buffer
 *  - Use sb_free to free the memory allocated for the builder when it is no longer needed
 *
 * CodeBuilder API:
 *  - Similar to StringBuilder but with automatic indentation at the start of lines, useful for code generation
 *  - Use code_builder_puts, code_builder_putc and code_builder_format just like StringBuilder, but with automatic indentation at line starts
 *  - Use code_builder_indent and code_builder_dedent to increase/decrease the indentation level
 *  - Use code_builder_to_cstr and code_builder_free to get the result and clean up
 *  - Customize the indentation string by setting the indent_str field of CodeBuilder (defaults to 4 spaces if NULL)
 *
 * Check the example section at the end of this file for a full example.
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef STRING_BUILDER_STATIC
    #define STRING_BUILDER_DEF static
#else
    #define STRING_BUILDER_DEF extern
#endif

#ifndef STRING_BUILDER_ASSERT
    #define STRING_BUILDER_ASSERT(condition, message) assert(((void)message, condition))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A simple dynamic string builder.
 */
typedef struct StringBuilder {
    // The buffer containing the string content, not necessarily null-terminated
    char* buffer;
    // The current length of the string content in the buffer
    size_t length;
    // The total capacity of the buffer
    size_t capacity;
    // Optional customization for memory allocation
    struct {
        void* context;
        void* (*realloc)(void* ctx, void* ptr, size_t new_size);
        void (*free)(void* ctx, void* ptr);
    } allocator;
} StringBuilder;

/**
 * Ensures the builder has at least the given capacity, growing it if needed.
 * @param sb The string builder to reserve capacity for.
 * @param capacity The minimum capacity to ensure.
 */
STRING_BUILDER_DEF void sb_reserve(StringBuilder* sb, size_t capacity);

/**
 * Converts the current content of the builder to a null-terminated C string.
 * The returned string is heap-allocated and must be freed by the caller.
 * @param sb The string builder to convert.
 * @return A null-terminated C string with the current content of the builder.
 */
STRING_BUILDER_DEF char* sb_to_cstr(StringBuilder* sb);

/**
 * Frees the memory allocated for the builder and resets its state.
 * @param sb The string builder to free.
 */
STRING_BUILDER_DEF void sb_free(StringBuilder* sb);

/**
 * Clears the content of the builder without freeing the allocated buffer.
 * @param sb The string builder to clear.
 */
STRING_BUILDER_DEF void sb_clear(StringBuilder* sb);

/**
 * Appends a null-terminated string to the builder.
 * @param sb The string builder to append to.
 * @param str The null-terminated string to append.
 */
STRING_BUILDER_DEF void sb_puts(StringBuilder* sb, char const* str);

/**
 * Appends a string with the given length to the builder.
 * @param sb The string builder to append to.
 * @param str The string to append, not necessarily null-terminated.
 * @param n The length of the string to append.
 */
STRING_BUILDER_DEF void sb_putsn(StringBuilder* sb, char const* str, size_t n);

/**
 * Appends a single character to the builder.
 * @param sb The string builder to append to.
 * @param c The character to append.
 */
STRING_BUILDER_DEF void sb_putc(StringBuilder* sb, char c);

/**
 * Appends formatted content to the builder, similar to printf.
 * @param sb The string builder to append to.
 * @param format The format string, similar to printf.
 * @param ... The arguments for the format string.
 */
STRING_BUILDER_DEF void sb_format(StringBuilder* sb, char const* format, ...);

/**
 * Same as @see sb_format but takes a va_list instead of variadic arguments.
 * @param sb The string builder to append to.
 * @param format The format string, similar to printf.
 * @param args The va_list of arguments for the format string.
 */
STRING_BUILDER_DEF void sb_vformat(StringBuilder* sb, char const* format, va_list args);

/**
 * Inserts a null-terminated string at the specified position in the builder.
 * @param sb The string builder to insert into.
 * @param pos The position at which to insert the string.
 * @param str The null-terminated string to insert.
 */
STRING_BUILDER_DEF void sb_insert(StringBuilder* sb, size_t pos, char const* str);

/**
 * Inserts a string with the given length at the specified position in the builder.
 * @param sb The string builder to insert into.
 * @param pos The position at which to insert the string.
 * @param str The string to insert, not necessarily null-terminated.
 * @param n The length of the string to insert.
 */
STRING_BUILDER_DEF void sb_insertn(StringBuilder* sb, size_t pos, char const* str, size_t n);

/**
 * Inserts a single character at the specified position in the builder.
 * @param sb The string builder to insert into.
 * @param pos The position at which to insert the character.
 * @param c The character to insert.
 */
STRING_BUILDER_DEF void sb_insertc(StringBuilder* sb, size_t pos, char c);

/**
 * Returns the current length of the string in the builder.
 * @param sb The string builder to get the length of.
 * @return The current length of the string content.
 */
STRING_BUILDER_DEF size_t sb_length(StringBuilder* sb);

/**
 * Returns a pointer to the character at the specified position in the builder.
 * This allows reading or modifying the character at that position.
 * @param sb The string builder to access.
 * @param pos The position of the character to access.
 * @return A pointer to the character at the specified position.
 */
STRING_BUILDER_DEF char* sb_char_at(StringBuilder* sb, size_t pos);

/**
 * Removes a portion of the string from the builder.
 * @param sb The string builder to remove from.
 * @param pos The position at which to start removing.
 * @param length The number of characters to remove.
 */
STRING_BUILDER_DEF void sb_remove(StringBuilder* sb, size_t pos, size_t length);

/**
 * Replaces all occurrences of a target string with a replacement string in the builder.
 * @param sb The string builder to perform the replacement in.
 * @param target The string to be replaced.
 * @param replacement The string to replace the target with.
 */
STRING_BUILDER_DEF void sb_replace(StringBuilder* sb, char const* target, char const* replacement);

/**
 * Checks if the builder contains a given string.
 * @param sb The string builder to search in.
 * @param str The null-terminated string to search for.
 * @return true if the string is found, false otherwise.
 */
STRING_BUILDER_DEF bool sb_contains(StringBuilder* sb, char const* str);

/**
 * Checks if the builder contains a given character.
 * @param sb The string builder to search in.
 * @param c The character to search for.
 * @return true if the character is found, false otherwise.
 */
STRING_BUILDER_DEF bool sb_containsc(StringBuilder* sb, char c);

/**
 * Finds the first occurrence of a string in the builder.
 * @param sb The string builder to search in.
 * @param str The null-terminated string to search for.
 * @return The index of the first occurrence, or -1 if not found.
 */
STRING_BUILDER_DEF int sb_index_of(StringBuilder* sb, char const* str);

/**
 * Finds the first occurrence of a character in the builder.
 * @param sb The string builder to search in.
 * @param c The character to search for.
 * @return The index of the first occurrence, or -1 if not found.
 */
STRING_BUILDER_DEF int sb_index_ofc(StringBuilder* sb, char c);

/**
 * Utility for building code with indentation, using an underlying string builder.
 * Useful for code generation where the goal is producing a somewhat nicely formatted output.
 */
typedef struct CodeBuilder {
    // The underlying string builder for the code content
    StringBuilder builder;
    // The current indentation level
    size_t indent_level;
    // The indentation string
    char const* indent_str;
} CodeBuilder;

STRING_BUILDER_DEF void code_builder_reserve(CodeBuilder* cb, size_t capacity);
STRING_BUILDER_DEF char* code_builder_to_cstr(CodeBuilder* cb);
STRING_BUILDER_DEF void code_builder_free(CodeBuilder* cb);
STRING_BUILDER_DEF void code_builder_clear(CodeBuilder* cb);
STRING_BUILDER_DEF void code_builder_puts(CodeBuilder* cb, char const* str);
STRING_BUILDER_DEF void code_builder_putsn(CodeBuilder* cb, char const* str, size_t n);
STRING_BUILDER_DEF void code_builder_putc(CodeBuilder* cb, char c);
STRING_BUILDER_DEF void code_builder_format(CodeBuilder* cb, char const* format, ...);
STRING_BUILDER_DEF void code_builder_vformat(CodeBuilder* cb, char const* format, va_list args);
STRING_BUILDER_DEF void code_builder_indent(CodeBuilder* cb);
STRING_BUILDER_DEF void code_builder_dedent(CodeBuilder* cb);

#ifdef __cplusplus
}
#endif

#endif /* STRING_BUILDER_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef STRING_BUILDER_IMPLEMENTATION

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

static void* sb_default_realloc(void* ctx, void* ptr, size_t new_size) {
    (void)ctx;
    return realloc(ptr, new_size);
}

static void sb_default_free(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}

static void sb_init_allocator(StringBuilder* sb) {
    if (sb->allocator.realloc != NULL || sb->allocator.free != NULL) {
        STRING_BUILDER_ASSERT(sb->allocator.realloc != NULL && sb->allocator.free != NULL, "both realloc and free function pointers must be set in allocator");
        return;
    }
    sb->allocator.realloc = sb_default_realloc;
    sb->allocator.free = sb_default_free;
}

// String builder //////////////////////////////////////////////////////////////

void sb_reserve(StringBuilder* sb, size_t capacity) {
    if (capacity <= sb->capacity) return;

    size_t newCapacity = (sb->capacity == 0) ? 16 : sb->capacity;
    while (newCapacity < capacity) newCapacity *= 2;
    sb_init_allocator(sb);
    char* newBuffer = (char*)sb->allocator.realloc(sb->allocator.context, sb->buffer, sizeof(char) * newCapacity);
    STRING_BUILDER_ASSERT(newBuffer != NULL, "failed to allocate memory for string builder");
    sb->buffer = newBuffer;
    sb->capacity = newCapacity;
}

char* sb_to_cstr(StringBuilder* sb) {
    sb_init_allocator(sb);
    char* cstr = (char*)sb->allocator.realloc(sb->allocator.context, NULL, sizeof(char) * (sb->length + 1));
    STRING_BUILDER_ASSERT(cstr != NULL, "failed to allocate memory for cstring from string builder");
    memcpy(cstr, sb->buffer, sizeof(char) * sb->length);
    cstr[sb->length] = '\0';
    return cstr;
}

void sb_free(StringBuilder* sb) {
    sb_init_allocator(sb);
    sb->allocator.free(sb->allocator.context, sb->buffer);
    sb->buffer = NULL;
    sb->length = 0;
    sb->capacity = 0;
}

void sb_clear(StringBuilder* sb) {
    sb->length = 0;
}

void sb_puts(StringBuilder* sb, char const* str) {
    size_t strLength = strlen(str);
    sb_putsn(sb, str, strLength);
}

void sb_putsn(StringBuilder* sb, char const* str, size_t n) {
    sb_reserve(sb, sb->length + n);
    memcpy(sb->buffer + sb->length, str, sizeof(char) * n);
    sb->length += n;
}

void sb_putc(StringBuilder* sb, char c) {
    sb_reserve(sb, sb->length + 1);
    sb->buffer[sb->length] = c;
    sb->length += 1;
}

void sb_format(StringBuilder* sb, char const* format, ...) {
    va_list args;
    va_start(args, format);
    sb_vformat(sb, format, args);
    va_end(args);
}

void sb_vformat(StringBuilder* sb, char const* format, va_list args) {
    va_list args_copy;
    va_copy(args_copy, args);
    int formattedLength = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    sb_reserve(sb, sb->length + (size_t)formattedLength);
    vsnprintf(sb->buffer + sb->length, (size_t)formattedLength + 1, format, args);
    sb->length += (size_t)formattedLength;
}

void sb_insert(StringBuilder* sb, size_t pos, char const* str) {
    size_t strLength = strlen(str);
    sb_insertn(sb, pos, str, strLength);
}

void sb_insertn(StringBuilder* sb, size_t pos, char const* str, size_t n) {
    if (n == 0) return;
    STRING_BUILDER_ASSERT(pos <= sb->length, "insert position out of bounds");
    sb_reserve(sb, sb->length + n);
    memmove(sb->buffer + pos + n, sb->buffer + pos, sb->length - pos);
    memcpy(sb->buffer + pos, str, n);
    sb->length += n;
}

void sb_insertc(StringBuilder* sb, size_t pos, char c) {
    STRING_BUILDER_ASSERT(pos <= sb->length, "insert position out of bounds");
    sb_reserve(sb, sb->length + 1);
    memmove(sb->buffer + pos + 1, sb->buffer + pos, sb->length - pos);
    sb->buffer[pos] = c;
    sb->length += 1;
}

size_t sb_length(StringBuilder* sb) {
    return sb->length;
}

char* sb_char_at(StringBuilder* sb, size_t pos) {
    STRING_BUILDER_ASSERT(pos < sb->length, "char_at position out of bounds");
    return &sb->buffer[pos];
}

void sb_remove(StringBuilder* sb, size_t pos, size_t length) {
    if (length == 0) return;
    STRING_BUILDER_ASSERT(pos <= sb->length, "remove position out of bounds");
    if (pos + length > sb->length) length = sb->length - pos;
    memmove(sb->buffer + pos, sb->buffer + pos + length, sb->length - pos - length);
    sb->length -= length;
}

void sb_replace(StringBuilder* sb, char const* target, char const* replacement) {
    size_t targetLen = strlen(target);
    // Avoid replacing empty strings, just makes no sense
    if (targetLen == 0) return;
    size_t replacementLen = strlen(replacement);
    size_t pos = 0;
    while (pos + targetLen <= sb->length) {
        if (memcmp(sb->buffer + pos, target, targetLen) != 0) {
            ++pos;
            continue;
        }
        if (targetLen == replacementLen) {
            // Same length, no shifting needed
            memcpy(sb->buffer + pos, replacement, replacementLen);
        } else if (replacementLen < targetLen) {
            // Replacement is shorter, copy replacement, then shift rest left
            size_t diff = targetLen - replacementLen;
            memcpy(sb->buffer + pos, replacement, replacementLen);
            memmove(sb->buffer + pos + replacementLen, sb->buffer + pos + targetLen, sb->length - pos - targetLen);
            sb->length -= diff;
        } else {
            // Replacement is longer, reserve, shift rest right, then copy replacement
            size_t diff = replacementLen - targetLen;
            sb_reserve(sb, sb->length + diff);
            memmove(sb->buffer + pos + replacementLen, sb->buffer + pos + targetLen, sb->length - pos - targetLen);
            memcpy(sb->buffer + pos, replacement, replacementLen);
            sb->length += diff;
        }
        pos += replacementLen;
    }
}

bool sb_contains(StringBuilder* sb, char const* str) {
    return sb_index_of(sb, str) >= 0;
}

bool sb_containsc(StringBuilder* sb, char c) {
    return sb_index_ofc(sb, c) >= 0;
}

int sb_index_of(StringBuilder* sb, char const* str) {
    size_t strLen = strlen(str);
    if (strLen == 0) return 0; // Empty string is found at position 0
    if (strLen > sb->length) return -1;
    for (size_t pos = 0; pos + strLen <= sb->length; ++pos) {
        if (memcmp(sb->buffer + pos, str, strLen) == 0) {
            return (int)pos;
        }
    }
    return -1;
}

int sb_index_ofc(StringBuilder* sb, char c) {
    for (size_t pos = 0; pos < sb->length; ++pos) {
        if (sb->buffer[pos] == c) {
            return (int)pos;
        }
    }
    return -1;
}

// Code builder ////////////////////////////////////////////////////////////////

static void code_builder_indent_if_needed(CodeBuilder* cb) {
    StringBuilder* sb = &cb->builder;
    if (sb->length == 0 || sb->buffer[sb->length - 1] == '\n' || sb->buffer[sb->length - 1] == '\r') {
        char const* indent = cb->indent_str == NULL ? "    " : cb->indent_str;
        for (size_t i = 0; i < cb->indent_level; ++i) {
            sb_puts(sb, indent);
        }
    }
}

static size_t code_builder_line_length(char const* str, size_t maxLength) {
    size_t i = 0;
    while (i < maxLength) {
        char ch = str[i];
        if (ch == '\0') break;
        if (ch == '\n') {
            ++i; // Include the newline in the length
            break;
        }
        if (ch == '\r') {
            ++i; // Include the carriage return in the length
            if (i < maxLength && str[i] == '\n') {
                ++i; // Include the newline in the length if it's a CRLF sequence
            }
            break;
        }
        ++i;
    }
    return i;
}

void code_builder_reserve(CodeBuilder* cb, size_t capacity) {
    sb_reserve(&cb->builder, capacity);
}

char* code_builder_to_cstr(CodeBuilder* cb) {
    return sb_to_cstr(&cb->builder);
}

void code_builder_free(CodeBuilder* cb) {
    sb_free(&cb->builder);
}
void code_builder_clear(CodeBuilder* cb) {
    sb_clear(&cb->builder);
}

void code_builder_puts(CodeBuilder* cb, char const* str) {
    size_t strLength = strlen(str);
    code_builder_putsn(cb, str, strLength);
}

void code_builder_putsn(CodeBuilder* cb, char const* str, size_t n) {
    // Each line is indented separately
    while (n > 0) {
        size_t lineLength = code_builder_line_length(str, n);
        code_builder_indent_if_needed(cb);
        sb_putsn(&cb->builder, str, lineLength);
        str += lineLength;
        n -= lineLength;
    }
}

void code_builder_putc(CodeBuilder* cb, char c) {
    code_builder_indent_if_needed(cb);
    sb_putc(&cb->builder, c);
}

void code_builder_format(CodeBuilder* cb, char const* format, ...) {
    va_list args;
    va_start(args, format);
    code_builder_vformat(cb, format, args);
    va_end(args);
}

void code_builder_vformat(CodeBuilder* cb, char const* format, va_list args) {
    // Each line is indented separately, so we format into a temporary buffer first, then putsn that buffer
    va_list args_copy;
    va_copy(args_copy, args);
    int formattedLength = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    STRING_BUILDER_ASSERT(formattedLength >= 0, "failed to compute formatted string length in code builder");
    sb_init_allocator(&cb->builder);
    char* formattedStr = (char*)cb->builder.allocator.realloc(cb->builder.allocator.context, NULL, sizeof(char) * ((size_t)formattedLength + 1));
    STRING_BUILDER_ASSERT(formattedStr != NULL, "failed to allocate memory for formatted string in code builder");
    vsnprintf(formattedStr, (size_t)formattedLength + 1, format, args);
    code_builder_putsn(cb, formattedStr, (size_t)formattedLength);
    cb->builder.allocator.free(cb->builder.allocator.context, formattedStr);
}

void code_builder_indent(CodeBuilder* cb) {
    ++cb->indent_level;
}

void code_builder_dedent(CodeBuilder* cb) {
    STRING_BUILDER_ASSERT(cb->indent_level > 0, "cannot dedent code builder, already at indent level 0");
    --cb->indent_level;
}

#ifdef __cplusplus
}
#endif

#endif /* STRING_BUILDER_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef STRING_BUILDER_SELF_TEST

// Use our own test framework
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

static StringBuilder test_sb_create(void) {
    StringBuilder sb = { 0 };
    return sb;
}

static bool test_sb_equals(StringBuilder* sb, char const* expected) {
    size_t expectedLen = strlen(expected);
    if (sb->length != expectedLen) return false;
    return memcmp(sb->buffer, expected, expectedLen) == 0;
}

// Initialization tests ////////////////////////////////////////////////////////

CTEST_CASE(string_builder_empty_on_init) {
    StringBuilder sb = test_sb_create();
    CTEST_ASSERT_TRUE(sb.buffer == NULL);
    CTEST_ASSERT_TRUE(sb.length == 0);
    CTEST_ASSERT_TRUE(sb.capacity == 0);
}

// Reserve tests ///////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_reserve_allocates_memory) {
    StringBuilder sb = test_sb_create();
    sb_reserve(&sb, 32);
    CTEST_ASSERT_TRUE(sb.buffer != NULL);
    CTEST_ASSERT_TRUE(sb.capacity >= 32);
    CTEST_ASSERT_TRUE(sb.length == 0);
    sb_free(&sb);
}

CTEST_CASE(string_builder_reserve_grows_exponentially) {
    StringBuilder sb = test_sb_create();
    sb_reserve(&sb, 1);
    size_t initialCapacity = sb.capacity;
    CTEST_ASSERT_TRUE(initialCapacity >= 16);
    sb_reserve(&sb, 100);
    CTEST_ASSERT_TRUE(sb.capacity >= 100);
    CTEST_ASSERT_TRUE(sb.capacity > initialCapacity);
    sb_free(&sb);
}

CTEST_CASE(string_builder_reserve_no_shrink) {
    StringBuilder sb = test_sb_create();
    sb_reserve(&sb, 64);
    size_t cap = sb.capacity;
    sb_reserve(&sb, 32);
    CTEST_ASSERT_TRUE(sb.capacity == cap);
    sb_free(&sb);
}

// Puts tests //////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_puts_appends_string) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(sb.length == 5);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_puts_multiple) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    sb_puts(&sb, ", ");
    sb_puts(&sb, "World!");
    CTEST_ASSERT_TRUE(sb.length == 13);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello, World!"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_puts_empty_string) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "");
    CTEST_ASSERT_TRUE(sb.length == 0);
    sb_puts(&sb, "test");
    sb_puts(&sb, "");
    CTEST_ASSERT_TRUE(sb.length == 4);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "test"));
    sb_free(&sb);
}

// Putsn tests /////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_putsn_appends_partial_string) {
    StringBuilder sb = test_sb_create();
    sb_putsn(&sb, "Hello, World!", 5);
    CTEST_ASSERT_TRUE(sb.length == 5);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_putsn_zero_length) {
    StringBuilder sb = test_sb_create();
    sb_putsn(&sb, "test", 0);
    CTEST_ASSERT_TRUE(sb.length == 0);
    sb_free(&sb);
}

CTEST_CASE(string_builder_putsn_exact_length) {
    StringBuilder sb = test_sb_create();
    sb_putsn(&sb, "test", 4);
    CTEST_ASSERT_TRUE(sb.length == 4);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "test"));
    sb_free(&sb);
}

// Putc tests //////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_putc_appends_char) {
    StringBuilder sb = test_sb_create();
    sb_putc(&sb, 'A');
    CTEST_ASSERT_TRUE(sb.length == 1);
    CTEST_ASSERT_TRUE(sb.buffer[0] == 'A');
    sb_free(&sb);
}

CTEST_CASE(string_builder_putc_multiple) {
    StringBuilder sb = test_sb_create();
    sb_putc(&sb, 'H');
    sb_putc(&sb, 'i');
    sb_putc(&sb, '!');
    CTEST_ASSERT_TRUE(sb.length == 3);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hi!"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_putc_null_char) {
    StringBuilder sb = test_sb_create();
    sb_putc(&sb, 'A');
    sb_putc(&sb, '\0');
    sb_putc(&sb, 'B');
    CTEST_ASSERT_TRUE(sb.length == 3);
    CTEST_ASSERT_TRUE(sb.buffer[0] == 'A');
    CTEST_ASSERT_TRUE(sb.buffer[1] == '\0');
    CTEST_ASSERT_TRUE(sb.buffer[2] == 'B');
    sb_free(&sb);
}

// Format tests ////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_format_simple_string) {
    StringBuilder sb = test_sb_create();
    sb_format(&sb, "Hello, %s!", "World");
    CTEST_ASSERT_TRUE(sb.length == 13);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello, World!"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_format_integer) {
    StringBuilder sb = test_sb_create();
    sb_format(&sb, "Value: %d", 42);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Value: 42"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_format_multiple_args) {
    StringBuilder sb = test_sb_create();
    sb_format(&sb, "%s=%d, %s=%d", "x", 10, "y", 20);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "x=10, y=20"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_format_append_after_puts) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Count: ");
    sb_format(&sb, "%d items", 5);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Count: 5 items"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_format_empty_format) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    sb_format(&sb, "");
    CTEST_ASSERT_TRUE(sb.length == 4);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "test"));
    sb_free(&sb);
}

// To cstring tests ////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_to_cstr_creates_null_terminated) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    char* cstr = sb_to_cstr(&sb);
    CTEST_ASSERT_TRUE(cstr != NULL);
    CTEST_ASSERT_TRUE(strlen(cstr) == 5);
    CTEST_ASSERT_TRUE(strcmp(cstr, "Hello") == 0);
    free(cstr);
    sb_free(&sb);
}

CTEST_CASE(string_builder_to_cstr_empty_builder) {
    StringBuilder sb = test_sb_create();
    char* cstr = sb_to_cstr(&sb);
    CTEST_ASSERT_TRUE(cstr != NULL);
    CTEST_ASSERT_TRUE(strlen(cstr) == 0);
    CTEST_ASSERT_TRUE(cstr[0] == '\0');
    free(cstr);
}

CTEST_CASE(string_builder_to_cstr_independent_copy) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    char* cstr = sb_to_cstr(&sb);
    sb_puts(&sb, " World");
    CTEST_ASSERT_TRUE(strcmp(cstr, "Hello") == 0);
    free(cstr);
    sb_free(&sb);
}

// Clear tests /////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_clear_resets_length) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello, World!");
    size_t capBefore = sb.capacity;
    sb_clear(&sb);
    CTEST_ASSERT_TRUE(sb.length == 0);
    CTEST_ASSERT_TRUE(sb.capacity == capBefore);
    sb_free(&sb);
}

CTEST_CASE(string_builder_clear_allows_reuse) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "First");
    sb_clear(&sb);
    sb_puts(&sb, "Second");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Second"));
    sb_free(&sb);
}

// Free tests //////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_free_resets_all) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    sb_free(&sb);
    CTEST_ASSERT_TRUE(sb.buffer == NULL);
    CTEST_ASSERT_TRUE(sb.length == 0);
    CTEST_ASSERT_TRUE(sb.capacity == 0);
}

CTEST_CASE(string_builder_free_empty_builder) {
    StringBuilder sb = test_sb_create();
    sb_free(&sb); // Should not crash
    CTEST_ASSERT_TRUE(sb.buffer == NULL);
}

// Combined operations tests ///////////////////////////////////////////////////

CTEST_CASE(string_builder_mixed_operations) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Name: ");
    sb_format(&sb, "%s", "John");
    sb_puts(&sb, ", Age: ");
    sb_format(&sb, "%d", 30);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Name: John, Age: 30"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_build_path) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "/home");
    sb_putc(&sb, '/');
    sb_puts(&sb, "user");
    sb_putc(&sb, '/');
    sb_puts(&sb, "documents");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "/home/user/documents"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_large_content) {
    StringBuilder sb = test_sb_create();
    for (int i = 0; i < 1000; ++i) {
        sb_putc(&sb, 'A');
    }
    CTEST_ASSERT_TRUE(sb.length == 1000);
    CTEST_ASSERT_TRUE(sb.capacity >= 1000);
    for (size_t i = 0; i < sb.length; ++i) {
        CTEST_ASSERT_TRUE(sb.buffer[i] == 'A');
    }
    sb_free(&sb);
}

CTEST_CASE(string_builder_repeated_clear_and_build) {
    StringBuilder sb = test_sb_create();
    for (int i = 0; i < 10; ++i) {
        sb_format(&sb, "iteration %d", i);
        sb_clear(&sb);
    }
    sb_puts(&sb, "final");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "final"));
    sb_free(&sb);
}

// Insert tests ////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_insert_at_beginning) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "World");
    sb_insert(&sb, 0, "Hello ");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello World"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insert_at_end) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    sb_insert(&sb, 5, " World");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello World"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insert_in_middle) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Helo");
    sb_insert(&sb, 2, "l");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insertn_partial) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "AC");
    sb_insertn(&sb, 1, "BXYZ", 1);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "ABC"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insert_empty_string) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    sb_insert(&sb, 2, "");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "test"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insertc_beginning) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "ello");
    sb_insertc(&sb, 0, 'H');
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insertc_middle) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hllo");
    sb_insertc(&sb, 1, 'e');
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_insertc_end) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hell");
    sb_insertc(&sb, 4, 'o');
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

// Length and char_at tests ////////////////////////////////////////////////////

CTEST_CASE(string_builder_length_empty) {
    StringBuilder sb = test_sb_create();
    CTEST_ASSERT_TRUE(sb_length(&sb) == 0);
    sb_free(&sb);
}

CTEST_CASE(string_builder_length_after_puts) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(sb_length(&sb) == 5);
    sb_puts(&sb, " World");
    CTEST_ASSERT_TRUE(sb_length(&sb) == 11);
    sb_free(&sb);
}

CTEST_CASE(string_builder_char_at_read) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(*sb_char_at(&sb, 0) == 'H');
    CTEST_ASSERT_TRUE(*sb_char_at(&sb, 1) == 'e');
    CTEST_ASSERT_TRUE(*sb_char_at(&sb, 4) == 'o');
    sb_free(&sb);
}

CTEST_CASE(string_builder_char_at_write) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hallo");
    *sb_char_at(&sb, 1) = 'e';
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

// Remove tests ////////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_remove_from_beginning) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    sb_remove(&sb, 0, 6);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "World"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_remove_from_end) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    sb_remove(&sb, 5, 6);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_remove_from_middle) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Helllo");
    sb_remove(&sb, 2, 1);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_remove_zero_length) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    sb_remove(&sb, 2, 0);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "test"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_remove_clamps_to_end) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    sb_remove(&sb, 3, 100);
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hel"));
    sb_free(&sb);
}

// Replace tests ///////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_replace_single_occurrence) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    sb_replace(&sb, "World", "Universe");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello Universe"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_multiple_occurrences) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "foo bar foo baz foo");
    sb_replace(&sb, "foo", "qux");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "qux bar qux baz qux"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_with_shorter) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    sb_replace(&sb, "World", "You");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello You"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_with_longer) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hi");
    sb_replace(&sb, "Hi", "Hello");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_with_same_length) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "cat and dog");
    sb_replace(&sb, "cat", "bat");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "bat and dog"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_no_match) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    sb_replace(&sb, "xyz", "abc");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello World"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_empty_target) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    sb_replace(&sb, "", "x");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "test"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_replace_with_empty) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    sb_replace(&sb, " World", "");
    CTEST_ASSERT_TRUE(test_sb_equals(&sb, "Hello"));
    sb_free(&sb);
}

// Contains tests //////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_contains_finds_string) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    CTEST_ASSERT_TRUE(sb_contains(&sb, "World"));
    CTEST_ASSERT_TRUE(sb_contains(&sb, "Hello"));
    CTEST_ASSERT_TRUE(sb_contains(&sb, "o W"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_contains_not_found) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    CTEST_ASSERT_TRUE(!sb_contains(&sb, "xyz"));
    CTEST_ASSERT_TRUE(!sb_contains(&sb, "HELLO"));
    sb_free(&sb);
}

CTEST_CASE(string_builder_contains_empty_string) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    CTEST_ASSERT_TRUE(sb_contains(&sb, ""));
    sb_free(&sb);
}

CTEST_CASE(string_builder_containsc_finds) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(sb_containsc(&sb, 'H'));
    CTEST_ASSERT_TRUE(sb_containsc(&sb, 'e'));
    CTEST_ASSERT_TRUE(sb_containsc(&sb, 'o'));
    sb_free(&sb);
}

CTEST_CASE(string_builder_containsc_not_found) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(!sb_containsc(&sb, 'x'));
    CTEST_ASSERT_TRUE(!sb_containsc(&sb, 'h'));
    sb_free(&sb);
}

CTEST_CASE(string_builder_contains_in_empty_builder) {
    StringBuilder sb = test_sb_create();
    CTEST_ASSERT_TRUE(!sb_contains(&sb, "test"));
    CTEST_ASSERT_TRUE(!sb_containsc(&sb, 'a'));
    CTEST_ASSERT_TRUE(sb_contains(&sb, ""));
    sb_free(&sb);
}

// Index of tests //////////////////////////////////////////////////////////////

CTEST_CASE(string_builder_index_of_finds_at_start) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "Hello") == 0);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_of_finds_in_middle) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "o W") == 4);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_of_finds_at_end) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "World") == 6);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_of_not_found) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello World");
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "xyz") == -1);
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "Worlds") == -1);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_of_empty_string) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "test");
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "") == 0);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_of_first_occurrence) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "abcabc");
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "bc") == 1);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_ofc_finds) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'H') == 0);
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'e') == 1);
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'o') == 4);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_ofc_not_found) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "Hello");
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'x') == -1);
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'h') == -1);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_ofc_first_occurrence) {
    StringBuilder sb = test_sb_create();
    sb_puts(&sb, "abcabc");
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'b') == 1);
    sb_free(&sb);
}

CTEST_CASE(string_builder_index_of_in_empty_builder) {
    StringBuilder sb = test_sb_create();
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "test") == -1);
    CTEST_ASSERT_TRUE(sb_index_ofc(&sb, 'a') == -1);
    CTEST_ASSERT_TRUE(sb_index_of(&sb, "") == 0);
    sb_free(&sb);
}

// Code builder tests //////////////////////////////////////////////////////////

CTEST_CASE(code_builder_no_indent_at_level_zero) {
    CodeBuilder cb = { 0 };
    code_builder_puts(&cb, "hello");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "hello") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_indents_on_fresh_buffer) {
    CodeBuilder cb = { 0 };
    code_builder_indent(&cb);
    code_builder_puts(&cb, "line1\nline2");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "    line1\n    line2") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_nested_indentation) {
    CodeBuilder cb = { 0 };
    code_builder_puts(&cb, "func {\n");
    code_builder_indent(&cb);
    code_builder_puts(&cb, "body;\n");
    code_builder_dedent(&cb);
    code_builder_puts(&cb, "}");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "func {\n    body;\n}") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_format_with_indent) {
    CodeBuilder cb = { 0 };
    code_builder_indent(&cb);
    code_builder_format(&cb, "x = %d;\ny = %d;", 1, 2);
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "    x = 1;\n    y = 2;") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_multiple_indent_levels) {
    CodeBuilder cb = { 0 };
    code_builder_puts(&cb, "a\n");
    code_builder_indent(&cb);
    code_builder_puts(&cb, "b\n");
    code_builder_indent(&cb);
    code_builder_puts(&cb, "c");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "a\n    b\n        c") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_custom_indent_string) {
    CodeBuilder cb = { .indent_str = "\t" };
    code_builder_indent(&cb);
    code_builder_puts(&cb, "a\nb");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "\ta\n\tb") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_crlf_handling) {
    CodeBuilder cb = { 0 };
    code_builder_indent(&cb);
    code_builder_puts(&cb, "line1\r\nline2");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "    line1\r\n    line2") == 0);
    free(result);
    code_builder_free(&cb);
}

CTEST_CASE(code_builder_clear_preserves_indent_level) {
    CodeBuilder cb = { 0 };
    code_builder_indent(&cb);
    code_builder_puts(&cb, "test");
    code_builder_clear(&cb);
    code_builder_puts(&cb, "new\nline");
    char* result = code_builder_to_cstr(&cb);
    CTEST_ASSERT_TRUE(strcmp(result, "    new\n    line") == 0);
    free(result);
    code_builder_free(&cb);
}

#endif /* STRING_BUILDER_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef STRING_BUILDER_EXAMPLE
#undef STRING_BUILDER_EXAMPLE

#include <stdio.h>
#include <stdlib.h>

#define STRING_BUILDER_STATIC
#define STRING_BUILDER_IMPLEMENTATION
#include "string_builder.h"

int main(void) {
    // StringBuilder: simple string concatenation
    StringBuilder sb = { 0 };
    sb_puts(&sb, "Hello, ");
    sb_puts(&sb, "World!");
    sb_format(&sb, " The answer is %d.", 42);
    char* msg = sb_to_cstr(&sb);
    printf("%s\n\n", msg);
    free(msg);
    sb_free(&sb);

    // CodeBuilder: code generation with automatic indentation
    CodeBuilder cb = { 0 };
    code_builder_puts(&cb, "int main(void) {\n");
    code_builder_indent(&cb);
    code_builder_format(&cb, "printf(\"%s\");\n", "Hello");
    code_builder_puts(&cb, "return 0;\n");
    code_builder_dedent(&cb);
    code_builder_puts(&cb, "}\n");
    char* code = code_builder_to_cstr(&cb);
    printf("%s", code);
    free(code);
    code_builder_free(&cb);

    return 0;
}

#endif /* STRING_BUILDER_EXAMPLE */
