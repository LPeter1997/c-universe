#include <stdio.h>

#include "../src/collections.h"

#define JSON_IMPLEMENTATION
#define JSON_STATIC
#include "../src/json.h"

// TODO: It would be very nice if a library provided easier IO functions that could let me do all this in one go
static char* read_file(char const* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* content = malloc(length + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);
    return content;
}

static void json_model_to_domain(Json_Document doc) {
    // Reorder operand_kinds to have Composite at the end
    // TODO: Would be nice to have a generic sorting algo at hand, instead to put all composite types at the end,
    // we remove them, collect them out to a separate list, then add them back at the end
    Json_Value* operandKinds = json_object_get(&doc.root, "operand_kinds");
    DynamicArray(Json_Value) compositeTypes = {0};
    for (size_t i = 0; i < json_length(operandKinds); ) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        Json_Value* category = json_object_get(operandKind, "category");
        if (strcmp(json_as_string(category), "Composite") != 0) {
            ++i;
            continue;
        }
        DynamicArray_append(compositeTypes, json_move(operandKind));
        json_array_remove(operandKinds, i);
    }
    for (size_t i = 0; i < DynamicArray_length(compositeTypes); ++i) {
        Json_Value* operandKind = &DynamicArray_at(compositeTypes, i);
        json_array_append(operandKinds, json_move(operandKind));
    }
    DynamicArray_free(compositeTypes);

    // We go through each enumerant of each operand_kind and if it has an "aliases" array, we unroll it by
    // cloning the enumerant for each alias and replacing the name with the alias
    for (size_t i = 0; i < json_length(operandKinds); ++i) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        Json_Value* enumerants = json_object_get(operandKind, "enumerants");
        if (enumerants == NULL) continue;
        DynamicArray(Json_Value) newEnumerants = {0};
        for (size_t j = 0; j < json_length(enumerants); ++j) {
            Json_Value* enumerant = json_array_at(enumerants, j);
            Json_Value* aliases = json_object_get(enumerant, "aliases");
            if (aliases == NULL) continue;
            for (size_t k = 0; k < json_length(aliases); ++k) {
                Json_Value* alias = json_array_at(aliases, k);
                Json_Value newEnumerant = json_copy(*enumerant);
                json_object_remove(&newEnumerant, "aliases", NULL);
                json_object_set(&newEnumerant, "name", json_move(alias));
                DynamicArray_append(newEnumerants, json_move(&newEnumerant));
            }
            json_object_remove(enumerant, "aliases", NULL);
        }
        for (size_t j = 0; j < DynamicArray_length(newEnumerants); ++j) {
            Json_Value* newEnumerant = &DynamicArray_at(newEnumerants, j);
            json_array_append(enumerants, json_move(newEnumerant));
        }
        DynamicArray_free(newEnumerants);
    }
}

int main(void) {
    char* jsonStr = read_file("../third_party/spirv.core.grammar.json");
    if (!jsonStr) {
        fprintf(stderr, "Failed to read JSON file\n");
        return 1;
    }
    Json_Document doc = json_parse(jsonStr, (Json_Options){0});
    free(jsonStr);
    if (doc.errors.length > 0) {
        fprintf(stderr, "Failed to parse JSON: %s\n", doc.errors.elements[0].message);
        json_free_document(&doc);
        return 1;
    }
    json_model_to_domain(doc);
    // For now this just manipulates the JSON, reprint to check
    char* output = json_write(doc.root, (Json_Options){ .newline_str = "\n", .indent_str = "  " }, NULL);
    printf("%s\n", output);
    free(output);
    json_free_document(&doc);
    return 0;
}
