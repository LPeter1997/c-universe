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

JSON_DEF char* json_format(char const* format, ...);
JSON_DEF char* json_vformat(char const* format, va_list args);

#ifdef __cplusplus
}
#endif

#endif /* JSON_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef JSON_IMPLEMENTATION

#include <assert.h>

#define JSON_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Implementation goes here

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
