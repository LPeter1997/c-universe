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

STRING_BUILDER_DEF void string_builder_ensure_capacity(StringBuilder* sb, size_t capacity);
STRING_BUILDER_DEF char* string_builder_build(StringBuilder* sb);
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

// TODO: Includes

// TODO: Local helper macro definitions

#ifdef __cplusplus
extern "C" {
#endif

// TODO: implementation

#ifdef __cplusplus
}
#endif

// TODO: Cleanup local helper macro definitions

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
