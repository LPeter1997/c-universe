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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

typedef struct Xml_Options {
    Xml_Allocator allocator;
    bool namespace_aware;
} Xml_Options;

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

typedef struct Xml_QualifiedName {
    char* ns;
    char* name;
} Xml_QualifiedName;

typedef struct Xml_Attribute {
    Xml_QualifiedName name;
    char* value;
} Xml_Attribute;

typedef struct Xml_Sax {
    void(*on_start_element)(void* user_data, Xml_QualifiedName name, Xml_Attribute* attributes, size_t attribute_count);
    void(*on_end_element)(void* user_data, Xml_QualifiedName name);
    void(*on_text)(void* user_data, char* text, size_t length);
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
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#define XML_ADD_TO_ARRAY(allocator, array, element) \
    do { \
        if ((array).length + 1 > (array).capacity) { \
            size_t newCapacity = ((array).capacity == 0) ? 8 : ((array).capacity * 2); \
            void* newElements = xml_realloc((allocator), (array).elements, newCapacity * sizeof(*(array).elements)); \
            (array).elements = newElements; \
            (array).capacity = newCapacity; \
        } \
        (array).elements[(array).length++] = element; \
    } while (false)

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

static bool xml_is_tag_char(char ch, bool isFirst) {
    return isalpha((unsigned char)ch) || ch == '_'
        || (!isFirst && (isdigit((unsigned char)ch) || ch == '-' || ch == '.' || ch == ':'));
}

static bool xml_isdigit(char ch, int* out_value) {
    if (ch >= '0' && ch <= '9') {
        *out_value = ch - '0';
        return true;
    }
    return false;
}

static bool xml_isxdigit(char ch, int* out_value) {
    if (ch >= '0' && ch <= '9') {
        *out_value = ch - '0';
        return true;
    }
    else if (ch >= 'a' && ch <= 'f') {
        *out_value = 10 + (ch - 'a');
        return true;
    }
    else if (ch >= 'A' && ch <= 'F') {
        *out_value = 10 + (ch - 'A');
        return true;
    }
    return false;
}

static size_t xml_utf8_encode(uint32_t cp, char out[4]) {
    if (cp <= 0x7F) {
        out[0] = (char)cp;
        return 1;
    }
    else if (cp <= 0x7FF) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    else if (cp <= 0xFFFF) {
        // exclude UTF-16 surrogate range
        if (cp >= 0xD800 && cp <= 0xDFFF) return 0;

        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    else if (cp <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        return 4;
    }

    // invalid Unicode code point
    return 0;
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

// Helpers /////////////////////////////////////////////////////////////////////

static char* xml_strndup(Xml_Allocator* allocator, char const* str, size_t length) {
    char* copy = (char*)xml_realloc(allocator, NULL, (length + 1) * sizeof(char));
    memcpy(copy, str, length);
    copy[length] = '\0';
    return copy;
}

static char* xml_strdup(Xml_Allocator* allocator, char const* str) {
    return xml_strndup(allocator, str, strlen(str));
}

static char* xml_format(Xml_Allocator* allocator, const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format, args_copy);
    XML_ASSERT(length >= 0, "failed to compute length of formatted string");
    va_end(args_copy);
    char* buffer = (char*)xml_realloc(allocator, NULL, ((size_t)length + 1) * sizeof(char));
    vsnprintf(buffer, (size_t)length + 1, format, args);
    va_end(args);
    return buffer;
}

// String builder //////////////////////////////////////////////////////////////

typedef struct Xml_StringBuilder {
    Xml_Allocator allocator;
    char* data;
    size_t length;
    size_t capacity;
} Xml_StringBuilder;

static void xml_string_builder_reserve(Xml_StringBuilder* builder, size_t new_capacity) {
    if (new_capacity <= builder->capacity) return;
    size_t new_cap = builder->capacity == 0 ? 16 : builder->capacity * 2;
    while (new_cap < new_capacity) new_cap *= 2;
    char* new_data = xml_realloc(&builder->allocator, builder->data, new_cap);
    builder->data = new_data;
    builder->capacity = new_cap;
}

static void xml_string_builder_appendc(Xml_StringBuilder* builder, char ch) {
    xml_string_builder_reserve(builder, builder->length + 1);
    builder->data[builder->length++] = ch;
}

static void xml_string_builder_append(Xml_StringBuilder* builder, char const* str, size_t length) {
    xml_string_builder_reserve(builder, builder->length + length);
    memcpy(builder->data + builder->length, str, length);
    builder->length += length;
}

static char* xml_string_builder_take(Xml_StringBuilder* builder) {
    xml_string_builder_appendc(builder, '\0');
    char* result = builder->data;
    builder->data = NULL;
    builder->length = 0;
    builder->capacity = 0;
    return result;
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
    Xml_Options options;
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
    if (parser->sax.on_text == NULL) {
        xml_free(&parser->options.allocator, text);
        return;
    }
    parser->sax.on_text(parser->user_data, text, length);
}

static void xml_parse_entity_ref(Xml_Parser* parser, Xml_StringBuilder* builder) {
    Xml_Allocator* allocator = &parser->options.allocator;

    char ch = xml_parser_peek(parser, 0, '\0');
    if (ch != '&') return;

    size_t offset = 1;
    char next = xml_parser_peek(parser, offset, '\0');
    if (next == '\0') {
        // TODO
    }
    else if (next == '#') {
        ++offset;
        bool isHex = false;
        if (xml_parser_peek(parser, offset, '\0') == 'x') {
            ++offset;
            isHex = true;
        }
        uint32_t codepoint = 0;
        size_t codepointLength = 0;
        int digitValue;
        while (true) {
            char c = xml_parser_peek(parser, offset, '\0');
            if (isHex) {
                if (!xml_isxdigit(c, &digitValue)) break;
            }
            else {
                if (!xml_isdigit(c, &digitValue)) break;
            }
            codepoint = codepoint * (isHex ? 16 : 10) + (uint32_t)digitValue;
            ++offset;
            ++codepointLength;
        }
        if (xml_parser_peek(parser, offset, '\0') == ';') {
            ++offset;
            if (codepointLength == 0) {
                char* message = xml_format(allocator, "invalid character reference, expected at least one digit");
                xml_parser_report_error(parser, parser->position, message);
            }
            else {
                char utf8[4];
                size_t utf8Length = xml_utf8_encode(codepoint, utf8);
                xml_string_builder_append(builder, utf8, utf8Length);
            }
        }
        else {
            char* message = xml_format(allocator, "invalid character reference, expected ';' at the end");
            xml_parser_report_error(parser, parser->position, message);
        }
    }
    else {
        if (xml_parser_matches(parser, offset, "amp;")) {
            offset += 4;
            xml_string_builder_append(builder, "&", 1);
        }
        else if (xml_parser_matches(parser, offset, "lt;")) {
            offset += 3;
            xml_string_builder_append(builder, "<", 1);
        }
        else if (xml_parser_matches(parser, offset, "gt;")) {
            offset += 3;
            xml_string_builder_append(builder, ">", 1);
        }
        else if (xml_parser_matches(parser, offset, "quot;")) {
            offset += 5;
            xml_string_builder_append(builder, "\"", 1);
        }
        else if (xml_parser_matches(parser, offset, "apos;")) {
            offset += 5;
            xml_string_builder_append(builder, "'", 1);
        }
        else {
            char* message = xml_format(allocator, "invalid character '%c' after '&', expected a valid entity reference", next);
            xml_parser_report_error(parser, parser->position, message);
        }
    }
    xml_parser_advance(parser, offset);
}

static bool xml_parse_text(Xml_Parser* parser) {
    Xml_StringBuilder builder = {.allocator = parser->options.allocator};
    while (true) {
        char ch = xml_parser_peek(parser, 0, '\0');
        if (ch == '&') {
            xml_parse_entity_ref(parser, &builder);
        }
        else if (xml_is_text_char(ch)) {
            xml_string_builder_appendc(&builder, ch);
            xml_parser_advance(parser, 1);
        }
        else {
            break;
        }
    }
    if (builder.length > 0) {
        // NOTE: We copy out length, as _take resets the builder
        size_t textLength = builder.length;
        char* text = xml_string_builder_take(&builder);
        xml_parser_report_text(parser, text, textLength);
        return true;
    }
    // NOTE: On 0 length, we haven't allocated anything, no need to free
    return false;
}

static size_t xml_parse_tag_name(Xml_Parser* parser, size_t offset) {
    Xml_Allocator* allocator = &parser->options.allocator;
    size_t length = 0;
    while (true) {
        char c = xml_parser_peek(parser, offset + length, '\0');
        if (c == '\0') {
            char* message = xml_format(allocator, "unexpected end of input in tag name");
            xml_parser_report_error(parser, parser->position, message);
            xml_parser_advance(parser, offset + length);
            return 0;
        }
        if (!xml_is_tag_char(c, length == 0)) break;
        ++length;
    }
    if (length == 0) {
        char* message = xml_format(allocator, "tag name cannot be empty");
        xml_parser_report_error(parser, parser->position, message);
        xml_parser_advance(parser, offset);
        return 0;
    }
    return length;
}

static bool xml_parse_element(Xml_Parser* parser) {
    Xml_Allocator* allocator = &parser->options.allocator;

    char ch = xml_parser_peek(parser, 0, '\0');
    if (ch != '<') return false;

    size_t offset = 1;
    char next = xml_parser_peek(parser, offset, '\0');
    if (next == '\0') {
        char* message = xml_format(allocator, "unexpected end of input after '<'");
        xml_parser_report_error(parser, parser->position, message);
        xml_parser_advance(parser, 1);
        return false;
    }
    else if (next == '/') {
        // End tag
        ++offset;
        size_t tagNameLength = xml_parse_tag_name(parser, offset);
        if (tagNameLength == 0) return false;
        offset += tagNameLength;
        if (xml_parser_peek(parser, offset, '\0') != '>') {
            char* message = xml_format(allocator, "expected '>' at the end of end tag");
            xml_parser_report_error(parser, parser->position, message);
            xml_parser_advance(parser, offset);
            return false;
        }
        ++offset;
        if (parser->sax.on_end_element != NULL) {
            char* tagName = xml_strndup(&parser->options.allocator, &parser->text[parser->position.index + offset - tagNameLength - 1], tagNameLength);
            parser->sax.on_end_element(parser->user_data, tagName);
        }
        xml_parser_advance(parser, offset);
        return true;
    }
    else if (next == '?') {
        // TODO: processing instruction
    }
    else if (next == '!') {
        // TODO: comment, CDATA section, DOCTYPE, etc.
    }
    else if (xml_is_tag_char(next, true)) {
        size_t tagNameLength = xml_parse_tag_name(parser, offset);
        if (tagNameLength == 0) return false;
        offset += tagNameLength;
        char* tagName = xml_strndup(&parser->options.allocator, &parser->text[parser->position.index + offset - tagNameLength], tagNameLength);
        xml_parser_advance(parser, offset);
        // We can have arbitrary attributes, then either a '>' or a '/>' at the end
        struct {
            Xml_Attribute* elements;
            size_t length;
            size_t capacity;
        } attributes = {0};
        bool returnValue = true;
        while (true) {
            char ch = xml_parser_peek(parser, 0, '\0');
            if (ch == '\0') {
                char* message = xml_format(allocator, "unexpected end of input in start tag");
                xml_parser_report_error(parser, parser->position, message);
                xml_parser_advance(parser, 1);
                returnValue = false;
                goto cleanup_start_tag;
            }
            // Skip space
            if (isspace((unsigned char)ch)) {
                xml_parser_advance(parser, 1);
                continue;
            }
            // End of start tag
            if (ch == '>') {
                xml_parser_advance(parser, 1);
                if (parser->sax.on_start_element == NULL) {
                    goto cleanup_start_tag;
                }
                else {
                    parser->sax.on_start_element(parser->user_data, tagName, attributes.elements, attributes.length);
                }
                return true;
            }
            // Attribute
            if (xml_is_tag_char(ch, true)) {
                // TODO
                continue;
            }
            // Self-closing tag
            if (ch == '/' && xml_parser_peek(parser, 1, '\0') == '>') {
                xml_parser_advance(parser, 2);
                if (parser->sax.on_start_element == NULL) {
                    goto cleanup_start_tag;
                }
                else {
                    parser->sax.on_start_element(parser->user_data, tagName, attributes.elements, attributes.length);
                    if (parser->sax.on_end_element != NULL) {
                        // To avoid double-freeing, copy tag name
                        char* tagNameCopy = xml_strdup(allocator, tagName);
                        parser->sax.on_end_element(parser->user_data, tagNameCopy);
                    }
                }
                return true;
            }
            // Invalid character
            char* message = xml_format(allocator, "invalid character '%c' in start tag, expected space, '>', '/', or attribute name", ch);
            xml_parser_report_error(parser, parser->position, message);
            xml_parser_advance(parser, 1);
            // We keep going
        }
    cleanup_start_tag:
        xml_free(allocator, tagName);
        // Free attributes
        for (size_t i = 0; i < attributes.length; ++i) {
            xml_free(allocator, attributes.elements[i].name);
            xml_free(allocator, attributes.elements[i].value);
        }
        xml_free(allocator, attributes.elements);
        return returnValue;
    }
    else {
        ++offset;
        char* message = xml_format(allocator, "invalid character '%c' after '<', expected a valid tag name", next);
        xml_parser_report_error(parser, parser->position, message);
        xml_parser_advance(parser, offset);
        return false;
    }
}

static void xml_parse_impl(Xml_Parser* parser) {
    if (xml_parse_text(parser)) return;
    if (xml_parse_element(parser)) return;

    // Unexpected
    Xml_Allocator* allocator = &parser->options.allocator;
    char* message = xml_format(allocator, "unexpected character '%c'", xml_parser_peek(parser, 0, '\0'));
    xml_parser_report_error(parser, parser->position, message);
    xml_parser_advance(parser, 1);
}

#ifdef __cplusplus
}
#endif

#undef XML_ADD_TO_ARRAY

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
