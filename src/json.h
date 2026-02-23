/**
 * json.h is a single-header JSON library for parsing, building and writing JSON format.
 *
 * Configuration:
 *  - #define JSON_IMPLEMENTATION before including this header in exactly one source file to include the implementation section
 *  - #define JSON_STATIC before including this header to make all functions have internal linkage
 *  - #define JSON_REALLOC and JSON_FREE to use custom memory allocation functions (by default they use realloc and free from the C standard library)
 *  - #define JSON_SELF_TEST before including this header to compile a self-test that verifies the library's functionality
 *  - #define JSON_EXAMPLE before including this header to compile a simple example that demonstrates how to use the library
 *
 * API:
 *  - The core types to represent JSON values and documents are Json_Value and Json_Document, which can represent any legal JSON structure
 *  - Use json_parse to parse a JSON string into a Json_Document, which contains the root JSON value and any errors encountered during parsing
 *  - Use json_parse_sax to parse a JSON string in a streaming fashion, invoking callbacks for parsing events
 *  - Use json_swrite or json_write to write a Json_Value into a JSON string
 *  - Use the various json_* constructor functions to create and manipulate JSON values, including objects and arrays (like json_object and json_array)
 *  - Use json_object_set, json_object_get and json_object_remove to manipulate key-value pairs in JSON objects
 *  - Use json_array_append, json_array_set and json_array_get to manipulate values in JSON arrays
 *  - Use json_free_value and json_free_document to free the memory associated with JSON values and documents when they are no longer needed
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

/**
 * Extension flags for the JSON parser, allowing it to support common, non-standard JSON features.
 */
typedef enum Json_Extension {
    JSON_EXTENSION_NONE = 0,
    JSON_EXTENSION_LINE_COMMENTS = 1 << 0,
    JSON_EXTENSION_BLOCK_COMMENTS = 1 << 1,
    JSON_EXTENSION_TRAILING_COMMAS = 1 << 2,
    JSON_EXTENSION_LEADING_ZEROS = 1 << 3,
    JSON_EXTENSION_ALL = JSON_EXTENSION_LINE_COMMENTS
                       | JSON_EXTENSION_BLOCK_COMMENTS
                       | JSON_EXTENSION_TRAILING_COMMAS
                       | JSON_EXTENSION_LEADING_ZEROS,
} Json_Extension;

/**
 * Options for the JSON parser and writer, allowing customization of their behavior.
 */
typedef struct Json_Options {
    // Enabled extensions, a combination of @see Json_Extension flags.
    Json_Extension extensions;
    // The string to use for newlines when writing. NULL means no newline.
    char const* newline_str;
    // The string to use for indentation when writing. NULL means no indentation.
    char const* indent_str;
} Json_Options;

/**
 * Represents an error encountered during JSON parsing.
 */
typedef struct Json_Error {
    // A human-readable message describing the error.
    // Owned by the document it's added to, the library will call JSON_FREE on it when the document is freed.
    char* message;
    // The line number where the error occurred, starting from 0.
    size_t line;
    // The column number where the error occurred, starting from 0.
    size_t column;
    // The index in the input string where the error occurred, starting from 0.
    size_t index;
} Json_Error;

/**
 * A set of callback functions for a SAX-style JSON parser, allowing the caller to handle JSON parsing events in a streaming fashion.
 */
typedef struct Json_Sax {
    // The callback when a null value is encountered.
    void(*on_null)(void* user_data);
    // The callback when a boolean value is encountered, with the boolean value as an argument.
    void(*on_bool)(void* user_data, bool value);
    // The callback when an integer value is encountered, with the integer value as an argument.
    void(*on_int)(void* user_data, long long value);
    // The callback when a double value is encountered, with the double value as an argument.
    void(*on_double)(void* user_data, double value);
    // The callback when a string value is encountered, with the string value and its length as arguments.
    // The string is null-terminated. If the callback is not null, the responsibility of freeing the string is on the caller.
    void(*on_string)(void* user_data, char* value, size_t length);
    // The callback when the start of an array is encountered.
    void(*on_array_start)(void* user_data);
    // The callback when the end of an array is encountered.
    void(*on_array_end)(void* user_data);
    // The callback when the start of an object is encountered.
    void(*on_object_start)(void* user_data);
    // The callback when a key in an object is encountered, with the key and its length as arguments.
    // The key is null-terminated. If the callback is not null, the responsibility of freeing the key is on the caller.
    void(*on_object_key)(void* user_data, char* key, size_t length);
    // The callback when the end of an object is encountered.
    void(*on_object_end)(void* user_data);
    // The callback when an error is encountered during parsing, with the error details as an argument.
    void(*on_error)(void* user_data, Json_Error error);
} Json_Sax;

/**
 * The different legal types of JSON values.
 */
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

/**
 * Represents a JSON value, which can be of various types including null, boolean, integer, double, string, array or object.
 */
typedef struct Json_Value {
    // The type of the JSON value, which determines the legal field to access in @see value.
    Json_ValueType type;
    union {
        bool boolean;
        long long integer;
        double floating;
        // Owned string
        char* string;
        struct {
            struct Json_Value* elements;
            size_t length;
            size_t capacity;
        } array;
        struct {
            struct Json_HashBucket* buckets;
            size_t buckets_length;
            size_t entry_count;
        } object;
    } value;
} Json_Value;

/**
 * Represents a JSON document, containing the root JSON value and any errors encountered during parsing.
 */
typedef struct Json_Document {
    Json_Value root;
    struct {
        Json_Error* elements;
        size_t length;
        size_t capacity;
    } errors;
} Json_Document;

// Parsing and writing API /////////////////////////////////////////////////////

/**
 * Parses the given JSON string using a SAX-style approach, invoking the provided callbacks for parsing events.
 * @param json The JSON string to parse.
 * @param sax The set of callback functions to invoke for parsing events.
 * @param options The options to customize the behavior of the parser.
 * @param user_data A pointer to user-defined data that will be passed to the callback functions.
 */
JSON_DEF void json_parse_sax(char const* json, Json_Sax sax, Json_Options options, void* user_data);

/**
 * Parses the given JSON string into a @see Json_Document structure, which contains the root JSON value and any errors encountered during parsing.
 * @param json The JSON string to parse.
 * @param options The options to customize the behavior of the parser.
 * @returns A @see Json_Document containing the parsed JSON value and any errors encountered during parsing.
 * The caller is responsible for freeing the document using @see json_free_document.
 */
JSON_DEF Json_Document json_parse(char const* json, Json_Options options);

/**
 * Writes the given JSON value into a string, writing it to the provided buffer.
 * @param value The JSON value to write.
 * @param options The options to customize the behavior of the writer.
 * @param buffer The buffer to write the JSON string to. Can be NULL to compute the required buffer size.
 * @param buffer_size The size of the buffer.
 * @returns The number of characters the JSON string would take up, excluding the null terminator.
 */
JSON_DEF size_t json_swrite(Json_Value value, Json_Options options, char* buffer, size_t buffer_size);

/**
 * Writes the given JSON value into a newly allocated string, which must be freed by the caller using JSON_FREE.
 * @param value The JSON value to write.
 * @param options The options to customize the behavior of the writer.
 * @param out_length A pointer to a size_t variable that will receive the length of the written string, excluding the null terminator. Can be NULL if the length is not needed.
 * @returns A newly allocated string containing the written JSON value.
 */
JSON_DEF char* json_write(Json_Value value, Json_Options options, size_t* out_length);

// Value construction //////////////////////////////////////////////////////////

/**
 * Creates a new JSON object value.
 * @returns A new, empty JSON object.
 */
JSON_DEF Json_Value json_object(void);

/**
 * Creates a new JSON array value.
 * @returns A new, empty JSON array.
 */
JSON_DEF Json_Value json_array(void);

/**
 * Creates a new JSON string value by copying the given string.
 * @param str The string to copy into the JSON value.
 * @returns A new JSON string value containing a copy of the given string.
 */
JSON_DEF Json_Value json_string(char const* str);

/**
 * Creates a new JSON integer number value.
 * @param value The integer value to store in the JSON value.
 * @returns A new JSON integer value containing the given integer.
 */
JSON_DEF Json_Value json_int(long long value);

/**
 * Creates a new JSON double number value.
 * @param value The double value to store in the JSON value.
 * @returns A new JSON double value containing the given double.
 */
JSON_DEF Json_Value json_double(double value);

/**
 * Creates a new JSON boolean value.
 * @param value The boolean value to store in the JSON value.
 * @returns A new JSON boolean value containing the given boolean.
 */
JSON_DEF Json_Value json_bool(bool value);

/**
 * Creates a new JSON null value.
 * @returns A new JSON null value.
 */
JSON_DEF Json_Value json_null(void);

/**
 * Creates a deep-copy of the given JSON value.
 * @param value The JSON value to copy.
 * @returns A new JSON value that is a deep copy of the given value.
 */
JSON_DEF Json_Value json_copy(Json_Value value);

// Resource cleanup ////////////////////////////////////////////////////////////

/**
 * Frees the memory associated with the given JSON value, including any nested values for arrays and objects.
 * @param value The JSON value to free.
 */
JSON_DEF void json_free_value(Json_Value* value);

/**
 * Frees the memory associated with the given JSON document, including the root value and any error messages.
 * @param doc The JSON document to free.
 */
JSON_DEF void json_free_document(Json_Document* doc);

// JSON Object manipulation ////////////////////////////////////////////////////

/**
 * Sets the value of the specified key in the given JSON object, replacing any existing value for that key.
 * @param object The JSON object to modify.
 * @param key The key to set in the object. It will be copied by the function, so the caller retains ownership of the key string.
 * @param value The value to set for the specified key. It will be shallow-copied by the function.
 */
JSON_DEF void json_object_set(Json_Value* object, char const* key, Json_Value value);

/**
 * Retrieves the value associated with the specified key in the given JSON object.
 * @param object The JSON object to query.
 * @param key The key to look up in the object.
 * @returns A pointer to the value associated with the specified key, or NULL if the key does not exist in the object.
 */
JSON_DEF Json_Value* json_object_get(Json_Value* object, char const* key);

/**
 * Retrieves the key and value at the specified index in the given JSON object.
 * @param object The JSON object to query.
 * @param index The index of the key-value pair to retrieve, starting from 0.
 * @param out_key A pointer to a char* variable that will receive the key string. The library retains ownership of the key string, so the caller should not free it.
 * @param out_value A pointer to a Json_Value variable that will receive the value. The library retains ownership of the value, so the caller should not free it.
 * @returns true if a key-value pair exists at the specified index and was retrieved successfully, or false if the index is out of bounds.
 */
JSON_DEF bool json_object_get_at(Json_Value* object, size_t index, char const** out_key, Json_Value* out_value);

/**
 * Removes the key-value pair with the specified key from the given JSON object.
 * @param object The JSON object to modify.
 * @param key The key of the key-value pair to remove from the object.
 * @param out_value A pointer to a @see Json_Value variable that will receive the removed value if the key was found and removed.
 * The caller takes ownership of the removed value and is responsible for freeing it using @see json_free_value.
 * Can be NULL if the removed value is not needed, in which case the library will free the removed value itself.
 */
JSON_DEF bool json_object_remove(Json_Value* object, char const* key, Json_Value* out_value);

// Array manipulation //////////////////////////////////////////////////////////

/**
 * Appends a value to the end of the given JSON array.
 * @param array The JSON array to modify.
 * @param value The value to append to the array. It will be shallow-copied by the function.
 */
JSON_DEF void json_array_append(Json_Value* array, Json_Value value);

/**
 * Inserts a value at the specified index in the given JSON array.
 * @param array The JSON array to modify.
 * @param index The index at which to insert the value, starting from 0. Must be less than or equal to the current length of the array.
 * If equal to the current length of the array, the call is equivalent to @see json_array_append.
 * @param value The value to insert at the specified index. It will be shallow-copied by the function.
 */
JSON_DEF void json_array_insert(Json_Value* array, size_t index, Json_Value value);

/**
 * Retrieves the value at the specified index in the given JSON array.
 * @param array The JSON array to query.
 * @param index The index of the value to retrieve, starting from 0.
 * @returns A pointer to the value at the specified index.
 */
JSON_DEF Json_Value* json_array_at(Json_Value* array, size_t index);

/**
 * Removes the value at the specified index from the given JSON array, shifting any subsequent values down to fill the gap.
 * @param array The JSON array to modify.
 * @param index The index of the value to remove, starting from 0.
 */
JSON_DEF void json_array_remove(Json_Value* array, size_t index);

// Safe accessors //////////////////////////////////////////////////////////////

/**
 * Moves the given JSON value out of its current location, leaving a null value in its place.
 * Useful for moving out of arrays or objects without shifting elements if a null-value is acceptable in the original location.
 * @param value The JSON value to move. It will be replaced with a null value.
 * @returns The original JSON value before it was replaced with null.
 */
JSON_DEF Json_Value json_move(Json_Value* value);

/**
 * Retrieves the length of the given JSON string, array or object, i.e. the string length, the number of elements in the array or
 * the number of key-value pairs in the object.
 * @param value The JSON value to query.
 * @returns The length of the JSON value if it is a string, array or object.
 */
JSON_DEF size_t json_length(Json_Value* value);

/**
 * Retrieves the integer value of the given JSON value if it is an integer.
 * @param value The JSON value to query.
 * @returns The integer value of the JSON value if it is an integer.
 */
JSON_DEF long long json_as_int(Json_Value* value);

/**
 * Retrieves the double value of the given JSON value if it is a double or an integer.
 * @param value The JSON value to query.
 * @returns The double value of the JSON value if it is a double or an integer.
 */
JSON_DEF double json_as_double(Json_Value* value);

/**
 * Retrieves the boolean value of the given JSON value if it is a boolean.
 * @param value The JSON value to query.
 * @returns The boolean value of the JSON value if it is a boolean.
 */
JSON_DEF bool json_as_bool(Json_Value* value);

/**
 * Retrieves the string value of the given JSON value if it is a string.
 * @param value The JSON value to query.
 * @returns The string value of the JSON value if it is a string. The returned string is null-terminated and owned by the library, so the caller should not free it.
 */
JSON_DEF char const* json_as_string(Json_Value* value);

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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct Json_HashEntry {
    char* key;
    Json_Value value;
    size_t hash;
} Json_HashEntry;

typedef struct Json_HashBucket {
    Json_HashEntry* entries;
    size_t length;
    size_t capacity;
} Json_HashBucket;

static const double Json_HashTable_UpsizeLoadFactor = 0.75;

// We whip up our own pow10 function to avoid depending on math.h, which might need extra linker flags on some platforms
static double json_pow10(int exp) {
    double result = 1;
    if (exp > 0) {
        for (int i = 0; i < exp; ++i) result *= 10;
    }
    else if (exp < 0) {
        for (int i = 0; i < -exp; ++i) result /= 10;
    }
    return result;
}

static bool json_isident(char c) {
    return isalpha((unsigned char)c) || isdigit((unsigned char)c) || c == '_';
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

static void json_parser_report_error(Json_Parser* parser, Json_Position position, char* message) {
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
    while (isspace((unsigned char)json_parser_peek(parser, 0, '\0'))) json_parser_advance(parser, 1);

    // We check for line comments here
    if (json_parser_peek(parser, 0, '\0') == '/' && json_parser_peek(parser, 1, '\0') == '/') {
        // If extension is not enabled, report an error
        if ((parser->options.extensions & JSON_EXTENSION_LINE_COMMENTS) == 0) {
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

    // And for block comments
    if (json_parser_peek(parser, 0, '\0') == '/' && json_parser_peek(parser, 1, '\0') == '*') {
        // If extension is not enabled, report an error
        if ((parser->options.extensions & JSON_EXTENSION_BLOCK_COMMENTS) == 0) {
            char* message = json_format("block comments are not allowed");
            json_parser_report_error(parser, parser->position, message);
            // We still continue the parser, treating the comment as whitespace
        }
        // Skip the comment
        json_parser_advance(parser, 2);
        while (true) {
            char c = json_parser_peek(parser, 0, '\0');
            if (c == '\0') {
                char* message = json_format("unexpected end of input in block comment");
                json_parser_report_error(parser, parser->position, message);
                return;
            }
            if (c == '*' && json_parser_peek(parser, 1, '\0') == '/') {
                json_parser_advance(parser, 2);
                break;
            }
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
    unsigned long long intValue = 0;
    bool negate = false;
    size_t parserOffset = 0;
    // Minus sign has to stick to the number, no whitespace allowed in between
    if (json_parser_peek(parser, parserOffset, '\0') == '-') {
        negate = true;
        ++parserOffset;
    }
    size_t digitStart = parserOffset;
    bool leadingZero = false;
    while (true) {
        char c = json_parser_peek(parser, parserOffset, '\0');
        if (!isdigit((unsigned char)c)) break;
        if (c == '0' && parserOffset == digitStart) leadingZero = true;
        // NOTE: Check below is written in a way that it only gets triggered ONCE
        if (leadingZero && parserOffset == digitStart + 1 && (parser->options.extensions & JSON_EXTENSION_LEADING_ZEROS) == 0) {
            char* message = json_format("leading zeros are not allowed in number values");
            json_parser_report_error(parser, parser->position, message);
            // We don't actually return here, for best-effort we just skip the leading zeros
        }
        intValue = intValue * 10 + (unsigned long long)(c - '0');
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
    // From here on we have a fraction and exponent part, both optional
    // Check, if either is coming up, if not, we can return an int value
    char next = json_parser_peek(parser, parserOffset, '\0');
    if (next != '.' && next != 'e' && next != 'E') {
        json_parser_advance(parser, parserOffset);
        long long signedValue = negate ? -(long long)intValue : (long long)intValue;
        if (sax->on_int != NULL) sax->on_int(parser->user_data, signedValue);
        return;
    }
    // We have a fraction or exponent part, we need to parse as double
    double doubleValue = (double)intValue;
    if (next == '.') {
        // Fractional part is present, skip dot
        ++parserOffset;
        size_t fractionDigitStart = parserOffset;
        double fractionMultiplier = 0.1;
        while (true) {
            char c = json_parser_peek(parser, parserOffset, '\0');
            if (!isdigit((unsigned char)c)) break;
            doubleValue += (c - '0') * fractionMultiplier;
            fractionMultiplier *= 0.1;
            ++parserOffset;
        }
        // Check that we have at least one digit in the fractional part
        if (parserOffset == fractionDigitStart) {
            char* message = json_format("expected digit after '.' in number value, but got '%c'", json_parser_peek(parser, parserOffset, '\0'));
            json_parser_report_error(parser, parser->position, message);
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
        size_t exponentDigitStart = parserOffset;
        int exponent = 0;
        while (true) {
            char c = json_parser_peek(parser, parserOffset, '\0');
            if (!isdigit((unsigned char)c)) break;
            exponent = exponent * 10 + (c - '0');
            ++parserOffset;
        }
        // Check that we have at least one digit in the exponent
        if (parserOffset == exponentDigitStart) {
            char* message = json_format("expected digit after exponent in number value, but got '%c'", json_parser_peek(parser, parserOffset, '\0'));
            json_parser_report_error(parser, parser->position, message);
        }
        if (expNegate) exponent = -exponent;
        doubleValue *= json_pow10(exponent);
    }
    // Apply sign at the end
    if (negate) doubleValue = -doubleValue;
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
    else if (isdigit((unsigned char)c) || c == '-') {
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
    char* last_key;
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
    Json_DomFrame frame = { .value = value, .last_key = NULL };
    JSON_ADD_TO_ARRAY(builder->stack.elements, builder->stack.length, builder->stack.capacity, frame);
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

static void json_dom_builder_on_string(void* user_data, char* value, size_t length) {
    Json_DomBuilder* builder = (Json_DomBuilder*)user_data;
    // The string is owned already, don't use json_string here
    json_dom_builder_append_value(builder, (Json_Value){
        .type = JSON_VALUE_STRING,
        .value = { .string = value },
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

static void json_dom_builder_on_object_key(void* user_data, char* key, size_t length) {
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

Json_Value json_copy(Json_Value value) {
    switch (value.type) {
    case JSON_VALUE_NULL:
    case JSON_VALUE_BOOL:
    case JSON_VALUE_INT:
    case JSON_VALUE_DOUBLE:
        // Don't need any deep-copy
        return value;
    case JSON_VALUE_STRING:
        return json_string(value.value.string);
    case JSON_VALUE_ARRAY: {
        Json_Value array = json_array();
        for (size_t i = 0; i < value.value.array.length; ++i) {
            json_array_append(&array, json_copy(value.value.array.elements[i]));
        }
        return array;
    }
    case JSON_VALUE_OBJECT: {
        Json_Value object = json_object();
        for (size_t i = 0; i < value.value.object.buckets_length; ++i) {
            Json_HashBucket* bucket = &value.value.object.buckets[i];
            for (size_t j = 0; j < bucket->length; ++j) {
                Json_HashEntry* entry = &bucket->entries[j];
                json_object_set(&object, entry->key, json_copy(entry->value));
            }
        }
        return object;
    }
    default:
        JSON_ASSERT(false, "invalid value type in json_copy");
        return json_null();
    }
}

Json_Value json_object(void) {
    return (Json_Value){
        .type = JSON_VALUE_OBJECT,
        .value = { .object = { 0 } },
    };
}

Json_Value json_array(void) {
    return (Json_Value){
        .type = JSON_VALUE_ARRAY,
        .value = { .array = { 0 } },
    };
}

Json_Value json_string(char const* str) {
    return (Json_Value){
        .type = JSON_VALUE_STRING,
        .value = { .string = json_strdup(str) },
    };
}

Json_Value json_int(long long value) {
    return (Json_Value){
        .type = JSON_VALUE_INT,
        .value = { .integer = value },
    };
}
Json_Value json_double(double value) {
    return (Json_Value){
        .type = JSON_VALUE_DOUBLE,
        .value = { .floating = value },
    };
}

Json_Value json_bool(bool value) {
    return (Json_Value){
        .type = JSON_VALUE_BOOL,
        .value = { .boolean = value },
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
    JSON_ADD_TO_ARRAY(array->value.array.elements, array->value.array.length, array->value.array.capacity, value);
}

void json_array_insert(Json_Value* array, size_t index, Json_Value value) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to insert index on non-array value");
    JSON_ASSERT(index <= array->value.array.length, "attempted to insert index out of bounds in array");
    if (index == array->value.array.length) {
        // Inserting at the current length is equivalent to appending
        json_array_append(array, value);
        return;
    }
    // Quite a stupid way, add to end to ensure capacity, shift, then insert at the right place
    JSON_ADD_TO_ARRAY(array->value.array.elements, array->value.array.length, array->value.array.capacity, value);
    memmove(&array->value.array.elements[index + 1], &array->value.array.elements[index], (array->value.array.length - index - 1) * sizeof(Json_Value));
    array->value.array.elements[index] = value;
}

Json_Value* json_array_at(Json_Value* array, size_t index) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to get index on non-array value");
    JSON_ASSERT(index < array->value.array.length, "attempted to get index out of bounds in array");
    return &array->value.array.elements[index];
}

void json_array_remove(Json_Value* array, size_t index) {
    JSON_ASSERT(array->type == JSON_VALUE_ARRAY, "attempted to remove index on non-array value");
    JSON_ASSERT(index < array->value.array.length, "attempted to remove index out of bounds in array");
    Json_Value* elements = array->value.array.elements;
    // Free the value being removed
    json_free_value(&elements[index]);
    memmove(&elements[index], &elements[index + 1], (array->value.array.length - index - 1) * sizeof(Json_Value));
    --array->value.array.length;
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

static double json_hash_table_load_factor(Json_Value* value) {
    JSON_ASSERT(value->type == JSON_VALUE_OBJECT, "attempted to compute hash table load factor on non-object value");
    if (value->value.object.buckets == NULL) return 1.0;
    return (double)value->value.object.entry_count / (double)value->value.object.buckets_length;
}

static void json_hash_table_resize(Json_Value* value, size_t newBucketCount) {
    JSON_ASSERT(value->type == JSON_VALUE_OBJECT, "attempted to resize hash table on non-object value");
    Json_HashBucket* newBuckets = (Json_HashBucket*)JSON_REALLOC(NULL, newBucketCount * sizeof(Json_HashBucket));
    JSON_ASSERT(newBuckets != NULL, "failed to allocate memory for resized hash table buckets");
    memset(newBuckets, 0, newBucketCount * sizeof(Json_HashBucket));
    // Add each item from each bucket to the new bucket array, essentially redistributing
    for (size_t i = 0; i < value->value.object.buckets_length; ++i) {
        Json_HashBucket* oldBucket = &value->value.object.buckets[i];
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
    JSON_FREE(value->value.object.buckets);
    value->value.object.buckets = newBuckets;
    value->value.object.buckets_length = newBucketCount;
}

static void json_hash_table_grow(Json_Value* value) {
    JSON_ASSERT(value->type == JSON_VALUE_OBJECT, "attempted to grow hash table on non-object value");
    // Let's double the size of the bucket array
    size_t newLength = value->value.object.buckets_length * 2;
    if (newLength < 8) newLength = 8;
    json_hash_table_resize(value, newLength);
}

void json_object_set(Json_Value* object, char const* key, Json_Value value) {
    JSON_ASSERT(object->type == JSON_VALUE_OBJECT, "attempted to set key-value pair on non-object value");
    // Grow if needed
    double loadFactor = json_hash_table_load_factor(object);
    if (loadFactor > Json_HashTable_UpsizeLoadFactor || object->value.object.buckets_length == 0) json_hash_table_grow(object);
    // Compute bucket index
    size_t hash = json_hash_string(key);
    size_t bucketIndex = hash % object->value.object.buckets_length;
    Json_HashBucket* bucket = &object->value.object.buckets[bucketIndex];
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
    Json_HashEntry newEntry = {
        // NOTE: We copy the key
        .key = json_strdup(key),
        .value = value,
        .hash = hash,
    };
    JSON_ADD_TO_ARRAY(bucket->entries, bucket->length, bucket->capacity, newEntry);
    // New element was added
    ++object->value.object.entry_count;
}

Json_Value* json_object_get(Json_Value* object, char const* key) {
    JSON_ASSERT(object->type == JSON_VALUE_OBJECT, "attempted to get value by key on non-object value");
    if (object->value.object.buckets_length == 0) return NULL;
    // Compute bucket index
    size_t hash = json_hash_string(key);
    size_t bucketIndex = hash % object->value.object.buckets_length;
    Json_HashBucket* bucket = &object->value.object.buckets[bucketIndex];
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
    if (object->value.object.buckets_length == 0) return false;
    size_t currentIndex = 0;
    for (size_t i = 0; i < object->value.object.buckets_length; ++i) {
        Json_HashBucket* bucket = &object->value.object.buckets[i];
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
    if (object->value.object.buckets_length == 0) return false;
    // Compute bucket index
    size_t hash = json_hash_string(key);
    size_t bucketIndex = hash % object->value.object.buckets_length;
    Json_HashBucket* bucket = &object->value.object.buckets[bucketIndex];
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
            --object->value.object.entry_count;
            return true;
        }
    }
    return false;
}

// Safe accessors //////////////////////////////////////////////////////////////

Json_Value json_move(Json_Value* value) {
    Json_Value movedValue = *value;
    *value = (Json_Value){ .type = JSON_VALUE_NULL };
    return movedValue;
}

size_t json_length(Json_Value* value) {
    switch (value->type) {
    case JSON_VALUE_ARRAY:
        return value->value.array.length;
    case JSON_VALUE_OBJECT:
        return value->value.object.entry_count;
    case JSON_VALUE_STRING:
        return strlen(value->value.string);
    default:
        JSON_ASSERT(false, "attempted to get length of non-array, non-object, non-string value");
        return 0;
    }
}

long long json_as_int(Json_Value* value) {
    if (value->type == JSON_VALUE_INT) return value->value.integer;
    if (value->type == JSON_VALUE_DOUBLE) return (long long)value->value.floating;
    JSON_ASSERT(false, "attempted to get int value of non-int, non-double value");
    return 0;
}

double json_as_double(Json_Value* value) {
    if (value->type == JSON_VALUE_DOUBLE) return value->value.floating;
    if (value->type == JSON_VALUE_INT) return (double)value->value.integer;
    JSON_ASSERT(false, "attempted to get double value of non-double, non-int value");
    return 0.0;
}

bool json_as_bool(Json_Value* value) {
    if (value->type == JSON_VALUE_BOOL) return value->value.boolean;
    JSON_ASSERT(false, "attempted to get bool value of non-bool value");
    return false;
}

char const* json_as_string(Json_Value* value) {
    if (value->type == JSON_VALUE_STRING) return value->value.string;
    JSON_ASSERT(false, "attempted to get string value of non-string value");
    return NULL;
}

// Writing /////////////////////////////////////////////////////////////////////

typedef struct Json_Writer {
    char* buffer;
    size_t length;
    size_t capacity;
    Json_Options options;
    size_t indent;
} Json_Writer;

static void json_writer_appendn(Json_Writer* writer, char const* str, size_t str_length) {
    if (str == NULL) return;
    if (writer->buffer != NULL && writer->length + str_length <= writer->capacity) {
        memcpy(writer->buffer + writer->length, str, str_length);
    }
    writer->length += str_length;
}

static void json_writer_append(Json_Writer* writer, char const* str) {
    if (str == NULL) return;
    json_writer_appendn(writer, str, strlen(str));
}

static void json_writer_append_char(Json_Writer* writer, char c) {
    if (writer->buffer != NULL && writer->length + 1 <= writer->capacity) {
        writer->buffer[writer->length] = c;
    }
    ++writer->length;
}

static void json_writer_append_indent(Json_Writer* writer) {
    if (writer->options.indent_str == NULL) return;
    for (size_t i = 0; i < writer->indent; ++i) {
        json_writer_append(writer, writer->options.indent_str);
    }
}

static void json_write_string_value(Json_Writer* writer, char const* str) {
    json_writer_append_char(writer, '"');
    while (*str != '\0') {
        char c = *str;
        switch (c) {
        case '"':
            json_writer_append(writer, "\\\"");
            break;
        case '\\':
            json_writer_append(writer, "\\\\");
            break;
        case '\b':
            json_writer_append(writer, "\\b");
            break;
        case '\f':
            json_writer_append(writer, "\\f");
            break;
        case '\n':
            json_writer_append(writer, "\\n");
            break;
        case '\r':
            json_writer_append(writer, "\\r");
            break;
        case '\t':
            json_writer_append(writer, "\\t");
            break;
        default:
            if (iscntrl((unsigned char)c)) {
                // Control characters must be escaped as \uXXXX
                char buffer[7];
                int length = snprintf(buffer, sizeof(buffer), "\\u%04x", (unsigned char)c);
                JSON_ASSERT(length == 6, "failed to write control character in JSON string value");
                json_writer_appendn(writer, buffer, (size_t)length);
            }
            else {
                // Regular character can be appended as is
                json_writer_append_char(writer, c);
            }
            break;
        }
        ++str;
    }
    json_writer_append_char(writer, '"');
}

static void json_write_value(Json_Writer* writer, Json_Value value) {
    switch (value.type) {
    case JSON_VALUE_NULL:
        json_writer_append(writer, "null");
        break;
    case JSON_VALUE_BOOL:
        json_writer_append(writer, value.value.boolean ? "true" : "false");
        break;
    case JSON_VALUE_INT: {
        char buffer[32];
        int length = snprintf(buffer, sizeof(buffer), "%lld", value.value.integer);
        JSON_ASSERT(length > 0 && (size_t)length < sizeof(buffer), "failed to write int value in JSON writer");
        json_writer_appendn(writer, buffer, (size_t)length);
    } break;
    case JSON_VALUE_DOUBLE: {
        char buffer[32];
        int length = snprintf(buffer, sizeof(buffer), "%g", value.value.floating);
        JSON_ASSERT(length > 0 && (size_t)length < sizeof(buffer), "failed to write double value in JSON writer");
        json_writer_appendn(writer, buffer, (size_t)length);
    } break;
    case JSON_VALUE_STRING:
        json_write_string_value(writer, value.value.string);
        break;
    case JSON_VALUE_ARRAY: {
        // For empty we can just append []
        if (value.value.array.length == 0) {
            json_writer_append(writer, "[]");
            break;
        }
        // For non-empty, we print each element on a new line with indentation
        json_writer_append_char(writer, '[');
        json_writer_append(writer, writer->options.newline_str);
        ++writer->indent;
        for (size_t i = 0; i < value.value.array.length; ++i) {
            json_writer_append_indent(writer);
            json_write_value(writer, value.value.array.elements[i]);
            if (i < value.value.array.length - 1) json_writer_append_char(writer, ',');
            json_writer_append(writer, writer->options.newline_str);
        }
        --writer->indent;
        json_writer_append_indent(writer);
        json_writer_append_char(writer, ']');
    } break;
    case JSON_VALUE_OBJECT: {
        // For empty we can just append {}
        if (value.value.object.entry_count == 0) {
            json_writer_append(writer, "{}");
            break;
        }
        // For non-empty, we print each property on a new line with indentation
        json_writer_append_char(writer, '{');
        json_writer_append(writer, writer->options.newline_str);
        ++writer->indent;
        size_t entryIndex = 0;
        for (size_t i = 0; i < value.value.object.buckets_length; ++i) {
            Json_HashBucket* bucket = &value.value.object.buckets[i];
            for (size_t j = 0; j < bucket->length; ++j, ++entryIndex) {
                Json_HashEntry* entry = &bucket->entries[j];
                json_writer_append_indent(writer);
                json_write_string_value(writer, entry->key);
                json_writer_append(writer, ": ");
                json_write_value(writer, entry->value);
                if (entryIndex < value.value.object.entry_count - 1) json_writer_append_char(writer, ',');
                json_writer_append(writer, writer->options.newline_str);
            }
        }
        --writer->indent;
        json_writer_append_indent(writer);
        json_writer_append_char(writer, '}');
    } break;
    default:
        JSON_ASSERT(false, "invalid value type in JSON writer");
        break;
    }
}

size_t json_swrite(Json_Value value, Json_Options options, char* buffer, size_t buffer_size) {
    Json_Writer writer = {
        .buffer = buffer,
        .length = 0,
        .capacity = buffer_size,
        .options = options,
        .indent = 0,
    };
    json_write_value(&writer, value);
    return writer.length;
}

char* json_write(Json_Value value, Json_Options options, size_t* out_length) {
    // Simply compute the length, then allocate a buffer of the needed size and write into it
    size_t length = json_swrite(value, options, NULL, 0);
    char* buffer = (char*)JSON_REALLOC(NULL, (length + 1) * sizeof(char));
    JSON_ASSERT(buffer != NULL, "failed to allocate buffer for JSON writing");
    json_swrite(value, options, buffer, length);
    buffer[length] = '\0';
    if (out_length != NULL) *out_length = length;
    return buffer;
}

// Resource release ////////////////////////////////////////////////////////////

void json_free_value(Json_Value* value) {
    switch (value->type) {
    case JSON_VALUE_STRING:
        JSON_FREE(value->value.string);
        // NULL out in case it's shared somewhere
        value->value.string = NULL;
        break;
    case JSON_VALUE_ARRAY: {
        for (size_t i = 0; i < value->value.array.length; ++i) {
            json_free_value(&value->value.array.elements[i]);
        }
        JSON_FREE(value->value.array.elements);
        // NULL out in case it's shared somewhere
        value->value.array.elements = NULL;
        value->value.array.length = 0;
        value->value.array.capacity = 0;
    } break;
    case JSON_VALUE_OBJECT: {
        for (size_t i = 0; i < value->value.object.buckets_length; ++i) {
            Json_HashBucket* bucket = &value->value.object.buckets[i];
            for (size_t j = 0; j < bucket->length; ++j) {
                Json_HashEntry* entry = &bucket->entries[j];
                // Free the value and the key
                json_free_value(&entry->value);
                JSON_FREE(entry->key);
            }
            JSON_FREE(bucket->entries);
        }
        JSON_FREE(value->value.object.buckets);
        // NULL out in case it's shared somewhere
        value->value.object.buckets = NULL;
        value->value.object.buckets_length = 0;
    } break;
    default:
        // No resources to free for other types
        break;
    }
}

void json_free_document(Json_Document* doc) {
    json_free_value(&doc->root);
    // Free each error message
    for (size_t i = 0; i < doc->errors.length; ++i) {
        JSON_FREE(doc->errors.elements[i].message);
    }
    JSON_FREE(doc->errors.elements);
    doc->errors.elements = NULL;
    doc->errors.length = 0;
    doc->errors.capacity = 0;
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

#include <stdio.h>
#include <string.h>

// Use our own testing library for self-testing
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

// Helper macros
#define ASSERT_NO_ERRORS(doc) CTEST_ASSERT_TRUE((doc).errors.length == 0)
#define ASSERT_HAS_ERRORS(doc) CTEST_ASSERT_TRUE((doc).errors.length > 0)

// Parsing primitives //////////////////////////////////////////////////////////

CTEST_CASE(parse_null) {
    Json_Document doc = json_parse("null", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_NULL);
    json_free_document(&doc);
}

CTEST_CASE(parse_true) {
    Json_Document doc = json_parse("true", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_BOOL);
    CTEST_ASSERT_TRUE(doc.root.value.boolean == true);
    json_free_document(&doc);
}

CTEST_CASE(parse_false) {
    Json_Document doc = json_parse("false", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_BOOL);
    CTEST_ASSERT_TRUE(doc.root.value.boolean == false);
    json_free_document(&doc);
}

CTEST_CASE(parse_integer) {
    Json_Document doc = json_parse("42", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_INT);
    CTEST_ASSERT_TRUE(doc.root.value.integer == 42);
    json_free_document(&doc);
}

CTEST_CASE(parse_negative_integer) {
    Json_Document doc = json_parse("-123", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_INT);
    CTEST_ASSERT_TRUE(doc.root.value.integer == -123);
    json_free_document(&doc);
}

CTEST_CASE(parse_double) {
    Json_Document doc = json_parse("3.14", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_DOUBLE);
    CTEST_ASSERT_TRUE(doc.root.value.floating > 3.13 && doc.root.value.floating < 3.15);
    json_free_document(&doc);
}

CTEST_CASE(parse_negative_double) {
    Json_Document doc = json_parse("-0.5", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_DOUBLE);
    CTEST_ASSERT_TRUE(doc.root.value.floating < -0.49 && doc.root.value.floating > -0.51);
    json_free_document(&doc);
}

CTEST_CASE(parse_double_with_exponent) {
    Json_Document doc = json_parse("1.5e10", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_DOUBLE);
    CTEST_ASSERT_TRUE(doc.root.value.floating > 1.4e10 && doc.root.value.floating < 1.6e10);
    json_free_document(&doc);
}

CTEST_CASE(parse_string) {
    Json_Document doc = json_parse("\"hello\"", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_STRING);
    CTEST_ASSERT_TRUE(strcmp(doc.root.value.string, "hello") == 0);
    json_free_document(&doc);
}

CTEST_CASE(parse_string_with_escapes) {
    Json_Document doc = json_parse("\"line1\\nline2\\ttab\"", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_STRING);
    CTEST_ASSERT_TRUE(strcmp(doc.root.value.string, "line1\nline2\ttab") == 0);
    json_free_document(&doc);
}

CTEST_CASE(parse_string_with_unicode) {
    Json_Document doc = json_parse("\"\\u0048\\u0065\\u006c\\u006c\\u006f\"", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_STRING);
    CTEST_ASSERT_TRUE(strcmp(doc.root.value.string, "Hello") == 0);
    json_free_document(&doc);
}

// Parsing arrays //////////////////////////////////////////////////////////////

CTEST_CASE(parse_empty_array) {
    Json_Document doc = json_parse("[]", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_ARRAY);
    CTEST_ASSERT_TRUE(doc.root.value.array.length == 0);
    json_free_document(&doc);
}

CTEST_CASE(parse_array_with_values) {
    Json_Document doc = json_parse("[1, 2, 3]", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_ARRAY);
    CTEST_ASSERT_TRUE(doc.root.value.array.length == 3);
    CTEST_ASSERT_TRUE(doc.root.value.array.elements[0].value.integer == 1);
    CTEST_ASSERT_TRUE(doc.root.value.array.elements[1].value.integer == 2);
    CTEST_ASSERT_TRUE(doc.root.value.array.elements[2].value.integer == 3);
    json_free_document(&doc);
}

// Parsing objects /////////////////////////////////////////////////////////////

CTEST_CASE(parse_empty_object) {
    Json_Document doc = json_parse("{}", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_OBJECT);
    CTEST_ASSERT_TRUE(doc.root.value.object.entry_count == 0);
    json_free_document(&doc);
}

CTEST_CASE(parse_object_with_properties) {
    Json_Document doc = json_parse("{\"name\": \"John\", \"age\": 30}", (Json_Options){0});
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.type == JSON_VALUE_OBJECT);
    CTEST_ASSERT_TRUE(doc.root.value.object.entry_count == 2);
    Json_Value* name = json_object_get(&doc.root, "name");
    Json_Value* age = json_object_get(&doc.root, "age");
    CTEST_ASSERT_TRUE(name != NULL && strcmp(name->value.string, "John") == 0);
    CTEST_ASSERT_TRUE(age != NULL && age->value.integer == 30);
    json_free_document(&doc);
}

// Error handling //////////////////////////////////////////////////////////////

CTEST_CASE(parse_leading_zeros_error) {
    Json_Document doc = json_parse("007", (Json_Options){0});
    ASSERT_HAS_ERRORS(doc);
    json_free_document(&doc);
}

CTEST_CASE(parse_leading_zeros_allowed_with_extension) {
    Json_Document doc = json_parse("007", (Json_Options){ .extensions = JSON_EXTENSION_LEADING_ZEROS });
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.value.integer == 7);
    json_free_document(&doc);
}

CTEST_CASE(parse_trailing_comma_error) {
    Json_Document doc = json_parse("[1, 2, 3,]", (Json_Options){0});
    ASSERT_HAS_ERRORS(doc);
    json_free_document(&doc);
}

CTEST_CASE(parse_trailing_comma_allowed_with_extension) {
    Json_Document doc = json_parse("[1, 2, 3,]", (Json_Options){ .extensions = JSON_EXTENSION_TRAILING_COMMAS });
    ASSERT_NO_ERRORS(doc);
    CTEST_ASSERT_TRUE(doc.root.value.array.length == 3);
    json_free_document(&doc);
}

// Builder API /////////////////////////////////////////////////////////////////

CTEST_CASE(build_object_manually) {
    Json_Value obj = json_object();
    json_object_set(&obj, "name", json_string("Alice"));
    json_object_set(&obj, "score", json_int(100));

    Json_Value* name = json_object_get(&obj, "name");
    Json_Value* score = json_object_get(&obj, "score");
    CTEST_ASSERT_TRUE(name != NULL && strcmp(name->value.string, "Alice") == 0);
    CTEST_ASSERT_TRUE(score != NULL && score->value.integer == 100);

    json_free_value(&obj);
}

CTEST_CASE(build_array_manually) {
    Json_Value arr = json_array();
    json_array_append(&arr, json_int(10));
    json_array_append(&arr, json_int(20));
    json_array_append(&arr, json_int(30));

    CTEST_ASSERT_TRUE(arr.value.array.length == 3);
    CTEST_ASSERT_TRUE(json_array_get(&arr, 0)->value.integer == 10);
    CTEST_ASSERT_TRUE(json_array_get(&arr, 1)->value.integer == 20);
    CTEST_ASSERT_TRUE(json_array_get(&arr, 2)->value.integer == 30);

    json_free_value(&arr);
}

CTEST_CASE(object_get_at_and_remove) {
    Json_Value obj = json_object();
    json_object_set(&obj, "a", json_int(1));
    json_object_set(&obj, "b", json_int(2));

    // Test get_at
    char const* key = NULL;
    Json_Value val;
    CTEST_ASSERT_TRUE(json_object_get_at(&obj, 0, &key, &val));
    CTEST_ASSERT_TRUE(key != NULL);

    // Test remove
    Json_Value removed;
    CTEST_ASSERT_TRUE(json_object_remove(&obj, "a", &removed));
    CTEST_ASSERT_TRUE(removed.value.integer == 1);
    CTEST_ASSERT_TRUE(obj.value.object.entry_count == 1);
    CTEST_ASSERT_TRUE(json_object_get(&obj, "a") == NULL);

    json_free_value(&obj);
}

CTEST_CASE(array_set_and_remove) {
    Json_Value arr = json_array();
    json_array_append(&arr, json_int(10));
    json_array_append(&arr, json_int(20));
    json_array_append(&arr, json_int(30));

    // Test set
    json_array_set(&arr, 1, json_int(99));
    CTEST_ASSERT_TRUE(json_array_get(&arr, 1)->value.integer == 99);

    // Test remove
    json_array_remove(&arr, 0);
    CTEST_ASSERT_TRUE(arr.value.array.length == 2);
    CTEST_ASSERT_TRUE(json_array_get(&arr, 0)->value.integer == 99);
    CTEST_ASSERT_TRUE(json_array_get(&arr, 1)->value.integer == 30);

    json_free_value(&arr);
}

// Writing /////////////////////////////////////////////////////////////////////

CTEST_CASE(write_primitives) {
    Json_Options opts = { .newline_str = "\n", .indent_str = "  " };

    char* null_str = json_write(json_null(), opts, NULL);
    char* true_str = json_write(json_bool(true), opts, NULL);
    char* int_str = json_write(json_int(42), opts, NULL);
    char* str_str = json_write(json_string("hello"), opts, NULL);

    CTEST_ASSERT_TRUE(strcmp(null_str, "null") == 0);
    CTEST_ASSERT_TRUE(strcmp(true_str, "true") == 0);
    CTEST_ASSERT_TRUE(strcmp(int_str, "42") == 0);
    CTEST_ASSERT_TRUE(strcmp(str_str, "\"hello\"") == 0);

    free(null_str);
    free(true_str);
    free(int_str);
    free(str_str);
}

CTEST_CASE(write_string_escapes) {
    Json_Value val = json_string("line1\nline2\ttab");
    char* str = json_write(val, (Json_Options){0}, NULL);
    CTEST_ASSERT_TRUE(strcmp(str, "\"line1\\nline2\\ttab\"") == 0);
    free(str);
    json_free_value(&val);
}

CTEST_CASE(write_roundtrip) {
    char const* original = "{\"name\": \"Bob\", \"active\": true}";
    Json_Document doc = json_parse(original, (Json_Options){0});
    ASSERT_NO_ERRORS(doc);

    char* reWritten = json_write(doc.root, (Json_Options){0}, NULL);
    Json_Document doc2 = json_parse(reWritten, (Json_Options){0});
    ASSERT_NO_ERRORS(doc2);

    // Verify the re-parsed values match
    Json_Value* name = json_object_get(&doc2.root, "name");
    Json_Value* active = json_object_get(&doc2.root, "active");
    CTEST_ASSERT_TRUE(name != NULL && strcmp(name->value.string, "Bob") == 0);
    CTEST_ASSERT_TRUE(active != NULL && active->value.boolean == true);

    free(reWritten);
    json_free_document(&doc);
    json_free_document(&doc2);
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
    // Parse a JSON string
    char const* input = "{\"name\": \"Alice\", \"scores\": [85, 92, 78]}";
    Json_Document doc = json_parse(input, (Json_Options){0});

    if (doc.errors.length > 0) {
        printf("Parse error: %s\n", doc.errors.elements[0].message);
        json_free_document(&doc);
        return 1;
    }

    // Access and print values
    Json_Value* name = json_object_get(&doc.root, "name");
    printf("Name: %s\n", name->value.string);

    Json_Value* scores = json_object_get(&doc.root, "scores");
    printf("Scores: ");
    for (size_t i = 0; i < scores->value.array.length; ++i) {
        printf("%lld ", scores->value.array.elements[i].value.integer);
    }
    printf("\n");

    // Modify the document
    json_object_set(&doc.root, "active", json_bool(true));
    json_array_append(scores, json_int(95));

    // Write with pretty printing
    Json_Options opts = { .newline_str = "\n", .indent_str = "  " };
    char* output = json_write(doc.root, opts, NULL);
    printf("\nModified JSON:\n%s\n", output);

    // Cleanup
    free(output);
    json_free_document(&doc);
    return 0;
}

#endif /* JSON_EXAMPLE */
