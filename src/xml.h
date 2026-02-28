/**
 * xml.h is a single-header TODO.
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
#ifndef XML_H
#define XML_H

#include <stddef.h>

#ifdef XML_STATIC
    #define XML_DEF static
#else
    #define XML_DEF extern
#endif

#ifndef XML_ASSERT
    #define XML_ASSERT(condition, message) ((void)message, (condition))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Xml_Allocator {
    // Context pointer that will be passed to the realloc and free functions
    void* context;
    // A function pointer for reallocating memory, with the same semantics as the standard realloc but with an additional context parameter
    void*(*realloc)(void* ctx, void* ptr, size_t new_size);
    // A function pointer for freeing memory, with the same semantics as the standard free but with an additional context parameter
    void(*free)(void* ctx, void* ptr);
} Xml_Allocator;

typedef struct Xml_Error {
    // A human-readable message describing the error.
    // Owned by the document it's added to, the library will call free on it when the document is freed.
    char* message;
    // The line number where the error occurred, starting from 0.
    size_t line;
    // The column number where the error occurred, starting from 0.
    size_t column;
    // The index in the input string where the error occurred, starting from 0.
    size_t index;
} Xml_Error;

typedef struct Xml_Attribute {
    char* name;
    char* value;
} Xml_Attribute;

typedef struct Xml_Sax {
    void(*on_start_element)(void* user_data, char* name, Xml_Attribute* attributes, size_t attribute_count);
    void(*on_end_element)(void* user_data, char* name);
    void(*on_text)(void* user_data, char const* text, size_t length);
    void(*on_error)(void* user_data, Xml_Error error);
} Xml_Sax;

// TODO: Public API declarations

#ifdef __cplusplus
}
#endif

#endif /* XML_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef XML_IMPLEMENTATION

#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

static bool xml_is_text_char(char ch) {
    return ch != '<'
        // '&' can start an entity ref, which we handle separately
        && ch != '&'
        // ']' can end a CDATA section, which we handle separately
        && ch != ']';
}

// Allocation //////////////////////////////////////////////////////////////////

static void* xml_default_realloc(void* ctx, void* ptr, size_t new_size) {
    (void)ctx;
    return realloc(ptr, new_size);
}

static void xml_default_free(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}

static void xml_init_allocator(Xml_Allocator* allocator) {
    if (allocator->realloc != NULL || allocator->free != NULL) {
        XML_ASSERT(allocator->realloc != NULL && allocator->free != NULL, "both realloc and free function pointers must be set in allocator");
        return;
    }
    allocator->realloc = xml_default_realloc;
    allocator->free = xml_default_free;
}

static void* xml_realloc(Xml_Allocator* allocator, void* ptr, size_t size) {
    xml_init_allocator(allocator);
    void* result = allocator->realloc(allocator->context, ptr, size);
    XML_ASSERT(result != NULL, "failed to allocate memory");
    return result;
}

static void xml_free(Xml_Allocator* allocator, void* ptr) {
    xml_init_allocator(allocator);
    allocator->free(allocator->context, ptr);
}

// Parsing /////////////////////////////////////////////////////////////////////

typedef struct Xml_Position {
    size_t index;
    size_t line;
    size_t column;
} Xml_Position;

typedef struct Xml_Parser {
    char const* text;
    size_t length;
    Xml_Position position;
    Xml_Sax sax;
    void* user_data;
} Xml_Parser;

static void xml_parser_report_error(Xml_Parser* parser, Xml_Position position, char* message) {
    Xml_Allocator* allocator = &parser->options.allocator;
    if (parser->sax.on_error == NULL) {
        xml_free(allocator, message);
        return;
    }
    Xml_Error error = {
        .message = message,
        .line = position.line,
        .column = position.column,
        .index = position.index,
    };
    parser->sax.on_error(parser->user_data, error);
}

static char xml_parser_peek(Xml_Parser* parser, size_t offset, char def) {
    if (parser->position.index + offset >= parser->length) return def;
    return parser->text[parser->position.index + offset];
}

static void xml_parser_advance(Xml_Parser* parser, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        XML_ASSERT(parser->position.index < parser->length, "attempted to advance past end of input");
        char ch = parser->text[parser->position.index];
        if (ch == '\r') {
            // Could be an OS-X or Windows-style newline
            // If Windows-style, we'll go to the next line in the next iteration
            if (xml_parser_peek(parser, 1, '\0') == '\n') {
                // Windows-style newline, will step to newline in the next iteration
            }
            else {
                // OSX-style newline
                ++parser->position.line;
                parser->position.column = 0;
            }
        }
        else if (ch == '\n') {
            // Unix-style newline
            ++parser->position.line;
            parser->position.column = 0;
        }
        else {
            ++parser->position.column;
        }
        ++parser->position.index;
    }
}

static bool xml_parser_matches(Xml_Parser* parser, size_t offset, char const* str) {
    while (*str != '\0') {
        char ch = xml_parser_peek(parser, offset, '\0');
        if (ch != *str) return false;
        ++offset;
        ++str;
    }
    return true;
}

static void xml_parser_report_text(Xml_Parser* parser, char* text, size_t length) {
    if (length == 0) return;
    if (parser->sax.on_text == NULL) return;
    parser->sax.on_text(parser->user_data, text, length);
}

static void xml_parse_text(Xml_Parser* parser) {
    size_t offset = 0;
    while (true) {
        char ch = xml_parser_peek(parser, offset, '\0');
        if (!xml_is_text_char(ch)) break;
        ++offset;
    }
    xml_parser_report_text(parser, &parser->text[parser->position.index], offset);
    xml_parser_advance(parser, offset);
}

static void xml_parse_impl(Xml_Parser* parser) {
start:
    xml_parse_text(parser);
    char ch = xml_parser_peek(parser, 0, '\0');
    if (ch == '\0') {
        // TODO
    }
    else if (ch == '<') {
        char next = xml_parser_peek(parser, 1, '\0');
        if (next == '\0') {
            // TODO
        }
        else if (next == '/') {
            // TODO
        }
        else if (next == '?') {
            // TODO
        }
        else if (next == '!') {
            // TODO
        }
        else {
            // TODO
        }
    }
    else if (ch == '&') {
        char next = xml_parser_peek(parser, 1, '\0');
        if (next == '\0') {
            // TODO
        }
        else if (next == '#') {
            if (xml_parser_peek(parser, 2, '\0') == 'x') {
                // TODO
            }
            else {
                // TODO
            }
        }
        else {
            size_t offset = 1;
            if (xml_parser_matches(parser, offset, "amp;")) {
                offset += 4;
                xml_parser_report_text(parser, "&", 1);
                goto start;
            }
            else if (xml_parser_matches(parser, offset, "lt;")) {
                offset += 3;
                xml_parser_report_text(parser, "<", 1);
                goto start;
            }
            else if (xml_parser_matches(parser, offset, "gt;")) {
                offset += 3;
                xml_parser_report_text(parser, ">", 1);
                goto start;
            }
            else if (xml_parser_matches(parser, offset, "quot;")) {
                offset += 5;
                xml_parser_report_text(parser, "\"", 1);
                goto start;
            }
            else if (xml_parser_matches(parser, offset, "apos;")) {
                offset += 5;
                xml_parser_report_text(parser, "'", 1);
                goto start;
            }
            else {
                // TODO
            }
        }
    }
    else {
        // TODO
    }
}

#ifdef __cplusplus
}
#endif

#endif /* XML_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef XML_SELF_TEST

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

CTEST_CASE(sample_test) {
    CTEST_ASSERT_FAIL("TODO");
}

#endif /* XML_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef XML_EXAMPLE
#undef XML_EXAMPLE

#include <stdio.h>

#define XML_IMPLEMENTATION
#define XML_STATIC
#include "xml.h"

int main(void) {
    // TODO: Example usage goes here
    return 0;
}

#endif /* XML_EXAMPLE */
