#include "../src/collections.h"

#define JSON_IMPLEMENTATION
#define JSON_STATIC
#include "../src/json.h"

// TODO: It would be very nice if a library provided easier IO functions that could let me do all this in one go
static char const* read_file(char const* path) {
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
    // TODO: Would be nice to have a generic sorting algo at hand, instead to put all composite types at the end,
    // we remove them, collect them out to a separate list, then add them back at the end
    Json_Value* operandKinds = json_object_get(&doc.root, "operand_kinds");
    DynamicArray(Json_Value) compositeTypes = {0};
    // TODO: Lackluster API, accessing array length should not involve diving into the guts of the union
    // Also makes it EXTREMELY easy to cause UB as the access is not validated
    for (size_t i = 0; i < operandKinds->value.array.length; ) {
        Json_Value* operandKind = json_array_get(operandKinds, i);
        Json_Value* category = json_object_get(operandKind, "category");
        // TODO: Same here, maybe add asserting accessors?
        if (strcmp(category->value.string, "Composite") != 0) {
            ++i;
            continue;
        }
        // TODO: We have no way to MOVE a value out of the structures without deallocating them
        // We should consider fixing the API
        DynamicArray_append(compositeTypes, json_copy(*operandKind));
        json_array_remove(operandKinds, i);
    }
    for (size_t i = 0; i < DynamicArray_length(compositeTypes); ++i) {
        Json_Value operandKind = DynamicArray_at(compositeTypes, i);
        json_array_append(operandKinds, operandKind);
    }
    DynamicArray_free(compositeTypes);
}

int main(void) {
    char const* jsonStr = read_file("../third_party/spirv.core.grammar.json");
    if (!jsonStr) {
        fprintf(stderr, "Failed to read JSON file\n");
        return 1;
    }
    Json_Document doc = json_parse(jsonStr, (Json_Options){0});
    free((void*)jsonStr);
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
