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
            size_t buckets_length;
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
JSON_DEF bool json_object_remove(Json_Value* object, char const* key, Json_Value* out_value);

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
#define JSON_ADD_TO_ARRAY(array, length, capacity, element) \
    do { \
        if ((length) + 1 > (capacity)) { \
            size_t newCapacity = ((capacity) == 0) ? 8 : ((capacity) * 2); \
            void* newElements = JSON_REALLOC((array), newCapacity * sizeof(*(array))); \
            JSON_ASSERT(newElements != NULL, "failed to allocate memory for array"); \
            (array) = newElements; \
            (capacity) = newCapacity; \
        } \
        (array)[(length)++] = element; \
    } while (false)

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

typedef struct Json_Position {
    size_t index;
    size_t line;
    size_t column;
} Json_Position;

typedef struct Json_Parser {
    char const* text;
    size_t length;
    Json_Position position;
    Json_Sax sax;
    Json_Options options;
    void* user_data;
} Json_Parser;

static void json_parser_report_error(Json_Parser* parser, Json_Position position, char const* message) {
    if (parser->sax.on_error == NULL) {
        JSON_FREE(message);
        return;
    }
    Json_Error error = {
        .message = message,
        .line = position.line,
        .column = position.column,
        .index = position.index,
    };
    parser->sax.on_error(parser->user_data, error);
}

static char json_parser_peek(Json_Parser* parser, size_t offset, char def) {
    if (parser->position.index + offset >= parser->length) return def;
    return parser->text[parser->position.index + offset];
}

static void json_parser_advance(Json_Parser* parser, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        JSON_ASSERT(parser->position.index < parser->length, "attempted to advance past end of input");
        char ch = parser->text[parser->position.index];
        if (ch == '\r') {
            // Could be an OS-X or Windows-style newline
            // If Windows-style, we'll go to the next line in the next iteration
            if (json_parser_peek(parser, 1, '\0') == '\n') {
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

static void json_parser_skip_whitespace(Json_Parser* parser) {
start:
    while (isspace(json_parser_peek(parser, 0, '\0'))) json_parser_advance(parser, 1);

    // We check for line comments here
    if (json_parser_peek(parser, 0, '\0') == '/' && json_parser_peek(parser, 1, '\0') == '/') {
        // If extension is not enabled, report an error
        if ((parser->options.extensions & JSON_EXTENSION_COMMENTS) == 0) {
            char* message = json_format("line comments are not allowed");
            json_parser_report_error(parser, parser->position, message);
            // We still continue the parser, treating the comment as whitespace
        }
        // Skip the comment
        json_parser_advance(parser, 2);
        while (true) {
            char c = json_parser_peek(parser, 0, '\n');
            if (c == '\n' || c == '\r') break;
            json_parser_advance(parser, 1);
        }
        // Whitespaces and comment again
        goto start;
    }
}

static bool json_parser_expect_char(Json_Parser* parser, char expected) {
    char c = json_parser_peek(parser, 0, '\0');
    if (c == expected) {
        json_parser_advance(parser, 1);
        return true;
    }
    else {
        char* message = json_format("expected character '%c', but got '%c'", expected, c);
        json_parser_report_error(parser, parser->position, message);
        return false;
    }
}

// NOTE: Does not actually advance the parser, this function is designed to be called twice:
//  - first with buffer = NULL to compute the required buffer size
//  - then with an allocated buffer to fill it
static size_t json_parse_string_value_impl(Json_Parser* parser, char* buffer, size_t bufferSize, size_t* outParserAdvance) {
    size_t parserOffset = 0;
    size_t bufferIndex = 0;
    if (json_parser_peek(parser, parserOffset, '\0') != '"') {
        char* message = json_format("expected '\"' at start of string value, but got '%c'", json_parser_peek(parser, parserOffset, '\0'));
        json_parser_report_error(parser, parser->position, message);
        goto done;
    }
    // skip opening quote
    ++parserOffset;
    while (true) {
        char c = json_parser_peek(parser, parserOffset, '\0');
        if (c == '\0') {
            char* message = json_format("unexpected end of input while parsing string value");
            json_parser_report_error(parser, parser->position, message);
            goto done;
        }
        if (c == '"') {
            // skip closing quote
            ++parserOffset;
            goto done;
        }
        if (c != '\\') {
            // Regular character, just copy
            if (buffer != NULL) {
                JSON_ASSERT(bufferIndex < bufferSize, "buffer overflow while parsing string value");
                buffer[bufferIndex] = c;
            }
            ++bufferIndex;
            ++parserOffset;
            continue;
        }
        // Escape sequence, skip backslash
        ++parserOffset;
        char escape = json_parser_peek(parser, parserOffset, '\0');
        if (escape == '\0') {
            char* message = json_format("unexpected end of input in escape sequence of string value");
            json_parser_report_error(parser, parser->position, message);
            goto done;
        }
        if (escape != 'u') {
            // Simple escape sequence, just translate and copy
            char escapedChar;
            switch (escape) {
                case '"': escapedChar = '"'; break;
                case '\\': escapedChar = '\\'; break;
                case '/': escapedChar = '/'; break;
                case 'b': escapedChar = '\b'; break;
                case 'f': escapedChar = '\f'; break;
                case 'n': escapedChar = '\n'; break;
                case 'r': escapedChar = '\r'; break;
                case 't': escapedChar = '\t'; break;
                default: {
                    char* message = json_format("invalid escape sequence '\\%c' in string value", escape);
                    json_parser_report_error(parser, parser->position, message);
                    // We don't actually return here, for best-effort we just treat it as a literal
                    escapedChar = escape;
                    break;
                }
            }
            if (buffer != NULL) {
                JSON_ASSERT(bufferIndex < bufferSize, "buffer overflow while parsing string value");
                buffer[bufferIndex] = escapedChar;
            }
            ++bufferIndex;
            ++parserOffset;
            continue;
        }
        // Unicode escape, we expect 4 hex digits after \u
        // First, skip 'u'
        ++parserOffset;
        uint32_t codepoint = 0;
        for (size_t i = 0; i < 4; ++i) {
            char hex = json_parser_peek(parser, parserOffset, '\0');
            if (hex == '\0') {
                char* message = json_format("unexpected end of input in unicode escape sequence of string value");
                json_parser_report_error(parser, parser->position, message);
                goto done;
            }
            int hexValue;
            if (!json_isxdigit(hex, &hexValue)) {
                char* message = json_format("invalid hex digit '%c' in unicode escape sequence of string value", hex);
                json_parser_report_error(parser, parser->position, message);
                // We don't actually return here, for best-effort we just treat it as a literal
                hexValue = 0;
            }
            codepoint = (codepoint << 4) | (uint32_t)hexValue;
            ++parserOffset;
        }
        // Encode the codepoint as UTF-8 and copy
        char utf8[4];
        size_t utf8Length = json_utf8_encode(codepoint, utf8);
        if (utf8Length == 0) {
            char* message = json_format("invalid Unicode code point U+%04X in string value", codepoint);
            json_parser_report_error(parser, parser->position, message);
            // We don't actually return here, for best-effort we just skip it
            continue;
        }
        if (buffer != NULL) {
            JSON_ASSERT(bufferIndex + utf8Length <= bufferSize, "buffer overflow while parsing string value");
            memcpy(buffer + bufferIndex, utf8, utf8Length * sizeof(char));
        }
        bufferIndex += utf8Length;
    }

done:
    *outParserAdvance = parserOffset;
    return bufferIndex;
}

// NOTE: All the parsers below are SAX-style parsers
// Any allocated value must be freed by the consumer of the values in case the corresponding callback is not null
// On a null SAX callback, the parser will free the value, if it allocated any

static void json_parse_string_value(Json_Parser* parser) {
    Json_Sax* sax = &parser->sax;
    size_t parserToAdvance;
    size_t length = json_parse_string_value_impl(parser, NULL, 0, &parserToAdvance);
    if (sax->on_string != NULL) {
        char* buffer = (char*)JSON_REALLOC(NULL, (length + 1) * sizeof(char));
        JSON_ASSERT(buffer != NULL, "failed to allocate buffer for string value");
        json_parse_string_value_impl(parser, buffer, length, &parserToAdvance);
        buffer[length] = '\0';
        sax->on_string(parser->user_data, buffer, length);
    }
    json_parser_advance(parser, parserToAdvance);
}

static void json_parse_identifier_value(Json_Parser* parser) {
    Json_Sax* sax = &parser->sax;
    size_t parserOffset = 0;
    while (json_isident(json_parser_peek(parser, parserOffset, '\0'))) ++parserOffset;

    // Check if we match true/false/null
    char const* ident = parser->text + parser->position.index;
    if (parserOffset == 4 && strncmp(ident, "true", 4) == 0) {
        json_parser_advance(parser, 4);
        if (sax->on_bool != NULL) sax->on_bool(parser->user_data, true);
    }
    else if (parserOffset == 5 && strncmp(ident, "false", 5) == 0) {
        json_parser_advance(parser, 5);
        if (sax->on_bool != NULL) sax->on_bool(parser->user_data, false);
    }
    else if (parserOffset == 4 && strncmp(ident, "null", 4) == 0) {
        json_parser_advance(parser, 4);
        if (sax->on_null != NULL) sax->on_null(parser->user_data);
    }
    else {
        char* message = json_format("unexpected identifier '%.*s'", (int)parserOffset, ident);
        json_parser_report_error(parser, parser->position, message);
        // We don't actually return here, for best-effort we just skip it and return null
        json_parser_advance(parser, parserOffset);
        // We still need to report a value to the consumer, so we report null
        if (sax->on_null != NULL) sax->on_null(parser->user_data);
    }
}

static void json_parse_number_value(Json_Parser* parser) {
    Json_Sax* sax = &parser->sax;
    long long intValue = 0;
    bool negate = false;
    size_t parserOffset = 0;
    // Minus sign has to stick to the number, no whitespace allowed in between
    if (json_parser_peek(parser, parserOffset, '\0') == '-') {
        negate = true;
        ++parserOffset;
    }
    size_t digitStart = parserOffset;
    while (true) {
        char c = json_parser_peek(parser, parserOffset, '\0');
        if (!isdigit(c)) break;
        intValue = intValue * 10 + (c - '0');
        ++parserOffset;
    }
    // Check that we have at least one digit
    if (parserOffset == digitStart) {
        char* message = json_format("expected digit in number value, but got '%c'", json_parser_peek(parser, parserOffset, '\0'));
        json_parser_report_error(parser, parser->position, message);
        json_parser_advance(parser, parserOffset);
        if (sax->on_null != NULL) sax->on_null(parser->user_data);
        return;
    }
    if (negate) intValue = -intValue;
    // From here on we have a fraction and exponent part, both optional
    // Check, if either is coming up, if not, we can return an int value
    char next = json_parser_peek(parser, parserOffset, '\0');
    if (next != '.' && next != 'e' && next != 'E') {
        json_parser_advance(parser, parserOffset);
        if (sax->on_int != NULL) sax->on_int(parser->user_data, intValue);
        return;
    }
    // We have a fraction or exponent part, we need to parse as double
    double doubleValue = (double)intValue;
    if (next == '.') {
        // Fractional part is present, skip dot
        ++parserOffset;
        double fractionMultiplier = 0.1;
        while (true) {
            char c = json_parser_peek(parser, parserOffset, '\0');
            if (!isdigit(c)) break;
            doubleValue += (c - '0') * fractionMultiplier;
            fractionMultiplier *= 0.1;
            ++parserOffset;
        }
    }
    next = json_parser_peek(parser, parserOffset, '\0');
    if (next == 'e' || next == 'E') {
        // Exponent part is present, skip 'e' or 'E'
        ++parserOffset;
        bool expNegate = false;
        if (json_parser_peek(parser, parserOffset, '\0') == '-') {
            expNegate = true;
            ++parserOffset;
        }
        else if (json_parser_peek(parser, parserOffset, '\0') == '+') {
            ++parserOffset;
        }
        int exponent = 0;
        while (true) {
            char c = json_parser_peek(parser, parserOffset, '\0');
            if (!isdigit(c)) break;
            exponent = exponent * 10 + (c - '0');
            ++parserOffset;
        }
        if (expNegate) exponent = -exponent;
        doubleValue *= pow(10, exponent);
    }
    json_parser_advance(parser, parserOffset);
    if (sax->on_double != NULL) sax->on_double(parser->user_data, doubleValue);
}

static void json_parse_value(Json_Parser* parser);

static void json_parse_array_value(Json_Parser* parser) {
    Json_Sax* sax = &parser->sax;
    if (!json_parser_expect_char(parser, '[')) return;

    if (sax->on_array_start != NULL) sax->on_array_start(parser->user_data);

    // Track these to report trailing comma errors
    Json_Position lastCommaPosition = { 0 };
    bool lastIsComma = false;

    while (true) {
        json_parser_skip_whitespace(parser);
        char c = json_parser_peek(parser, 0, '\0');
        // End of file
        if (c == '\0') {
            char* message = json_format("unexpected end of input while parsing array");
            json_parser_report_error(parser, parser->position, message);
            // Break out of the loop to report the array end regardless
            break;
        }
        // End of array, consumed after the loop
        if (c == ']') break;
        // Must be a value
        json_parse_value(parser);
        lastIsComma = false;
        // After a value, we can have either a comma (more values coming) or a closing bracket (end of array)
        json_parser_skip_whitespace(parser);
        c = json_parser_peek(parser, 0, '\0');
        if (c == ',') {
            lastCommaPosition = parser->position;
            lastIsComma = true;
            json_parser_advance(parser, 1);
            continue;
        }
        // Was not a comma, must be end of array
        break;
    }

    // If trailing comma extension is not enabled, report an error
    if (lastIsComma && (parser->options.extensions & JSON_EXTENSION_TRAILING_COMMAS) == 0) {
        char* message = json_format("trailing comma in array is not allowed");
        json_parser_report_error(parser, lastCommaPosition, message);
    }

    // Done
    json_parser_expect_char(parser, ']');
    if (sax->on_array_end != NULL) sax->on_array_end(parser->user_data);
}

static void json_parse_object_value(Json_Parser* parser) {
    Json_Sax* sax = &parser->sax;
    if (!json_parser_expect_char(parser, '{')) return;

    if (sax->on_object_start != NULL) sax->on_object_start(parser->user_data);

    // Track these to report trailing comma errors
    Json_Position lastCommaPosition = { 0 };
    bool lastIsComma = false;

    while (true) {
        json_parser_skip_whitespace(parser);
        char c = json_parser_peek(parser, 0, '\0');
        // End of file
        if (c == '\0') {
            char* message = json_format("unexpected end of input while parsing object");
            json_parser_report_error(parser, parser->position, message);
            // Break out of the loop to report the object end regardless
            break;
        }
        // End of object, consumed after the loop
        if (c == '}') break;
        // Must be a string key
        if (c != '"') {
            char* message = json_format("expected '\"' at start of object key, but got '%c'", c);
            json_parser_report_error(parser, parser->position, message);
            // We don't actually return here, for best-effort we just skip it and continue parsing
            json_parser_advance(parser, 1);
            continue;
        }
        // Parse the string key
        // This is one of the places we actually need to allocate memory, since we need to report the processed string key to the consumer
        size_t parserToAdvance;
        size_t keyLength = json_parse_string_value_impl(parser, NULL, 0, &parserToAdvance);
        if (sax->on_object_key != NULL) {
            // We only actually allocate, if the consumer has a callback for the key, otherwise we can just skip it
            char* keyBuffer = (char*)JSON_REALLOC(NULL, (keyLength + 1) * sizeof(char));
            JSON_ASSERT(keyBuffer != NULL, "failed to allocate buffer for object key");
            json_parse_string_value_impl(parser, keyBuffer, keyLength, &parserToAdvance);
            keyBuffer[keyLength] = '\0';
            sax->on_object_key(parser->user_data, keyBuffer, keyLength);
        }
        json_parser_advance(parser, parserToAdvance);
        // After the key, we need a colon
        json_parser_skip_whitespace(parser);
        // Welp, let's skip to next iteration if there's no colon
        if (!json_parser_expect_char(parser, ':')) {
            // Still report a null value to keep callbacks balanced
            if (sax->on_null != NULL) sax->on_null(parser->user_data);
            continue;
        }
        // After the colon, we need a value
        json_parser_skip_whitespace(parser);
        json_parse_value(parser);
        lastIsComma = false;
        // After a key-value pair, we can have either a comma (more pairs coming) or a closing brace (end of object)
        json_parser_skip_whitespace(parser);
        c = json_parser_peek(parser, 0, '\0');
        if (c == ',') {
            lastCommaPosition = parser->position;
            lastIsComma = true;
            json_parser_advance(parser, 1);
            continue;
        }
        // Was not a comma, must be end of object
        break;
    }

    // If trailing comma extension is not enabled, report an error
    if (lastIsComma && (parser->options.extensions & JSON_EXTENSION_TRAILING_COMMAS) == 0) {
        char* message = json_format("trailing comma in object is not allowed");
        json_parser_report_error(parser, lastCommaPosition, message);
    }

    // Done
    json_parser_expect_char(parser, '}');
    if (sax->on_object_end != NULL) sax->on_object_end(parser->user_data);
}

static void json_parse_value(Json_Parser* parser) {
    json_parser_skip_whitespace(parser);
    char c = json_parser_peek(parser, 0, '\0');
    if (c == '{') {
        json_parse_object_value(parser);
    }
    else if (c == '[') {
        json_parse_array_value(parser);
    }
    else if (c == '"') {
        json_parse_string_value(parser);
    }
    else if (isdigit(c) || c == '-') {
        json_parse_number_value(parser);
    }
    else if (json_isident(c)) {
        json_parse_identifier_value(parser);
    }
    else {
        char* message = json_format("unexpected character '%c' while parsing value", c);
        json_parser_report_error(parser, parser->position, message);
        // Report null value to still give them a value
        if (parser->sax.on_null != NULL) parser->sax.on_null(parser->user_data);
    }
}

void json_parse_sax(char const* json, Json_Sax sax, Json_Options options, void* user_data) {
    Json_Parser parser = {
        .text = json,
        .length = strlen(json),
        .position = { 0 },
        .sax = sax,
        .options = options,
        .user_data = user_data,
    };
    json_parse_value(&parser);
    // If we have remaining non-whitespace characters after parsing the value, that's an error
    json_parser_skip_whitespace(&parser);
    if (parser.position.index < parser.length) {
        char* message = json_format("unexpected character '%c' after parsing complete value", json_parser_peek(&parser, 0, '\0'));
        json_parser_report_error(&parser, parser.position, message);
    }
}

// DOM parsing

typedef struct Json_DomFrame {
    Json_Value value;
    // Relevant for objects only
    char const* last_key;
} Json_DomFrame;

typedef struct Json_DomBuilder {
    struct {
        Json_DomFrame* elements;
        size_t length;
        size_t capacity;
    } stack;
    Json_Document document;
    bool document_root_set;
} Json_DomBuilder;

static void json_dom_builder_push(Json_DomBuilder* builder, Json_Value value) {
    JSON_ADD_TO_ARRAY(builder->stack.elements, builder->stack.length, builder->stack.capacity, (Json_DomFrame){ .value = value, .last_key = NULL });
}

static Json_Value json_dom_builder_pop(Json_DomBuilder* builder) {
    JSON_ASSERT(builder->stack.length > 0, "attempted to pop from empty DOM builder stack");
    Json_DomFrame frame = builder->stack.elements[--builder->stack.length];
    JSON_ASSERT(frame.last_key == NULL, "DOM builder frame has pending object key on pop");
    return frame.value;
}

static void json_dom_builder_append_value(Json_DomBuilder* builder, Json_Value value) {
    if (builder->stack.length == 0) {
        // This is the root value, set it on the document
        JSON_ASSERT(!builder->document_root_set, "multiple root values in JSON document");
        builder->document.root = value;
        builder->document_root_set = true;
        return;
    }
    // Otherwise, we need to append it to the current container on top of the stack
    Json_DomFrame* current = &builder->stack.elements[builder->stack.length - 1];
    if (current->value.type == JSON_VALUE_ARRAY) {
        json_array_append(&current->value, value);
    }
    else if (current->value.type == JSON_VALUE_OBJECT) {
        JSON_ASSERT(current->last_key != NULL, "attempted to append value to object without a key in DOM builder");
        json_object_set(&current->value, current->last_key, value);
        // We assume that json_object_set copies, we need to free the key
        JSON_FREE(current->last_key);
        current->last_key = NULL;
    }
    else {
        JSON_ASSERT(false, "attempted to append value to non-container in DOM builder");
    }
}

static void json_dom_builder_on_null(void* user_data) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    json_dom_builder_append_value(builder, json_null());
}

static void json_dom_builder_on_bool(void* user_data, bool value) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    json_dom_builder_append_value(builder, json_bool(value));
}

static void json_dom_builder_on_int(void* user_data, long long value) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    json_dom_builder_append_value(builder, json_int(value));
}

static void json_dom_builder_on_double(void* user_data, double value) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    json_dom_builder_append_value(builder, json_double(value));
}

static void json_dom_builder_on_string(void* user_data, char const* value, size_t length) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    // The string is owned already, don't use json_string here
    json_dom_builder_append_value(builder, (Json_Value){
        .type = JSON_VALUE_STRING,
        .string_value = value,
    });
}

static void json_dom_builder_on_array_start(void* user_data) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    json_dom_builder_push(builder, json_array());
}

static void json_dom_builder_on_array_end(void* user_data) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    Json_Value array = json_dom_builder_pop(builder);
    json_dom_builder_append_value(builder, array);
}

static void json_dom_builder_on_object_start(void* user_data) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    json_dom_builder_push(builder, json_object());
}

static void json_dom_builder_on_object_key(void* user_data, char const* key, size_t length) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    // The key is owned already, we just need to keep track of it until we get the value
    Json_DomFrame* current = &builder->stack.elements[builder->stack.length - 1];
    current->last_key = key;
}

static void json_dom_builder_on_object_end(void* user_data) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    Json_Value object = json_dom_builder_pop(builder);
    json_dom_builder_append_value(builder, object);
}

static void json_dom_builder_on_error(void* user_data, Json_Error error) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    Json_Document* doc = &builder->document;
    JSON_ADD_TO_ARRAY(doc->errors.elements, doc->errors.length, doc->errors.capacity, error);
}

Json_Document json_parse(char const* json, Json_Options options) {
    // Configure our DOM builder as a SAX consumer
    Json_DomBuilder builder = { 0 };
    Json_Sax sax = {
        .on_null = json_dom_builder_on_null,
        .on_bool = json_dom_builder_on_bool,
        .on_int = json_dom_builder_on_int,
        .on_double = json_dom_builder_on_double,
        .on_string = json_dom_builder_on_string,
        .on_array_start = json_dom_builder_on_array_start,
        .on_array_end = json_dom_builder_on_array_end,
        .on_object_start = json_dom_builder_on_object_start,
        .on_object_key = json_dom_builder_on_object_key,
        .on_object_end = json_dom_builder_on_object_end,
        .on_error = json_dom_builder_on_error,
    };
    json_parse_sax(json, sax, options, &builder);
    // We must have cleaned up everything properly
    JSON_ASSERT(builder.stack.length == 0, "DOM builder stack is not empty after parsing complete document");
    // Deallocate the stack memory, we don't need it anymore
    JSON_FREE(builder.stack.elements);
    return builder.document;
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

// Array manipulation ////////////////////////////////////////////////////////////

void json_array_append(Json_Value* array, Json_Value value) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to append to non-array value");
    JSON_ADD_TO_ARRAY(array->array_value.elements, array->array_value.length, array->array_value.capacity, value);
}

void json_array_set(Json_Value* array, size_t index, Json_Value value) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to set index on non-array value");
    JSON_ASSERT(index < array->array_value.length, "attempted to set index out of bounds in array");
    // Free old value if needed
    json_free_value(&array->array_value.elements[index]);
    array->array_value.elements[index] = value;
}

Json_Value* json_array_get(Json_Value* array, size_t index) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to get index on non-array value");
    JSON_ASSERT(index < array->array_value.length, "attempted to get index out of bounds in array");
    return &array->array_value.elements[index];
}

void json_array_remove(Json_Value* array, size_t index) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to remove index on non-array value");
    JSON_ASSERT(index < array->array_value.length, "attempted to remove index out of bounds in array");
    Json_Value* elements = array->array_value.elements;
    // Free the value being removed
    json_free_value(&elements[index]);
    memmove(&elements[index], &elements[index + 1], (array->array_value.length - index - 1) * sizeof(Json_Value));
    --array->array_value.length;
}

// Object manipulation /////////////////////////////////////////////////////////

static size_t json_hash_string(char const* str) {
    size_t hash = 5381;
    while (*str != '\0') {
        hash = ((hash << 5) + hash) + (size_t)(unsigned char)(*str); // hash * 33 + c
        ++str;
    }
    return hash;
}

typedef struct Json_HashEntry {
    char const* key;
    Json_Value value;
    size_t hash;
} Json_HashEntry;

typedef struct Json_HashBucket {
    Json_HashEntry* entries;
    size_t length;
    size_t capacity;
} Json_HashBucket;

static const double Json_HashTable_UpsizeLoadFactor = 0.75;

static double json_hash_table_load_factor(Json_Value* value) {
    JSON_ASSERT(value->type == JSON_VALUE_OBJECT, "attempted to compute hash table load factor on non-object value");
    if (value->object_value.buckets == NULL) return 1.0;
    return (double)value->object_value.entry_count / (double)value->object_value.buckets_length;
}

static void json_hash_table_resize(Json_Value* value, size_t newBucketCount) {
    JSON_ASSERT(value->type == JSON_VALUE_OBJECT, "attempted to resize hash table on non-object value");
    Json_HashBucket* newBuckets = (Json_HashBucket*)JSON_REALLOC(NULL, newBucketCount * sizeof(Json_HashBucket));
    JSON_ASSERT(newBuckets != NULL, "failed to allocate memory for resized hash table buckets");
    memset(newBuckets, 0, newBucketCount * sizeof(Json_HashBucket));
    // Add each item from each bucket to the new bucket array, essentially redistributing
    for (size_t i = 0; i < value->object_value.buckets_length; ++i) {
        Json_HashBucket* oldBucket = &value->object_value.buckets[i];
        for (size_t j = 0; j < oldBucket->length; ++j) {
            Json_HashEntry entry = oldBucket->entries[j];
            size_t newBucketIndex = entry.hash % newBucketCount;
            Json_HashBucket* newBucket = &newBuckets[newBucketIndex];
            JSON_ADD_TO_ARRAY(newBucket->entries, newBucket->length, newBucket->capacity, entry);
        }
        // Old bucket entries have been moved to the new buckets, we can free the old bucket entries array
        JSON_FREE(oldBucket->entries);
    }
    // Free the old buckets and replace with the new ones
    JSON_FREE(value->object_value.buckets);
    value->object_value.buckets = newBuckets;
    value->object_value.buckets_length = newBucketCount;
}

static void json_hash_table_grow(Json_Value* value) {
    JSON_ASSERT(value->type == JSON_VALUE_OBJECT, "attempted to grow hash table on non-object value");
    // Let's double the size of the bucket array
    size_t newLength = value->object_value.buckets_length * 2;
    if (newLength < 8) newLength = 8;
    json_hash_table_resize(value, newLength);
}

void json_object_set(Json_Value* object, char const* key, Json_Value value) {
    JSON_ASSERT(object->type == JSON_VALUE_OBJECT, "attempted to set key-value pair on non-object value");
    // Grow if needed
    double loadFactor = json_hash_table_load_factor(object);
    if (loadFactor > Json_HashTable_UpsizeLoadFactor || object->object_value.buckets_length == 0) json_hash_table_grow(object);
    // Compute bucket index
    size_t hash = json_hash_string(key);
    size_t bucketIndex = hash % object->object_value.buckets_length;
    Json_HashBucket* bucket = &object->object_value.buckets[bucketIndex];
    // Check if the key already exists in the bucket, if so, replace the value
    for (size_t i = 0; i < bucket->length; ++i) {
        Json_HashEntry* entry = &bucket->entries[i];
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            // Free the old value if needed
            json_free_value(&entry->value);
            entry->value = value;
            return;
        }
    }
    // Key does not exist, add a new entry to the bucket
    JSON_ADD_TO_ARRAY(bucket->entries, bucket->length, bucket->capacity, (Json_HashEntry){
        // NOTE: We copy the key
        .key = json_strdup(key),
        .value = value,
        .hash = hash,
    });
    // New element was added
    ++object->object_value.entry_count;
}

Json_Value* json_object_get(Json_Value* object, char const* key) {
    JSON_ASSERT(object->type == JSON_VALUE_OBJECT, "attempted to get value by key on non-object value");
    if (object->object_value.buckets_length == 0) return NULL;
    // Compute bucket index
    size_t hash = json_hash_string(key);
    size_t bucketIndex = hash % object->object_value.buckets_length;
    Json_HashBucket* bucket = &object->object_value.buckets[bucketIndex];
    // Look for the key in the bucket
    for (size_t i = 0; i < bucket->length; ++i) {
        Json_HashEntry* entry = &bucket->entries[i];
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            return &entry->value;
        }
    }
    return NULL;
}

bool json_object_get_at(Json_Value* object, size_t index, char const** out_key, Json_Value* out_value) {
    JSON_ASSERT(object->type == JSON_VALUE_OBJECT, "attempted to get key-value pair by index on non-object value");
    if (object->object_value.buckets_length == 0) return false;
    size_t currentIndex = 0;
    for (size_t i = 0; i < object->object_value.buckets_length; ++i) {
        Json_HashBucket* bucket = &object->object_value.buckets[i];
        for (size_t j = 0; j < bucket->length; ++j) {
            if (currentIndex == index) {
                Json_HashEntry* entry = &bucket->entries[j];
                if (out_key != NULL) *out_key = entry->key;
                if (out_value != NULL) *out_value = entry->value;
                return true;
            }
            ++currentIndex;
        }
    }
    return false;
}

bool json_object_remove(Json_Value* object, char const* key, Json_Value* out_value) {
    JSON_ASSERT(object->type == JSON_VALUE_OBJECT, "attempted to remove key-value pair on non-object value");
    if (object->object_value.buckets_length == 0) return false;
    // Compute bucket index
    size_t hash = json_hash_string(key);
    size_t bucketIndex = hash % object->object_value.buckets_length;
    Json_HashBucket* bucket = &object->object_value.buckets[bucketIndex];
    // Look for the key in the bucket
    for (size_t i = 0; i < bucket->length; ++i) {
        Json_HashEntry* entry = &bucket->entries[i];
        if (entry->hash == hash && strcmp(entry->key, key) == 0) {
            // If old value is wanted, copy it out, otherwise free it
            if (out_value != NULL) *out_value = entry->value;
            else json_free_value(&entry->value);
            // Free the key
            JSON_FREE(entry->key);
            // Remove the entry by shifting the remaining entries
            memmove(&bucket->entries[i], &bucket->entries[i + 1], (bucket->length - i - 1) * sizeof(Json_HashEntry));
            --bucket->length;
            --object->object_value.entry_count;
            return true;
        }
    }
    return false;
}

// Resource release ////////////////////////////////////////////////////////////

void json_value_free(Json_Value* value) {
    switch (value->type) {
    case JSON_VALUE_STRING:
        JSON_FREE(value->string_value);
        break;
    case JSON_VALUE_ARRAY:
    {
        for (size_t i = 0; i < value->array_value.length; ++i) {
            json_value_free(&value->array_value.elements[i]);
        }
        JSON_FREE(value->array_value.elements);
    } break;
    case JSON_VALUE_OBJECT:
    {
        for (size_t i = 0; i < value->object_value.buckets_length; ++i) {
            Json_HashBucket* bucket = &value->object_value.buckets[i];
            for (size_t j = 0; j < bucket->length; ++j) {
                Json_HashEntry* entry = &bucket->entries[j];
                // Free the value and the key
                json_value_free(&entry->value);
                JSON_FREE(entry->key);
            }
            JSON_FREE(bucket->entries);
        }
        JSON_FREE(value->object_value.buckets);
    } break;
    default:
        // No resources to free for other types
        break;
    }
}

void json_document_free(Json_Document* doc) {
    json_value_free(&doc->root);
    JSON_FREE(doc->errors.elements);
}

#ifdef __cplusplus
}
#endif

#undef JSON_ADD_TO_ARRAY
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
