#include <stdio.h>

#include "../src/collections.h"

#define JSON_IMPLEMENTATION
#define JSON_STATIC
#include "../src/json.h"

#define STRING_BUILDER_IMPLEMENTATION
#define STRING_BUILDER_STATIC
#include "../src/string_builder.h"

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

static void json_flatten_aliases(Json_Value* array, char const* nameKey) {
    if (array == NULL) return;
    DynamicArray(Json_Value) newValues = {0};
    for (size_t i = 0; i < json_length(array); ++i) {
        Json_Value* value = json_array_at(array, i);
        Json_Value* aliases = json_object_get(value, "aliases");
        if (aliases == NULL) continue;
        for (size_t j = 0; j < json_length(aliases); ++j) {
            Json_Value* alias = json_array_at(aliases, j);
            Json_Value newValue = json_copy(*value);
            json_object_remove(&newValue, "aliases", NULL);
            json_object_set(&newValue, nameKey, json_move(alias));
            DynamicArray_append(newValues, json_move(&newValue));
        }
        json_object_remove(value, "aliases", NULL);
    }
    for (size_t i = 0; i < DynamicArray_length(newValues); ++i) {
        Json_Value* newValue = &DynamicArray_at(newValues, i);
        json_array_append(array, json_move(newValue));
    }
    DynamicArray_free(newValues);
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

    // Flatten out enumerants with aliases
    for (size_t i = 0; i < json_length(operandKinds); ++i) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        Json_Value* enumerants = json_object_get(operandKind, "enumerants");
        json_flatten_aliases(enumerants, "name");
    }

    // Flatten out instructions with aliases
    Json_Value* instructions = json_object_get(&doc.root, "instructions");
    json_flatten_aliases(instructions, "opname");
}

static void generate_c_header_comment(CodeBuilder* cb, Json_Document doc) {
    long long majorVersion = json_as_int(json_object_get(&doc.root, "major_version"));
    long long minorVersion = json_as_int(json_object_get(&doc.root, "minor_version"));
    long long revision = json_as_int(json_object_get(&doc.root, "revision"));
    code_builder_format(cb, ""
        "// This portion is auto-generated from the official SPIR-V grammar JSON.\n"
        "//\n"
        "// SPIR-V Version: %lld.%lld (revision %lld)\n"
        "//\n"
        "// KHRONOS COPYRIGHT NOTICE\n", majorVersion, minorVersion, revision);
    Json_Value* copyright = json_object_get(&doc.root, "copyright");
    for (size_t i = 0; i < json_length(copyright); ++i) {
        Json_Value* line = json_array_at(copyright, i);
        code_builder_format(cb, "// %s\n", json_as_string(line));
    }
    code_builder_putc(cb, '\n');
}

static void generate_c_bitenum_typedef(CodeBuilder* cb, Json_Value* operandKind) {
    char const* name = json_as_string(json_object_get(operandKind, "kind"));
    Json_Value* enumerants = json_object_get(operandKind, "enumerants");
    // Check, if any enumerant has parameters
    bool hasParametricMembers = false;
    for (size_t i = 0; i < json_length(enumerants); ++i) {
        Json_Value* enumerant = json_array_at(enumerants, i);
        Json_Value* parameters = json_object_get(enumerant, "parameters");
        if (parameters == NULL) continue;
        hasParametricMembers = true;
        break;
    }
    if (hasParametricMembers) {
        code_builder_format(cb, "// TODO parametric BitEnum %s\n", name);
    }
    else {
        code_builder_format(cb, "// TODO simple BitEnum %s\n", name);
    }
}

static void generate_c_typedef(CodeBuilder* cb, Json_Value* operandKind) {
    char const* category = json_as_string(json_object_get(operandKind, "category"));
    if (strcmp(category, "BitEnum") == 0) {
        generate_c_bitenum_typedef(cb, operandKind);
        return;
    }
    else {
        char const* name = json_as_string(json_object_get(operandKind, "kind"));
        code_builder_format(cb, "// TODO: %s (category: %s)\n", name, category);
    }
}

static char* generate_c_code(Json_Document doc) {
    CodeBuilder cb = {0};
    generate_c_header_comment(&cb, doc);

    // TODO: Would be nice to sort them by name or something
    Json_Value* operandKinds = json_object_get(&doc.root, "operand_kinds");
    for (size_t i = 0; i < json_length(operandKinds); ++i) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        generate_c_typedef(&cb, operandKind);
    }

    char* result = code_builder_to_cstr(&cb);
    code_builder_free(&cb);
    return result;
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
    char* cCode = generate_c_code(doc);
    printf("%s", cCode);
    json_free_document(&doc);
    return 0;
}
