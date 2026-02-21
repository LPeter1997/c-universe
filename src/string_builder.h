/**
 * string_builder.h is a single-header string building library for C.
 *
 * Configuration:
 *  - TODO
 *
 * API:
 *  - TODO
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stddef.h>

#ifdef STRING_BUILDER_STATIC
    #define STRING_BUILDER_DEF static
#else
    #define STRING_BUILDER_DEF extern
#endif

#ifndef STRING_BUILDER_REALLOC
    #define STRING_BUILDER_REALLOC realloc
#endif
#ifndef STRING_BUILDER_FREE
    #define STRING_BUILDER_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StringBuilder {
    char* buffer;
    size_t length;
    size_t capacity;
} StringBuilder;

STRING_BUILDER_DEF void string_builder_reserve(StringBuilder* sb, size_t capacity);
STRING_BUILDER_DEF char* string_builder_to_cstr(StringBuilder* sb);
STRING_BUILDER_DEF void string_builder_free(StringBuilder* sb);
STRING_BUILDER_DEF void string_builder_clear(StringBuilder* sb);

STRING_BUILDER_DEF void string_builder_puts(StringBuilder* sb, char const* str);
STRING_BUILDER_DEF void string_builder_putsn(StringBuilder* sb, char const* str, size_t n);
STRING_BUILDER_DEF void string_builder_putc(StringBuilder* sb, char c);
STRING_BUILDER_DEF void string_builder_format(StringBuilder* sb, char const* format, ...);

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

#define STRING_BUILDER_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

void string_builder_reserve(StringBuilder* sb, size_t capacity) {
    if (capacity <= sb->capacity) return;

    size_t newCapacity = (sb->capacity == 0) ? 16 : sb->capacity;
    while (newCapacity < capacity) newCapacity *= 2;
    char* newBuffer = (char*)STRING_BUILDER_REALLOC(sb->buffer, sizeof(char) * newCapacity);
    STRING_BUILDER_ASSERT(newBuffer != NULL, "failed to allocate memory for string builder");
    sb->buffer = newBuffer;
    sb->capacity = newCapacity;
}

char* string_builder_to_cstr(StringBuilder* sb) {
    char* cstr = (char*)STRING_BUILDER_REALLOC(NULL, sizeof(char) * (sb->length + 1));
    STRING_BUILDER_ASSERT(cstr != NULL, "failed to allocate memory for cstring from string builder");
    memcpy(cstr, sb->buffer, sizeof(char) * sb->length);
    cstr[sb->length] = '\0';
    return cstr;
}

void string_builder_free(StringBuilder* sb);
void string_builder_clear(StringBuilder* sb);

void string_builder_puts(StringBuilder* sb, char const* str);
void string_builder_putsn(StringBuilder* sb, char const* str, size_t n);
void string_builder_putc(StringBuilder* sb, char c);
void string_builder_format(StringBuilder* sb, char const* format, ...);

#ifdef __cplusplus
}
#endif

#undef STRING_BUILDER_ASSERT

#endif /* STRING_BUILDER_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef STRING_BUILDER_SELF_TEST

// TODO

#endif /* STRING_BUILDER_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef STRING_BUILDER_EXAMPLE
#undef STRING_BUILDER_EXAMPLE

// TODO

#endif /* STRING_BUILDER_EXAMPLE */
