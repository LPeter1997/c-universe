/**
 * liquid_c.h is a single-header liquid template engine implementation in C.
 *
 * Useful for embedding for tools that need to emit code but also require user-facing templating.
 *
 * Before #include-ing this header, #define LIQUID_C_IMPLEMENTATION in the file you want to paste the implementation.
 *
 * Configuration:
 *  - #define LIQUID_C_STATIC to have the implementations prefixed with 'static', otherwise they will be 'extern'
 *  - #define LIQUID_C_ALLOC, LIQUID_C_REALLOC and LIQUID_C_FREE to override allocation methods, by default 'malloc', 'realloc' and 'free' are used
 *
 * API:
 *  - LiquidString and functions prefixed with 'LiquidString_' for string manipulation
 *  - TODO
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef LIQUID_C_H
#define LIQUID_C_H

#ifdef LIQUID_C_STATIC
#   define LIQUID_C_DEF static
#else
#   define LIQUID_C_DEF extern
#endif

#ifndef LIQUID_C_ALLOC
#   define LIQUID_C_ALLOC malloc
#endif
#ifndef LIQUID_C_REALLOC
#   define LIQUID_C_REALLOC realloc
#endif
#ifndef LIQUID_C_FREE
#   define LIQUID_C_FREE free
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Type declarations ///////////////////////////////////////////////////////////

typedef struct LiquidValue LiquidValue;

/**
 * A dynamic null-terminated string type.
 */
typedef struct LiquidString {
    // Owned, 0-terminated string data
    char* data;
    // Allocated capacity of the string, including the 0 terminator
    size_t capacity;
    // Length of the string, including the 0 terminator
    size_t length;
} LiquidString;

/**
 * A dynamic list of liquid values.
 */
typedef struct LiquidList {
    // Owned array of values
    LiquidValue* items;
    // Allocated capacity of the list
    size_t capacity;
    // Number of items currently in the list
    size_t length;
} LiquidList;

typedef struct LiquidDictEntry {
    LiquidString key;
    uint32_t hash;
    LiquidValue value;
} LiquidDictEntry;

typedef struct LiquidDictBucket {
    LiquidDictEntry* entries;
    size_t capacity;
    size_t length;
} LiquidDictBucket;

/**
 * An associative container mapping string keys to values.
 */
typedef struct LiquidDict {
    // Owned array of buckets
    LiquidDictBucket* buckets;
    // Allocated capacity of the bucket array
    size_t capacity; 
    // Number of buckets currently in the bucket array
    size_t length;
} LiquidDict;

// TODO
typedef struct LiquidValue {
    // TODO
} LiquidValue;

// LiquidString API ////////////////////////////////////////////////////////////

/**
 * Creates a new string from the given data.
 * The data is copied into the string, so the caller retains ownership of the input data.
 * @param text The text to copy into the string.
 * @param length The length of the text to copy.
 * @return A new string containing the copied text.
 */
LIQUID_C_DEF LiquidString LiquidString_from_data(char const* text, size_t length);

/**
 * Creates a new string from a C-style null-terminated string.
 * The input string is copied into the new string, so the caller retains ownership of the input string.
 * @param cstr The C-style string to copy into the new string.
 * @return A new string containing the copied text.
 */
LIQUID_C_DEF LiquidString LiquidString_from_cstr(char const* cstr);

/**
 * Creates a new empty string.
 * @return An empty string.
 */
LIQUID_C_DEF LiquidString LiquidString_new();

/**
 * Deletes a string, freeing its resources.
 * @param str The string to delete.
 */
LIQUID_C_DEF void LiquidString_delete(LiquidString* str);

/**
 * Ensures that the string has at least the given capacity.
 * @param str The string to ensure capacity for.
 * @param capacity The minimum capacity to ensure for the string, including the 0 terminator.
 */
LIQUID_C_DEF void LiquidString_ensure_capacity(LiquidString* str, size_t capacity);

/**
 * Inserts the given text into the string at the specified index.
 * @param str The string to insert into.
 * @param index The index to insert the text at, must be between 0 and the current length of the string (inclusive).
 * @param text The text to insert into the string.
 * @param length The length of the text to insert.
 */
LIQUID_C_DEF void LiquidString_insert_data(LiquidString* str, size_t index, char const* text, size_t length);

/**
 * Inserts the given character into the string at the specified index.
 * @param str The string to insert into.
 * @param index The index to insert the character at, must be between 0 and the current length of the string (inclusive).
 * @param ch The character to insert into the string.
 */
LIQUID_C_DEF void LiquidString_insert_char(LiquidString* str, size_t index, char ch);

/**
 * Inserts the given string into the string at the specified index.
 * @param str The string to insert into.
 * @param index The index to insert the string at, must be between 0 and the current length of the string (inclusive).
 * @param cstr The C-style string to insert into the string.
 */
LIQUID_C_DEF void LiquidString_insert_cstr(LiquidString* str, size_t index, char const* cstr);

/**
 * Inserts the given string into the string at the specified index.
 * @param str The string to insert into.
 * @param index The index to insert the string at, must be between 0 and the current length of the string (inclusive).
 * @param other The string to insert into the string.
 */
LIQUID_C_DEF void LiquidString_insert_string(LiquidString* str, size_t index, LiquidString other);

/**
 * Appends the given text to the end of the string.
 * @param str The string to append to.
 * @param text The text to append to the string.
 * @param length The length of the text to append.
 */
LIQUID_C_DEF void LiquidString_append_data(LiquidString* str, char const* text, size_t length);

/**
 * Appends the given character to the end of the string.
 * @param str The string to append to.
 * @param ch The character to append to the string.
 */
LIQUID_C_DEF void LiquidString_append_char(LiquidString* str, char ch);

/**
 * Appends the given C-style string to the end of the string.
 * @param str The string to append to.
 * @param cstr The C-style string to append to the string.
 */
LIQUID_C_DEF void LiquidString_append_cstr(LiquidString* str, char const* cstr);

/**
 * Appends the given string to the end of the string.
 * @param str The string to append to.
 * @param other The string to append to the string.
 */
LIQUID_C_DEF void LiquidString_append_string(LiquidString* str, LiquidString other);

/**
 * Removes the specified range of characters from the string.
 * @param str The string to remove from.
 * @param index The index to start removing characters from, must be between 0 and the current length of the string (exclusive).
 * @param length The number of characters to remove, must be such that index + length is between 0 and the current length of the string (inclusive).
 */
LIQUID_C_DEF void LiquidString_remove(LiquidString* str, size_t index, size_t length);

// LiquidList API //////////////////////////////////////////////////////////////

/**
 * Creates a new empty list.
 * @return An empty list.
 */
LIQUID_C_DEF LiquidList LiquidList_new();

/**
 * Deletes a list, freeing its resources.
 * @param list The list to delete.
 */
LIQUID_C_DEF void LiquidList_delete(LiquidList* list);

/**
 * Ensures that the list has at least the given capacity.
 * @param list The list to ensure capacity for.
 * @param capacity The minimum capacity to ensure for the list.
 */
LIQUID_C_DEF void LiquidList_ensure_capacity(LiquidList* list, size_t capacity);

/**
 * Inserts the given range of values into the list at the specified index.
 * @param list The list to insert into.
 * @param index The index to insert the values at, must be between 0 and the current length of the list (inclusive).
 * @param values The array of values to insert into the list.
 * @param length The number of values to insert from the array.
 */
LIQUID_C_DEF void LiquidList_insert_range(LiquidList* list, size_t index, LiquidValue* values, size_t length);

/**
 * Inserts the given value into the list at the specified index.
 * @param list The list to insert into.
 * @param index The index to insert the value at, must be between 0 and the current length of the list (inclusive).
 * @param value The value to insert into the list.
 */
LIQUID_C_DEF void LiquidList_insert(LiquidList* list, size_t index, LiquidValue value);

/**
 * Appends the given value to the end of the list.
 * @param list The list to append to.
 * @param value The value to append to the list.
 */
LIQUID_C_DEF void LiquidList_append(LiquidList* list, LiquidValue value);

/**
 * Removes the specified range of values from the list.
 * @param list The list to remove from.
 * @param index The index to start removing values from, must be between 0 and the current length of the list (exclusive).
 * @param length The number of values to remove, must be such that index + length is between 0 and the current length of the list (inclusive).
 */
LIQUID_C_DEF void LiquidList_remove_range(LiquidList* list, size_t index, size_t length);

/**
 * Removes the value at the specified index from the list.
 * @param list The list to remove from.
 * @param index The index of the value to remove, must be between 0 and the current length of the list (exclusive).
 */
LIQUID_C_DEF void LiquidList_remove(LiquidList* list, size_t index);

// LiquidDict API //////////////////////////////////////////////////////////////

/**
 * Creates a new empty dictionary.
 * @return An empty dictionary.
 */
LIQUID_C_DEF LiquidDict LiquidDict_new();

/**
 * Deletes a dictionary, freeing its resources.
 * @param dict The dictionary to delete.
 */
LIQUID_C_DEF void LiquidDict_delete(LiquidDict* dict);

/**
 * 
 */
LIQUID_C_DEF void LiquidDict_insert(LiquidDict* dict, LiquidString key, LiquidValue value);

LIQUID_C_DEF void LiquidDict_insert_cstr(LiquidDict* dict, char const* key, LiquidValue value);

LIQUID_C_DEF bool LiquidDict_get(LiquidDict* dict, LiquidString key, LiquidValue* out_value); 

LIQUID_C_DEF bool LiquidDict_get_cstr(LiquidDict* dict, char const* key, LiquidValue* out_value);

LIQUID_C_DEF bool LiquidDict_remove(LiquidDict* dict, LiquidString key, bool* out_removed); LIQUID_C_DEF void LiquidDict_remove_cstr(LiquidDict* dict, char const* key);

#ifdef __cplusplus
}
#endif

#endif /* LIQUID_C_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef LIQUID_C_IMPLEMENTATION

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIQUID_C_ASSERT(condition, message) assert(((void)message, condition))

// LiquidString implementation /////////////////////////////////////////////////

LiquidString LiquidString_from_data(char const* text, size_t length) {
    char* data = (char*)LIQUID_C_ALLOC(sizeof(char) * (length + 1));
    LIQUID_C_ASSERT(data != NULL, "failed to allocate memory for string data");
    memcpy(data, text, length);
    data[length] = '\0';
    return (LiquidString){
        .data = data,
        .capacity = length + 1,
        .length = length + 1,
    };
}

LiquidString LiquidString_from_cstr(char const* cstr) {
    return LiquidString_from_data(cstr, strlen(cstr));
}

LiquidString LiquidString_new() {
    return LiquidString_from_data("", 0);
}

void LiquidString_delete(LiquidString* str) {
    LIQUID_C_FREE(str->data);
    str->data = NULL;
    str->capacity = 0;
    str->length = 0;
}

void LiquidString_ensure_capacity(LiquidString* str, size_t capacity) {
    if (capacity <= str->capacity) return;

    size_t new_capacity = str->capacity == 0 ? 8 : str->capacity * 2;
    while (new_capacity < capacity) new_capacity *= 2;

    str->data = (char*)LIQUID_C_REALLOC(str->data, sizeof(char) * new_capacity);
    LIQUID_C_ASSERT(str->data != NULL, "failed to reallocate memory for string data");
    str->capacity = new_capacity;
}

void LiquidString_insert_data(LiquidString* str, size_t index, char const* text, size_t length) {
    LIQUID_C_ASSERT(index <= str->length, "index out of bounds for string insert");
    LiquidString_ensure_capacity(str, str->length + length);
    memmove(str->data + index + length, str->data + index, str->length - index);
    memcpy(str->data + index, text, length);
    str->length += length;
}

void LiquidString_insert_char(LiquidString* str, size_t index, char ch) {
    LiquidString_insert_data(str, index, &ch, 1);
}

void LiquidString_insert_cstr(LiquidString* str, size_t index, char const* cstr) {
    LiquidString_insert_data(str, index, cstr, strlen(cstr));
}

void LiquidString_insert_string(LiquidString* str, size_t index, LiquidString other) {
    LiquidString_insert_data(str, index, other.data, other.length - 1);
}

void LiquidString_append_data(LiquidString* str, char const* text, size_t length) {
    LiquidString_insert_data(str, str->length - 1, text, length);
}

void LiquidString_append_char(LiquidString* str, char ch) {
    LiquidString_append_data(str, &ch, 1);
}

void LiquidString_append_cstr(LiquidString* str, char const* cstr) {
    LiquidString_append_data(str, cstr, strlen(cstr));
}

void LiquidString_append_string(LiquidString* str, LiquidString other) {
    LiquidString_append_data(str, other.data, other.length - 1);
}

void LiquidString_remove(LiquidString* str, size_t index, size_t length) {
    LIQUID_C_ASSERT(index + length <= str->length - 1, "index and length out of bounds for string remove");
    memmove(str->data + index, str->data + index + length, str->length - index - length);
    str->length -= length;
}

// LiquidList implementation ///////////////////////////////////////////////////

LiquidList LiquidList_new() {
    return (LiquidList){
        .items = NULL,
        .capacity = 0,
        .length = 0,
    };
}

void LiquidList_delete(LiquidList* list) {
    LIQUID_C_FREE(list->items);
    list->items = NULL;
    list->capacity = 0;
    list->length = 0;
}

void LiquidList_ensure_capacity(LiquidList* list, size_t capacity) {
    if (capacity <= list->capacity) return;

    size_t new_capacity = list->capacity == 0 ? 8 : list->capacity * 2;
    while (new_capacity < capacity) new_capacity *= 2;

    list->items = (LiquidValue*)LIQUID_C_REALLOC(list->items, sizeof(LiquidValue) * new_capacity);
    LIQUID_C_ASSERT(list->items != NULL, "failed to reallocate memory for list items");
    list->capacity = new_capacity;
}

void LiquidList_insert_range(LiquidList* list, size_t index, LiquidValue* values, size_t length) {
    LIQUID_C_ASSERT(index <= list->length, "index out of bounds for list insert");
    LiquidList_ensure_capacity(list, list->length + length);
    memmove(list->items + index + length, list->items + index, sizeof(LiquidValue) * (list->length - index));
    memcpy(list->items + index, values, sizeof(LiquidValue) * length);
    list->length += length;
}

void LiquidList_insert(LiquidList* list, size_t index, LiquidValue value) {
    LiquidList_insert_range(list, index, &value, 1);
}

void LiquidList_append(LiquidList* list, LiquidValue value) {
    LiquidList_insert(list, list->length, value);
}

void LiquidList_remove_range(LiquidList* list, size_t index, size_t length) {
    LIQUID_C_ASSERT(index + length <= list->length, "index and length out of bounds for list remove");
    memmove(list->items + index, list->items + index + length, sizeof(LiquidValue) * (list->length - index - length));
    list->length -= length;
}

void LiquidList_remove(LiquidList* list, size_t index) {
    LiquidList_remove_range(list, index, 1);
}

#undef LIQUID_C_ASSERT

#endif
