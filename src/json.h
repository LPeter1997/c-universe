/**
 * json.h is a single-header JSON library for parsing, building and serializing JSON format.
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
#ifndef JSON_H
#define JSON_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef JSON_STATIC
    #define JSON_DEF static
#else
    #define JSON_DEF extern
#endif

#ifndef JSON_REALLOC
    #define JSON_REALLOC realloc
#endif
#ifndef JSON_FREE
    #define JSON_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Json_Extension {
    JSON_EXTENSION_NONE = 0,
    JSON_EXTENSION_COMMENTS = 1 << 0,
    JSON_EXTENSION_TRAILING_COMMAS = 1 << 1,
} Json_Extension;

typedef struct Json_Options {
    Json_Extension extensions;
    char const* newline_str;
    char const* indent_str;
} Json_Options;

typedef struct Json_Error {
    // Owned string
    char const* message;
    size_t line;
    size_t column;
    size_t index;
} Json_Error;

typedef struct Json_Sax {
    void(*on_null)(void* user_data);
    void(*on_bool)(void* user_data, bool value);
    void(*on_int)(void* user_data, long long value);
    void(*on_double)(void* user_data, double value);
    void(*on_string)(void* user_data, char const* value, size_t length);
    void(*on_array_start)(void* user_data);
    void(*on_array_end)(void* user_data);
    void(*on_object_start)(void* user_data);
    void(*on_object_key)(void* user_data, char const* key, size_t length);
    void(*on_object_end)(void* user_data);
    void(*on_error)(void* user_data, Json_Error error);
} Json_Sax;

typedef enum Json_ValueType {
    JSON_VALUE_NULL,
    JSON_VALUE_BOOL,
    JSON_VALUE_INT,
    JSON_VALUE_DOUBLE,
    JSON_VALUE_STRING,
    JSON_VALUE_ARRAY,
    JSON_VALUE_OBJECT,
} Json_ValueType;

struct Json_HashBucket;

typedef struct Json_Value {
    Json_ValueType type;
    union {
        bool bool_value;
        long long int_value;
        double double_value;
        // Owned string
        char const* string_value;
        struct {
            Json_Value* elements;
            size_t length;
            size_t capacity;
        } array_value;
        struct {
            struct Json_HashBucket* buckets;
            size_t length;
            size_t capacity;
            size_t entry_count;
        } object_value;
    };
} Json_Value;

typedef struct Json_Document {
    Json_Value root;
    struct {
        Json_Error* elements;
        size_t length;
        size_t capacity;
    } errors;
} Json_Document;

JSON_DEF void json_parse_sax(char const* json, Json_Sax sax, Json_Options options, void* user_data);
JSON_DEF Json_Document json_parse(char const* json, Json_Options options);
JSON_DEF size_t json_serialize_to(Json_Value value, Json_Options options, char* buffer, size_t buffer_size);
JSON_DEF char* json_serialize(Json_Value value, Json_Options options, size_t* out_length);

JSON_DEF Json_Value json_object(void);
JSON_DEF Json_Value json_array(void);
JSON_DEF Json_Value json_string(char const* str);
JSON_DEF Json_Value json_int(long long value);
JSON_DEF Json_Value json_double(double value);
JSON_DEF Json_Value json_bool(bool value);
JSON_DEF Json_Value json_null(void);

JSON_DEF void json_value_free(Json_Value* value);
JSON_DEF void json_document_free(Json_Document* doc);

JSON_DEF void json_object_set(Json_Value* object, char const* key, Json_Value value);
JSON_DEF Json_Value* json_object_get(Json_Value* object, char const* key);
JSON_DEF bool json_object_get_at(Json_Value* object, size_t index, char const** out_key, Json_Value* out_value);
JSON_DEF bool json_object_remove(Json_Value* object, char const* key);

JSON_DEF void json_array_append(Json_Value* array, Json_Value value);
JSON_DEF void json_array_set(Json_Value* array, size_t index, Json_Value value);
JSON_DEF Json_Value* json_array_get(Json_Value* array, size_t index);
JSON_DEF void json_array_remove(Json_Value* array, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* JSON_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef JSON_IMPLEMENTATION

#include <assert.h>
#include <ctype.h>

#define JSON_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

static bool json_isident(char c) {
    return isalpha(c) || isdigit(c) || c == '_';
}

static bool json_isxdigit(char ch, int* out_value) {
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

static char* json_strdup(char const* str) {
    size_t length = strlen(str);
    char* copy = (char*)JSON_REALLOC(NULL, (length + 1) * sizeof(char));
    JSON_ASSERT(copy != NULL, "failed to allocate memory for string duplication");
    memcpy(copy, str, length * sizeof(char));
    copy[length] = '\0';
    return copy;
}

static char* json_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format, args_copy);
    JSON_ASSERT(length >= 0, "failed to compute length of formatted string");
    va_end(args_copy);
    char* buffer = (char*)JSON_REALLOC(NULL, ((size_t)length + 1) * sizeof(char));
    JSON_ASSERT(buffer != NULL, "failed to allocate memory for formatted string");
    vsnprintf(buffer, (size_t)length + 1, format, args);
    va_end(args);
    return buffer;
}

static size_t json_utf8_encode(uint32_t cp, char out[4]) {
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

// Parsing /////////////////////////////////////////////////////////////////////

typedef struct Json_Parser {
    char const* text;
    size_t length;
    size_t index;
    size_t line;
    size_t column;
    Json_Sax sax;
    Json_Options options;
    void* user_data;
} Json_Parser;

static void json_parser_report_error(Json_Parser* parser, char const* message) {
    if (parser->sax.on_error == NULL) {
        JSON_FREE(message);
        return;
    }
    Json_Error error = {
        .message = message,
        .line = parser->line,
        .column = parser->column,
        .index = parser->index,
    };
    parser->sax.on_error(parser->user_data, error);
}

static char json_parser_peek(Json_Parser* parser, size_t offset, char def) {
    if (parser->index + offset >= parser->length) return def;
    return parser->text[parser->index + offset];
}

static void json_parser_skip_whitespace(Json_Parser* parser) {
    while (parser->index < parser->length) {
        char c = parser->text[parser->index];
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;

        if (c == '\r') {
            if (json_parser_peek(parser, 1, '\0') == '\n') {
                // Windows-style newline
                parser->index += 2;
            }
            else {
                // OSX-style newline
                ++parser->index;
            }
            ++parser->line;
            parser->column = 0;
        }
        else if (c == '\n') {
            // Unix-style newline
            ++parser->index;
            ++parser->line;
            parser->column = 0;
        }
        else {
            ++parser->index;
            ++parser->column;
        }
    }
}

static void json_parser_advance(Json_Parser* parser, size_t count) {
    parser->index += count;
    parser->column += count;
}

static bool json_parser_expect_char(Json_Parser* parser, char expected) {
    char c = json_parser_peek(parser, 0, '\0');
    if (c == expected) {
        json_parser_advance(parser, 1);
        return true;
    }
    else {
        char* message = json_format("expected character '%c', but got '%c'", expected, c);
        json_parser_report_error(parser, message);
        return false;
    }
}

// TODO: RETHINK THIS METHOD, IT'S AN ABSOLUTE CATASTROPHE
// NOTE: Does not actually advance the parser, when buffer is NULL
// This is so the user of this call can first compute the required buffer size, then allocate, then call again to fill the buffer
static size_t json_parse_string_value_impl(JsonParser* parser, char* buffer, size_t bufferSize) {
    size_t parserOffset = 0;
    // Skip opening quote
    if (json_parser_peek(parser, parserOffset, '\0') != '"') {
        char* message = json_format("expected '\"' at the start of string value, but got '%c'", json_parser_peek(parser, parserOffset, '\0'));
        json_parser_report_error(parser, message);
        return 0;
    }

    // Only advance, if buffer is not NULL
    if (buffer != NULL) {
        json_parser_advance(parser, 1);
    }
    else {
        ++parserOffset;
    }

    size_t bufferIndex = 0;
    while (parser->index + parserOffset < parser->length) {
        char ch = parser->text[parser->index + parserOffset];
        if (ch == '"') break;
        if (ch != '\\') {
            // Normal character
            if (buffer != NULL) {
                JSON_ASSERT(bufferIndex < bufferSize, "buffer overflow while parsing string value");
                json_parser_advance(parser, 1);
                buffer[bufferIndex] = ch;
            }
            ++parserOffset;
            ++bufferIndex;
            continue;
        }
        // Escape sequence
        json_parser_advance(parser, 1);
        char escaped = json_parser_peek(parser, 0, '\0');
        char unescaped = '\0';
        switch (escaped) {
        case '"': unescaped = '"'; break;
        case '\\': unescaped = '\\'; break;
        case '/': unescaped = '/'; break;
        case 'b': unescaped = '\b'; break;
        case 'f': unescaped = '\f'; break;
        case 'n': unescaped = '\n'; break;
        case 'r': unescaped = '\r'; break;
        case 't': unescaped = '\t'; break;
        }
        if (unescaped != '\0') {
            // Simple escape sequence
            if (buffer != NULL) {
                JSON_ASSERT(bufferIndex < bufferSize, "buffer overflow while parsing string value");
                buffer[bufferIndex] = unescaped;
            }
            ++bufferIndex;
            continue;
        }
        // Unicode codepoint escape sequence
        if (escaped == 'u') {
            // TODO
            continue;
        }
        // Invalid escape sequence
        char* message = json_format("invalid escape sequence '\\%c' in string value", escaped);
        json_parser_report_error(parser, message);
        // We add the unescaped substring to best approximate the error
        if (buffer != NULL) {
            JSON_ASSERT(bufferIndex + 1 < bufferSize, "buffer overflow while parsing string value");
            buffer[bufferIndex] = '\\';
            buffer[bufferIndex + 1] = escaped;
        }
        bufferIndex += 2;
    }

    return bufferIndex;
}

static Json_Value json_parse_string_value(Json_Parser* parser) {
    size_t bufferSize = json_parse_string_value_impl(parser, NULL, 0);
    if (bufferSize == 0) return json_string("");
    char* buffer = (char*)JSON_REALLOC(NULL, (bufferSize + 1) * sizeof(char));
    JSON_ASSERT(buffer != NULL, "failed to allocate memory for parsed string value");
    json_parse_string_value_impl(parser, buffer, bufferSize);
    buffer[bufferSize] = '\0';
    // NOTE: we don't use json_string here as we already have an owned string
    return (Json_Value){
        .type = JSON_VALUE_STRING,
        .string_value = buffer,
    };
}

static Json_Value json_parse_value(Json_Parser* parser) {
    json_parser_skip_whitespace(parser);
    char c = json_parser_peek(parser, 0, '\0');
    if (c == '{') {
        // TODO: Parse object
    }
    else if (c == '[') {
        // TODO: Parse array
    }
    else if (c == '"') {
        return json_parse_string_value(parser);
    }
    else if (isdigit(c) || c == '-') {
        // TODO: Parse number (int or double)
    }
    else if (json_isident(c)) {
        // TODO: Parse an identifier, match to true/false/null
    }
    else {
        char* message = json_format("unexpected character '%c' while parsing value", c);
        json_parser_report_error(parser, message);
        return json_null();
    }
}

void json_parse_sax(char const* json, Json_Sax sax, Json_Options options, void* user_data) {

}

// Value constructors //////////////////////////////////////////////////////////

Json_Value json_object(void) {
    return (Json_Value){
        .type = JSON_VALUE_OBJECT,
        .object_value = { 0 },
    };
}

Json_Value json_array(void) {
    return (Json_Value){
        .type = JSON_VALUE_ARRAY,
        .array_value = { 0 },
    };
}

Json_Value json_string(char const* str) {
    return (Json_Value){
        .type = JSON_VALUE_STRING,
        .string_value = json_strdup(str),
    };
}

Json_Value json_int(long long value) {
    return (Json_Value){
        .type = JSON_VALUE_INT,
        .int_value = value,
    };
}
Json_Value json_double(double value) {
    return (Json_Value){
        .type = JSON_VALUE_DOUBLE,
        .double_value = value,
    };
}

Json_Value json_bool(bool value) {
    return (Json_Value){
        .type = JSON_VALUE_BOOL,
        .bool_value = value,
    };
}

Json_Value json_null(void) {
    return (Json_Value){
        .type = JSON_VALUE_NULL,
    };
}

#ifdef __cplusplus
}
#endif

#undef JSON_ASSERT

#endif /* JSON_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef JSON_SELF_TEST

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

CTEST_CASE(sample_test) {
    CTEST_ASSERT_FAIL("TODO");
}

#endif /* JSON_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef JSON_EXAMPLE
#undef JSON_EXAMPLE

#include <stdio.h>

#define JSON_IMPLEMENTATION
#define JSON_STATIC
#include "json.h"

int main(void) {
    // TODO: Example usage goes here
    return 0;
}

#endif /* JSON_EXAMPLE */
