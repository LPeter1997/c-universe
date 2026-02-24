#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "../src/collections.h"

#define JSON_IMPLEMENTATION
#define JSON_STATIC
#include "../src/json.h"

#define STRING_BUILDER_IMPLEMENTATION
#define STRING_BUILDER_STATIC
#include "../src/string_builder.h"

// Domain model definition

typedef struct Metadata {
    char const* min_version;
    char const* max_version;
    bool provisional;
    DynamicArray(char const*) capabilities;
    DynamicArray(char const*) extensions;
} Metadata;

typedef enum Quantifier {
    QUANTIFIER_ONE,
    QUANTIFIER_OPTIONAL,
    QUANTIFIER_ANY,
} Quantifier;

struct Type;

typedef struct Operand {
    struct Type* type;
    Quantifier quantifier;
    char const* name;
} Operand;

typedef struct Enum {
    char const* doc;
    char const* name;
    bool flags;
    DynamicArray(Operand) parameters;
} Enum;

typedef struct Tuple {
    char const* doc;
    char const* name;
    DynamicArray(struct Type*) members;
} Tuple;

typedef enum TypeKind {
    TYPE_STRONG_ID,
    TYPE_NUMBER,
    TYPE_STRING,
    TYPE_ENUM,
    TYPE_TUPLE,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    union {
        char const* strong_id;
        char const* number;
        Enum enumeration;
        Tuple tuple;
    } value;
} Type;

typedef struct Instruction {
    Metadata metadata;
    char const* name;
    uint32_t opcode;
    DynamicArray(Operand) operands;
} Instruction;

typedef struct Model {
    DynamicArray(char const*) copyright;
    uint32_t magic;
    long long major_version;
    long long minor_version;
    long long revision;
    DynamicArray(Type) types;
    DynamicArray(Instruction) instructions;
} Model;

static void free_metadata(Metadata* metadata) {
    DynamicArray_free(metadata->capabilities);
    DynamicArray_free(metadata->extensions);
}

static void free_type(Type* type) {
    switch (type->kind) {
    case TYPE_ENUM: {
        Enum* enumeration = &type->value.enumeration;
        DynamicArray_free(enumeration->parameters);
        break;
    }
    case TYPE_TUPLE: {
        Tuple* tuple = &type->value.tuple;
        DynamicArray_free(tuple->members);
        break;
    }
    default:
        break;
    }
}

static void free_instruction(Instruction* instruction) {
    free_metadata(&instruction->metadata);
    DynamicArray_free(instruction->operands);
}

static void free_model(Model* model) {
    DynamicArray_free(model->copyright);
    for (size_t i = 0; i < DynamicArray_length(model->types); ++i) {
        Type* type = &DynamicArray_at(model->types, i);
        free_type(type);
    }
    DynamicArray_free(model->types);
    for (size_t i = 0; i < DynamicArray_length(model->instructions); ++i) {
        Instruction* instruction = &DynamicArray_at(model->instructions, i);
        free_instruction(instruction);
    }
    DynamicArray_free(model->instructions);
}

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

static void json_hex_string_to_number(Json_Value* value) {
    if (value == NULL || value->type != JSON_VALUE_STRING) return;
    char const* strValue = json_as_string(value);
    if (strlen(strValue) > 2 && strValue[0] == '0' && strValue[1] == 'x') {
        long long intValue = strtoll(strValue + 2, NULL, 16);
        // Trick to avoid an extra lookup
        Json_Value oldValue = json_move(value);
        *value = json_int(intValue);
        json_free_value(&oldValue);
    }
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

static void json_model_simplification(Json_Document doc) {
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

    // We go through each enumerant of each operand kind and if the value is a string,
    // we assume it's a hex string starting with "0x" and convert it to an integer
    for (size_t i = 0; i < json_length(operandKinds); ++i) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        Json_Value* enumerants = json_object_get(operandKind, "enumerants");
        if (enumerants == NULL) continue;
        for (size_t j = 0; j < json_length(enumerants); ++j) {
            Json_Value* enumerant = json_array_at(enumerants, j);
            Json_Value* value = json_object_get(enumerant, "value");
            json_hex_string_to_number(value);
        }
    }

    // Convert magic number from hex string to number
    Json_Value* magicValue = json_object_get(&doc.root, "magic_number");
    json_hex_string_to_number(magicValue);
}

static Model json_model_to_domain(Json_Document doc) {
    json_model_simplification(doc);

    Model model = {0};

    // Copyright
    Json_Value* copyright = json_object_get(&doc.root, "copyright");
    for (size_t i = 0; i < json_length(copyright); ++i) {
        Json_Value* line = json_array_at(copyright, i);
        DynamicArray_append(model.copyright, json_as_string(line));
    }

    // Magic number and version
    model.magic = (uint32_t)json_as_int(json_object_get(&doc.root, "magic_number"));
    model.major_version = json_as_int(json_object_get(&doc.root, "major_version"));
    model.minor_version = json_as_int(json_object_get(&doc.root, "minor_version"));
    model.revision = json_as_int(json_object_get(&doc.root, "revision"));

    return model;
}

static void generate_c_header_comment(CodeBuilder* cb, Model* model) {
    code_builder_format(cb, ""
        "// This portion is auto-generated from the official SPIR-V grammar JSON.\n"
        "//\n"
        "// SPIR-V Version: %lld.%lld (revision %lld)\n"
        "//\n"
        "// KHRONOS COPYRIGHT NOTICE\n", model->major_version, model->minor_version, model->revision);
    for (size_t i = 0; i < DynamicArray_length(model->copyright); ++i) {
        char const* line = DynamicArray_at(model->copyright, i);
        code_builder_format(cb, "// %s\n", line);
    }
    code_builder_putc(cb, '\n');
}

static char* generate_c_code(Model* model) {
    CodeBuilder cb = {0};
    generate_c_header_comment(&cb, model);

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
    Model model = json_model_to_domain(doc);
    char* cCode = generate_c_code(&model);
    printf("%s", cCode);
    json_free_document(&doc);
    return 0;
}
