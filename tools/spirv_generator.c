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

// Domain model definition /////////////////////////////////////////////////////

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

typedef struct Operand {
    char const* typeName;
    Quantifier quantifier;
    char* name;
} Operand;

typedef struct Enumerant {
    Metadata metadata;
    char const* name;
    char const* doc;
    long long value;
    DynamicArray(Operand) parameters;
    char const* alias_of;
} Enumerant;

typedef struct Enum {
    bool flags;
    DynamicArray(Enumerant) enumerants;
} Enum;

typedef struct Tuple {
    DynamicArray(char const*) memberTypeNames;
} Tuple;

typedef enum TypeKind {
    TYPE_STRONG_ID,
    TYPE_UINT32,
    TYPE_INT32,
    TYPE_FLOAT32,
    TYPE_STRING,
    TYPE_ENUM,
    TYPE_TUPLE,
} TypeKind;

typedef struct Type {
    TypeKind kind;
    char const* doc;
    char const* name;
    union {
        char const* type_name; // For strong IDs and primitives
        Enum enumeration;
        Tuple tuple;
    } value;
} Type;

typedef struct Instruction {
    Metadata metadata;
    char const* name;
    uint32_t opcode;
    DynamicArray(Operand) operands;
    char const* alias_of;
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

static void free_operand(Operand* operand) {
    free(operand->name);
}

static void free_enumerant(Enumerant* enumerant) {
    free_metadata(&enumerant->metadata);
    for (size_t i = 0; i < DynamicArray_length(enumerant->parameters); ++i) {
        Operand* operand = &DynamicArray_at(enumerant->parameters, i);
        free_operand(operand);
    }
    DynamicArray_free(enumerant->parameters);
}

static void free_type(Type* type) {
    switch (type->kind) {
    case TYPE_ENUM: {
        Enum* enumeration = &type->value.enumeration;
        for (size_t i = 0; i < DynamicArray_length(enumeration->enumerants); ++i) {
            Enumerant* enumerant = &DynamicArray_at(enumeration->enumerants, i);
            free_enumerant(enumerant);
        }
        DynamicArray_free(enumeration->enumerants);
        break;
    }
    case TYPE_TUPLE: {
        Tuple* tuple = &type->value.tuple;
        DynamicArray_free(tuple->memberTypeNames);
        break;
    }
    default:
        break;
    }
}

static void free_instruction(Instruction* instruction) {
    free_metadata(&instruction->metadata);
    for (size_t i = 0; i < DynamicArray_length(instruction->operands); ++i) {
        Operand* operand = &DynamicArray_at(instruction->operands, i);
        free_operand(operand);
    }
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

// JSON document manipulation //////////////////////////////////////////////////

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
        Json_Value* valueName = json_object_get(value, nameKey);
        if (aliases == NULL) continue;
        for (size_t j = 0; j < json_length(aliases); ++j) {
            Json_Value* alias = json_array_at(aliases, j);
            Json_Value newValue = json_copy(value);
            json_object_remove(&newValue, "aliases", NULL);
            json_object_set(&newValue, nameKey, json_move(alias));
            json_object_set(&newValue, "alias_of", json_copy(valueName));
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

// Check if a kind name is already in the sorted array
static bool kind_is_sorted(Json_Value* sortedArray, char const* kindName) {
    for (size_t i = 0; i < json_length(sortedArray); ++i) {
        Json_Value* item = json_array_at(sortedArray, i);
        char const* name = json_as_string(json_object_get(item, "kind"));
        if (strcmp(name, kindName) == 0) return true;
    }
    return false;
}

// Check if all dependencies of an operand kind are already sorted
static bool dependencies_satisfied(Json_Value* operandKind, Json_Value* sortedArray) {
    // Collect dependencies inline to avoid DynamicArray pointer issues
    DynamicArray(char const*) deps = {0};

    // Composite kinds reference other kinds via "bases"
    Json_Value* bases = json_object_get(operandKind, "bases");
    if (bases != NULL) {
        for (size_t i = 0; i < json_length(bases); ++i) {
            DynamicArray_append(deps, json_as_string(json_array_at(bases, i)));
        }
    }
    // Enums can have parameters on enumerants that reference other kinds
    Json_Value* enumerants = json_object_get(operandKind, "enumerants");
    if (enumerants != NULL) {
        for (size_t i = 0; i < json_length(enumerants); ++i) {
            Json_Value* enumerant = json_array_at(enumerants, i);
            Json_Value* parameters = json_object_get(enumerant, "parameters");
            if (parameters == NULL) continue;
            for (size_t j = 0; j < json_length(parameters); ++j) {
                Json_Value* param = json_array_at(parameters, j);
                Json_Value* kindVal = json_object_get(param, "kind");
                if (kindVal != NULL) {
                    DynamicArray_append(deps, json_as_string(kindVal));
                }
            }
        }
    }

    bool satisfied = true;
    for (size_t i = 0; i < DynamicArray_length(deps); ++i) {
        if (!kind_is_sorted(sortedArray, DynamicArray_at(deps, i))) {
            satisfied = false;
            break;
        }
    }
    DynamicArray_free(deps);
    return satisfied;
}

// Topological sort of operand_kinds (simple O(n^2) approach)
static void json_topological_sort_operand_kinds(Json_Value* operandKinds) {
    Json_Value sorted = json_array();
    size_t remaining = json_length(operandKinds);

    while (remaining > 0) {
        bool progress = false;
        for (size_t i = 0; i < json_length(operandKinds); ++i) {
            Json_Value* item = json_array_at(operandKinds, i);
            if (item->type == JSON_VALUE_NULL) continue;  // Already moved

            char const* kindName = json_as_string(json_object_get(item, "kind"));
            if (kind_is_sorted(&sorted, kindName)) continue;

            if (dependencies_satisfied(item, &sorted)) {
                json_array_append(&sorted, json_copy(item));
                // Mark as moved by replacing with null
                Json_Value old = json_move(item);
                *item = json_null();
                json_free_value(&old);
                remaining--;
                progress = true;
            }
        }
        if (!progress && remaining > 0) {
            fprintf(stderr, "Warning: circular dependency detected in operand_kinds\n");
            // Append remaining items as-is to avoid infinite loop
            for (size_t i = 0; i < json_length(operandKinds); ++i) {
                Json_Value* item = json_array_at(operandKinds, i);
                if (item->type != JSON_VALUE_NULL) {
                    json_array_append(&sorted, json_copy(item));
                }
            }
            break;
        }
    }

    // Replace original array contents with sorted
    Json_Value old = json_move(operandKinds);
    *operandKinds = json_move(&sorted);
    json_free_value(&old);
}

static void json_model_simplification(Json_Document doc) {
    // Reorder operand_kinds to a topological order
    // This is so we adhere to C definition order
    Json_Value* operandKinds = json_object_get(&doc.root, "operand_kinds");
    json_topological_sort_operand_kinds(operandKinds);

    // Flatten out enumerants with aliases
    for (size_t i = 0; i < json_length(operandKinds); ++i) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        Json_Value* enumerants = json_object_get(operandKind, "enumerants");
        json_flatten_aliases(enumerants, "enumerant");
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

// Domain conversion ///////////////////////////////////////////////////////////

static Type* find_type_by_name(Model* model, char const* name) {
    // TODO: Linear lookup, but the API of our HashTable is kinda bad, so we'll stick to array for now
    for (size_t i = 0; i < DynamicArray_length(model->types); ++i) {
        Type* candidate = &DynamicArray_at(model->types, i);
        if (candidate->name != NULL && strcmp(candidate->name, name) == 0) {
            return candidate;
        }
    }
    fprintf(stderr, "Unknown type %s\n", name);
    assert(false);
    return NULL;
}

static Metadata json_metadata_to_domain(Json_Value* value) {
    Metadata metadata = {0};
    Json_Value* minVersion = json_object_get(value, "version");
    if (minVersion != NULL) metadata.min_version = json_as_string(minVersion);
    Json_Value* maxVersion = json_object_get(value, "lastVersion");
    if (maxVersion != NULL) metadata.max_version = json_as_string(maxVersion);
    Json_Value* provisional = json_object_get(value, "provisional");
    if (provisional != NULL) metadata.provisional = json_as_bool(provisional);
    Json_Value* capabilities = json_object_get(value, "capabilities");
    for (size_t i = 0; capabilities != NULL && i < json_length(capabilities); ++i) {
        Json_Value* capability = json_array_at(capabilities, i);
        DynamicArray_append(metadata.capabilities, json_as_string(capability));
    }
    Json_Value* extensions = json_object_get(value, "extensions");
    for (size_t i = 0; extensions != NULL && i < json_length(extensions); ++i) {
        Json_Value* extension = json_array_at(extensions, i);
        DynamicArray_append(metadata.extensions, json_as_string(extension));
    }
    return metadata;
}

static char* operand_infer_name(Operand* operand, char const* hint) {
    StringBuilder sb = {0};
    if (hint == NULL) hint = operand->typeName;
    sb_puts(&sb, hint);
    sb_replace(&sb, " ", "");
    // TODO: Needs more processing
    char* result = sb_to_cstr(&sb);
    sb_free(&sb);
    return result;
}

static Operand json_operand_to_domain(Json_Value* operand) {
    // NOTE: Name is optional
    Json_Value* name = json_object_get(operand, "name");
    char const* kind = json_as_string(json_object_get(operand, "kind"));
    Json_Value* quantifierValue = json_object_get(operand, "quantifier");
    Quantifier quantifier = QUANTIFIER_ONE;
    if (quantifierValue != NULL) {
        char const* quantifierStr = json_as_string(quantifierValue);
        if (strcmp(quantifierStr, "?") == 0) quantifier = QUANTIFIER_OPTIONAL;
        else if (strcmp(quantifierStr, "*") == 0) quantifier = QUANTIFIER_ANY;
    }
    Operand result = {
        .name = NULL,
        .quantifier = quantifier,
        .typeName = kind,
    };
    result.name = operand_infer_name(&result, name == NULL ? NULL : json_as_string(name));
    return result;
}

static Enumerant json_enumerant_to_domain(Json_Value* enumerant) {
    Json_Value* doc = json_object_get(enumerant, "doc");
    char const* name = json_as_string(json_object_get(enumerant, "enumerant"));
    long long value = json_as_int(json_object_get(enumerant, "value"));
    Json_Value* aliasOf = json_object_get(enumerant, "alias_of");
    Metadata metadata = json_metadata_to_domain(enumerant);
    Enumerant result = {
        .name = name,
        .metadata = metadata,
        .value = value,
        .doc = doc == NULL ? NULL : json_as_string(doc),
        .parameters = {0},
        .alias_of = aliasOf == NULL ? NULL : json_as_string(aliasOf),
    };
    Json_Value* parameters = json_object_get(enumerant, "parameters");
    for (size_t i = 0; parameters != NULL && i < json_length(parameters); ++i) {
        Json_Value* parameter = json_array_at(parameters, i);
        Operand operand = json_operand_to_domain(parameter);
        DynamicArray_append(result.parameters, operand);
    }
    return result;
}

static Enum json_enum_to_domain(Json_Value* operandKind) {
    Json_Value* doc = json_object_get(operandKind, "doc");
    bool isFlags = strcmp(json_as_string(json_object_get(operandKind, "category")), "BitEnum") == 0;
    Enum result = {
        .flags = isFlags,
        .enumerants = {0},
    };
    Json_Value* enumerants = json_object_get(operandKind, "enumerants");
    for (size_t i = 0; i < json_length(enumerants); ++i) {
        Json_Value* enumerant = json_array_at(enumerants, i);
        Enumerant enumerantStruct = json_enumerant_to_domain(enumerant);
        DynamicArray_append(result.enumerants, enumerantStruct);
    }
    return result;
}

static Tuple json_tuple_to_domain(Json_Value* operandKind) {
    Tuple result = {0};
    Json_Value* members = json_object_get(operandKind, "bases");
    for (size_t i = 0; i < json_length(members); ++i) {
        char const* memberKind = json_as_string(json_array_at(members, i));
        DynamicArray_append(result.memberTypeNames, memberKind);
    }
    return result;
}

static Type json_operand_kind_to_domain(Json_Value* operandKind) {
    char const* category = json_as_string(json_object_get(operandKind, "category"));
    char const* name = json_as_string(json_object_get(operandKind, "kind"));
    Json_Value* docJson = json_object_get(operandKind, "doc");
    char const* doc = docJson == NULL ? NULL : json_as_string(docJson);
    if (strcmp(category, "BitEnum") == 0 || strcmp(category, "ValueEnum") == 0) {
        Enum enumeration = json_enum_to_domain(operandKind);
        return (Type){
            .kind = TYPE_ENUM,
            .name = name,
            .doc = doc,
            .value.enumeration = enumeration,
        };
    }
    else if (strcmp(category, "Id") == 0) {
        // Strongly-typed ID
        return (Type){
            .kind = TYPE_STRONG_ID,
            .name = name,
            .doc = doc,
            .value.type_name = "uint32_t",
        };
    }
    else if (strcmp(category, "Literal") == 0) {
        TypeKind kind = TYPE_UINT32;
        char const* typeName = "uint32_t";
        if (strcmp(name, "LiteralString") == 0) {
            kind = TYPE_STRING;
            typeName = "char const*";
        }
        else if (strcmp(name, "LiteralFloat") == 0) {
            kind = TYPE_FLOAT32;
            typeName = "float";
        }
        else if (strcmp(name, "LiteralInteger") == 0) {
            kind = TYPE_INT32;
            typeName = "int32_t";
        }
        return (Type){
            .kind = kind,
            .name = name,
            .doc = doc,
            .value.type_name = typeName,
        };
    }
    else if (strcmp(category, "Composite") == 0) {
        Tuple tuple = json_tuple_to_domain(operandKind);
        return (Type){
            .kind = TYPE_TUPLE,
            .name = name,
            .doc = doc,
            .value.tuple = tuple,
        };
    }
    else {
        printf("TODO %s\n", category);
        return (Type){0};
    }
}

static Instruction json_instruction_to_domain(Json_Value* instruction) {
    char const* name = json_as_string(json_object_get(instruction, "opname"));
    uint32_t opcode = (uint32_t)json_as_int(json_object_get(instruction, "opcode"));
    Metadata metadata = json_metadata_to_domain(instruction);
    Json_Value* aliasOf = json_object_get(instruction, "alias_of");
    Instruction result = {
        .name = name,
        .opcode = opcode,
        .alias_of = aliasOf == NULL ? NULL : json_as_string(aliasOf),
        .metadata = metadata,
        .operands = {0},
    };
    Json_Value* operands = json_object_get(instruction, "operands");
    for (size_t i = 0; operands != NULL && i < json_length(operands); ++i) {
        Json_Value* operand = json_array_at(operands, i);
        Operand operandStruct = json_operand_to_domain(operand);
        DynamicArray_append(result.operands, operandStruct);
    }
    return result;
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

    // Types
    Json_Value* operandKinds = json_object_get(&doc.root, "operand_kinds");
    for (size_t i = 0; i < json_length(operandKinds); ++i) {
        Json_Value* operandKind = json_array_at(operandKinds, i);
        Type type = json_operand_kind_to_domain(operandKind);
        DynamicArray_append(model.types, type);
    }

    // Instructions
    Json_Value* instructions = json_object_get(&doc.root, "instructions");
    for (size_t i = 0; i < json_length(instructions); ++i) {
        Json_Value* instruction = json_array_at(instructions, i);
        Instruction instr = json_instruction_to_domain(instruction);
        DynamicArray_append(model.instructions, instr);
    }

    return model;
}

// C code generation ///////////////////////////////////////////////////////////

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

static void generate_c_doc(CodeBuilder* cb, char const* doc) {
    if (doc == NULL) return;
    code_builder_format(cb, ""
        "/**\n"
        " * %s\n"
        " */\n", doc);
}

static void generate_c_type(CodeBuilder* cb, Type* type) {
    generate_c_doc(cb, type->doc);
    if (type->kind == TYPE_STRONG_ID
     || type->kind == TYPE_UINT32
     || type->kind == TYPE_INT32
     || type->kind == TYPE_FLOAT32
     || type->kind == TYPE_STRING) {
        code_builder_format(cb, "typedef %s Spv_%s;\n\n", type->value.type_name, type->name);
    }
    else if (type->kind == TYPE_ENUM) {
        Enum* enumeration = &type->value.enumeration;
        char const* enumSuffix = enumeration->flags ? "Flags" : "Tag";
        char const* tagName = enumeration->flags ? "flags" : "tag";

        code_builder_format(cb, "typedef enum Spv_%s%s {\n", type->name, enumSuffix);
        code_builder_indent(cb);
        for (size_t i = 0; i < DynamicArray_length(enumeration->enumerants); ++i) {
            Enumerant* enumerant = &DynamicArray_at(enumeration->enumerants, i);
            generate_c_doc(cb, enumerant->doc);
            code_builder_format(cb, "Spv_%s_%s = %lld,\n", type->name, enumerant->name, enumerant->value);
        }
        code_builder_dedent(cb);
        code_builder_format(cb, "} Spv_%s%s;\n\n", type->name, enumSuffix);

        // TODO: Would be nice to have a general search algo
        bool hasParameters = false;
        for (size_t i = 0; i < DynamicArray_length(enumeration->enumerants); ++i) {
            Enumerant* enumerant = &DynamicArray_at(enumeration->enumerants, i);
            if (enumerant->parameters.length > 0) {
                hasParameters = true;
                break;
            }
        }

        // Define the struct describing the operand
        code_builder_format(cb, "typedef struct Spv_%s {\n", type->name);
        code_builder_indent(cb);
        code_builder_format(cb, "Spv_%s%s %s;\n", type->name, enumSuffix, tagName);
        if (hasParameters && !enumeration->flags) {
            code_builder_puts(cb, "union {\n");
            code_builder_indent(cb);
        }
        for (size_t i = 0; i < DynamicArray_length(enumeration->enumerants); ++i) {
            Enumerant* enumerant = &DynamicArray_at(enumeration->enumerants, i);
            if (enumerant->alias_of != NULL) continue;
            if (enumerant->parameters.length == 0) continue;
            code_builder_format(cb, "struct {\n");
            code_builder_indent(cb);
            for (size_t j = 0; j < DynamicArray_length(enumerant->parameters); ++j) {
                Operand* param = &DynamicArray_at(enumerant->parameters, j);
                if (param->quantifier == QUANTIFIER_ONE) {
                    code_builder_format(cb, "Spv_%s %s;\n", param->typeName, param->name);
                }
                else if (param->quantifier == QUANTIFIER_ANY) {
                    code_builder_format(cb, "Spv_%s* %s;\n", param->typeName, param->name);
                    code_builder_format(cb, "size_t %sCount;\n", param->name);
                }
                else {
                    code_builder_format(cb, "// TODO: handle quantifier %d\n", param->quantifier);
                }
            }
            code_builder_dedent(cb);
            code_builder_format(cb, "} %s;\n", enumerant->name);
        }
        if (hasParameters && !enumeration->flags) {
            code_builder_dedent(cb);
            code_builder_format(cb, "} variants;\n\n", type->name);
        }
        code_builder_dedent(cb);
        code_builder_format(cb, "} Spv_%s;\n\n", type->name);

        for (size_t i = 0; i < DynamicArray_length(enumeration->enumerants); ++i) {
            Enumerant* enumerant = &DynamicArray_at(enumeration->enumerants, i);
            char const* originalMember = enumerant->alias_of == NULL ? enumerant->name : enumerant->alias_of;
            if (!enumeration->flags && enumerant->parameters.length == 0) {
                // Value-enum element without parameters, we generate a constant for it
                code_builder_format(cb, "static inline const Spv_%s spv_%s_%s = { .%s = Spv_%s_%s };\n", type->name, type->name, enumerant->name, tagName, type->name, originalMember);
            }
            else if (!enumeration->flags) {
                // Value-enum element with parameters, we generate a function that can create the struct with the parameters
                code_builder_format(cb, "static inline Spv_%s spv_%s_%s(", type->name, type->name, enumerant->name);
                for (size_t j = 0; j < DynamicArray_length(enumerant->parameters); ++j) {
                    Operand* param = &DynamicArray_at(enumerant->parameters, j);
                    if (param->quantifier == QUANTIFIER_ONE) {
                        code_builder_format(cb, "Spv_%s %s", param->typeName, param->name);
                    }
                    else if (param->quantifier == QUANTIFIER_ANY) {
                        code_builder_format(cb, "Spv_%s* %s, size_t %sCount", param->typeName, param->name, param->name);
                    }
                    else {
                        code_builder_format(cb, "// TODO: handle quantifier %d\n", param->quantifier);
                    }
                    if (j < DynamicArray_length(enumerant->parameters) - 1) {
                        code_builder_puts(cb, ", ");
                    }
                }
                code_builder_puts(cb, ") {\n");
                code_builder_indent(cb);
                code_builder_format(cb, "return (Spv_%s){\n", type->name);
                code_builder_indent(cb);
                code_builder_format(cb, ".%s = Spv_%s_%s,\n", tagName, type->name, originalMember);
                for (size_t j = 0; j < DynamicArray_length(enumerant->parameters); ++j) {
                    Operand* param = &DynamicArray_at(enumerant->parameters, j);
                    if (param->quantifier == QUANTIFIER_ONE) {
                        code_builder_format(cb, ".variants.%s.%s = %s;\n", originalMember, param->name, param->name);
                    }
                    else if (param->quantifier == QUANTIFIER_ANY) {
                        code_builder_format(cb, ".variants.%s.%s = %s;\n", originalMember, param->name, param->name);
                        code_builder_format(cb, ".variants.%s.%sCount = %sCount;\n", originalMember, param->name, param->name);
                    }
                    else {
                        code_builder_format(cb, "// TODO: handle quantifier %d\n", param->quantifier);
                    }
                }
                code_builder_dedent(cb);
                code_builder_format(cb, "};\n");
                code_builder_dedent(cb);
                code_builder_puts(cb, "}\n");
            }
            else if (enumerant->value != 0) {
                // If it's a flags enum, we generate a function that takes in a pointer to the struct and sets the flag and parameters on it
                code_builder_format(cb, "static inline void spv_%s_set_%s(Spv_%s* operand", type->name, enumerant->name, type->name);
                for (size_t j = 0; j < DynamicArray_length(enumerant->parameters); ++j) {
                    Operand* param = &DynamicArray_at(enumerant->parameters, j);
                    if (param->quantifier == QUANTIFIER_ONE) {
                        code_builder_format(cb, ", Spv_%s %s", param->typeName, param->name);
                    }
                    else if (param->quantifier == QUANTIFIER_ANY) {
                        code_builder_format(cb, ", Spv_%s* %s, size_t %sCount", param->typeName, param->name, param->name);
                    }
                    else {
                        code_builder_format(cb, ", // TODO: handle quantifier %d for parameter %s\n", param->quantifier, param->name);
                    }
                }
                code_builder_puts(cb, ") {\n");
                code_builder_indent(cb);
                code_builder_format(cb, "operand->%s |= Spv_%s_%s;\n", tagName, type->name, originalMember);
                for (size_t j = 0; j < DynamicArray_length(enumerant->parameters); ++j) {
                    Operand* param = &DynamicArray_at(enumerant->parameters, j);
                    if (param->quantifier == QUANTIFIER_ONE) {
                        code_builder_format(cb, "operand->%s.%s = %s;\n", originalMember, param->name, param->name);
                    }
                    else if (param->quantifier == QUANTIFIER_ANY) {
                        code_builder_format(cb, "operand->%s.%s = %s;\n", originalMember, param->name, param->name);
                        code_builder_format(cb, "operand->%s.%sCount = %sCount;\n", originalMember, param->name, param->name);
                    }
                    else {
                        code_builder_format(cb, "// TODO: handle quantifier %d\n", param->quantifier);
                    }
                }
                code_builder_dedent(cb);
                code_builder_puts(cb, "}\n");
            }
            else {
                // We expect 0 to be a special case, define a constant for it
                assert(DynamicArray_length(enumerant->parameters) == 0);
                code_builder_format(cb, "static inline const Spv_%s spv_%s_%s = { .%s = 0 };\n", type->name, type->name, enumerant->name, tagName);
            }
        }
    }
    else if (type->kind == TYPE_TUPLE) {
        Tuple* tuple = &type->value.tuple;
        code_builder_format(cb, "typedef struct Spv_%s {\n", type->name);
        code_builder_indent(cb);
        for (size_t i = 0; i < DynamicArray_length(tuple->memberTypeNames); ++i) {
            char const* memberTypeName = DynamicArray_at(tuple->memberTypeNames, i);
            code_builder_format(cb, "Spv_%s member%d;\n", memberTypeName, (int)i + 1);
        }
        code_builder_dedent(cb);
        code_builder_format(cb, "} Spv_%s;\n\n", type->name);
    }
    else {
        code_builder_format(cb, "// TODO: %s\n", type->name);
    }
}

static void generate_c_operand_encoder(CodeBuilder* cb, Type* operandType, char const* name) {
    if (operandType->kind == TYPE_STRONG_ID
     || operandType->kind == TYPE_UINT32) {
        code_builder_format(cb, "spv_encode_u32(encoder, %s);\n", name);
    }
    else if (operandType->kind == TYPE_INT32) {
        code_builder_format(cb, "spv_encode_i32(encoder, %s);\n", name);
    }
    else if (operandType->kind == TYPE_FLOAT32) {
        code_builder_format(cb, "spv_encode_f32(encoder, %s);\n", name);
    }
    else if (operandType->kind == TYPE_STRING) {
        code_builder_format(cb, "spv_encode_string(encoder, %s);\n", name);
    }
    else if (operandType->kind == TYPE_ENUM) {
        Enum* enumeration = &operandType->value.enumeration;
        // First we need to write the tag/flag
        code_builder_format(cb, "spv_encode_u32(encoder, %s.%s);\n", name, enumeration->flags ? "flags" : "tag");

        bool hasParameters = false;
        for (size_t i = 0; i < DynamicArray_length(enumeration->enumerants); ++i) {
            Enumerant* enumerant = &DynamicArray_at(enumeration->enumerants, i);
            if (enumerant->parameters.length > 0) {
                hasParameters = true;
                break;
            }
        }
        if (hasParameters) {
            code_builder_format(cb, "// TODO handle parameters for enum %s\n", operandType->name);
        }
    }
    else {
        code_builder_format(cb, "// TODO: encode %s of type %s\n", name, operandType->name);
    }
}

static void generate_c_instruction_encoder(CodeBuilder* cb, Model* model, Instruction* instruction) {
    code_builder_format(cb, "static inline void spv_%s(Spv_InstructionEncoder* encoder", instruction->name);
    for (size_t i = 0; i < DynamicArray_length(instruction->operands); ++i) {
        Operand* operand = &DynamicArray_at(instruction->operands, i);
        if (operand->quantifier == QUANTIFIER_ONE) {
            code_builder_format(cb, ", Spv_%s %s", operand->typeName, operand->name);
        }
        else if (operand->quantifier == QUANTIFIER_ANY) {
            code_builder_format(cb, ", Spv_%s* %s, size_t %sCount", operand->typeName, operand->name, operand->name);
        }
        else if (operand->quantifier == QUANTIFIER_OPTIONAL) {
            code_builder_format(cb, ", struct { bool present; Spv_%s value; } %s", operand->typeName, operand->name);
        }
        else {
            code_builder_format(cb, ", // TODO: handle quantifier %d for operand %s\n", operand->quantifier, operand->name);
        }
    }
    code_builder_puts(cb, ") {\n");
    code_builder_indent(cb);
    // Save encoder offset
    code_builder_puts(cb, "size_t startOffset = encoder->offset;\n");
    // We skip one word for the instruction header which is (wordCount << 16) | opcode
    // We will need to come back to this to patch it at the end, when we actually know the number of words
    code_builder_puts(cb, "spv_encode_u32(encoder, 0);\n");
    for (size_t i = 0; i < DynamicArray_length(instruction->operands); ++i) {
        Operand* operand = &DynamicArray_at(instruction->operands, i);
        Type* operandType = find_type_by_name(model, operand->typeName);
        if (operand->quantifier == QUANTIFIER_ONE) {
            generate_c_operand_encoder(cb, operandType, operand->name);
        }
        else if (operand->quantifier == QUANTIFIER_ANY) {
            code_builder_format(cb, "for (size_t i = 0; i < %sCount; ++i) {\n", operand->name);
            code_builder_indent(cb);
            // Construct the name as an accessor to the current element, e.g. "myOperand[i]"
            char elementName[256];
            snprintf(elementName, sizeof(elementName), "%s[i]", operand->name);
            generate_c_operand_encoder(cb, operandType, elementName);
            code_builder_dedent(cb);
            code_builder_puts(cb, "}\n");
        }
        else if (operand->quantifier == QUANTIFIER_OPTIONAL) {
            // Only encode if present
            code_builder_format(cb, "if (%s.present) {\n", operand->name);
            code_builder_indent(cb);
            // Construct the name as an accessor to the value, e.g. "myOperand.value"
            char valueName[256];
            snprintf(valueName, sizeof(valueName), "%s.value", operand->name);
            generate_c_operand_encoder(cb, operandType, valueName);
            code_builder_dedent(cb);
            code_builder_puts(cb, "}\n");
        }
        else {
            code_builder_format(cb, "// TODO: handle quantifier %d for operand %s\n", operand->quantifier, operand->name);
        }
    }
    // Patch instruction header with actual word count and opcode
    code_builder_format(cb, "size_t endOffset = encoder->builder.offset;\n");
    code_builder_format(cb, "size_t wordCount = (endOffset - startOffset);\n");
    code_builder_format(cb, "encoder->builder.words[startOffset] = (wordCount << 16) | %u;\n", instruction->opcode);
    code_builder_dedent(cb);
    code_builder_puts(cb, "}\n\n");
}

static char* generate_c_code(Model* model) {
    CodeBuilder cb = {0};
    generate_c_header_comment(&cb, model);

    for (size_t i = 0; i < DynamicArray_length(model->types); ++i) {
        Type* type = &DynamicArray_at(model->types, i);
        generate_c_type(&cb, type);
    }

    for (size_t i = 0; i < DynamicArray_length(model->instructions); ++i) {
        Instruction* instruction = &DynamicArray_at(model->instructions, i);
        generate_c_instruction_encoder(&cb, model, instruction);
    }

    char* result = code_builder_to_cstr(&cb);
    code_builder_free(&cb);
    return result;
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
    free_model(&model);
    printf("%s", cCode);
    json_free_document(&doc);
    return 0;
}
