/**
 * argparse.h is a single-header library for parsing command-line arguments in C.
 *
 * The goal was to get something relatively extensible and similar to System.CommandLine in .NET.
 *
 * Configuration:
 *  - TODO
 *
 * API:
 *  - TODO
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef ARGPARSE_STATIC
#   define ARGPARSE_DEF static
#else
#   define ARGPARSE_DEF extern
#endif

#ifndef ARGPARSE_REALLOC
#   define ARGPARSE_REALLOC realloc
#endif
#ifndef ARGPARSE_FREE
#   define ARGPARSE_FREE free
#endif

#ifndef ARGPARSE_VALUE_SEPARATORS
#   define ARGPARSE_VALUE_SEPARATORS "=", ":"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OptionParseResult {
    // Owned pointer to the parsed value, which must be freed by the caller in case of a parsing success
    void* value;
    // Owned error message in case of a parsing failure, must be freed by the caller
    // A non-NULL value indicates a parsing failure, while a NULL value indicates a parsing success
    char const* error;
} OptionParseResult;

typedef OptionParseResult OptionParseFn(char const* text, size_t length);

typedef struct OptionDescription {
    char const* long_name;
    char const* short_name;
    char const* description;
    bool is_required;
    bool takes_value;
    void* default_value;
    bool allow_multiple;
    OptionParseFn* parse_fn;
} OptionDescription;

typedef struct CommandDescription {
    char const* name;
    char const* description;

    OptionDescription* options;
    size_t options_length;
    size_t options_capacity;

    struct CommandDescription* subcommands;
    size_t subcommands_length;
    size_t subcommands_capacity;
} CommandDescription;

typedef struct Option {
    OptionDescription description;
    void* value;
} Option;

typedef struct ArgumentPack {
    char const* program_name;

    CommandDescription* command;
    // Owned array of options that were parsed for the command
    Option* options;
    size_t options_length;
    size_t options_capacity;

    // Error message in case of a parsing failure
    // The memory is owned by this struct and must be freed
    char const* error;
} ArgumentPack;

ARGPARSE_DEF ArgumentPack argparse_parse(int argc, char** argv, CommandDescription* root_command);
ARGPARSE_DEF void argparse_free(ArgumentPack* pack);

ARGPARSE_DEF void argparse_add_option(CommandDescription* command, OptionDescription optionDesc);
ARGPARSE_DEF void argparse_add_subcommand(CommandDescription* command, CommandDescription subcommand);
ARGPARSE_DEF void argparse_free_command(CommandDescription* command);

ARGPARSE_DEF bool argparse_has_option(ArgumentPack* pack, OptionDescription* optionDesc);
ARGPARSE_DEF void* argparse_get_value(ArgumentPack* pack, OptionDescription* optionDesc);

#ifdef __cplusplus
}
#endif

#endif /* ARGPARSE_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_IMPLEMENTATION

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARGPARSE_ASSERT(condition, message) assert(((void)message, condition))

/**
 * Duplicates a string, moving the copy to heap-memory.
 * Used for errors that we want to return in owned memory.
 * @param str The string to duplicate.
 * @return A pointer to the duplicated string, which is owned by the caller and must be freed.
 */
static char const* __argparse_strdup(char const* str) {
    size_t length = strlen(str);
    char* copy = (char*)ARGPARSE_REALLOC(NULL, length + 1);
    ARGPARSE_ASSERT(copy != NULL, "failed to allocate memory for string copy");
    strcpy(copy, str);
    return copy;
}

/**
 * Formats a string using printf-style formatting, moving the result to heap-memory.
 * Used for errors that we want to return in owned memory.
 * @param format The printf-style format string.
 * @param ... The arguments for the format string.
 * @return A pointer to the formatted string, which is owned by the caller and must be freed.
 */
static char const* __argparse_snprintf(char const* format, ...) {
    va_list args;
    va_start(args, format);
    size_t length = (size_t)vsnprintf(NULL, 0, format, args);
    va_end(args);

    char* result = (char*)ARGPARSE_REALLOC(NULL, length + 1);
    ARGPARSE_ASSERT(result != NULL, "failed to allocate memory for formatted string");

    va_start(args, format);
    vsnprintf(result, length + 1, format, args);
    va_end(args);

    return result;
}

static char const* __argparse_infer_option_name(OptionDescription* optionDesc) {
    if (optionDesc->long_name != NULL) return optionDesc->long_name;
    if (optionDesc->short_name != NULL) return optionDesc->short_name;
    return "<unnamed option>";
}

static bool __argparse_options_eq(OptionDescription* a, OptionDescription* b) {
    return (a->long_name != NULL && a->long_name == b->long_name)
        || (a->short_name != NULL && a->short_name == b->short_name);
}

static void __argparse_pack_ensure_option_capacity(ArgumentPack* pack, size_t newCapacity) {
    if (newCapacity <= pack->options_capacity) return;

    size_t capacity = (pack->options_capacity == 0) ? 8 : pack->options_capacity;
    while (capacity < newCapacity) capacity *= 2;

    Option* newOptions = (Option*)ARGPARSE_REALLOC(pack->options, sizeof(Option) * capacity);
    ARGPARSE_ASSERT(newOptions != NULL, "failed to allocate memory for options in argument pack");
    pack->options = newOptions;
    pack->options_capacity = capacity;
}

static void __argparse_add_option_to_pack(ArgumentPack* pack, OptionDescription* optionDesc, void* value) {
    __argparse_pack_ensure_option_capacity(pack, pack->options_length + 1);
    pack->options[pack->options_length++] = (Option){
        .description = *optionDesc,
        .value = value,
    };
}

static void* __argparse_parse_option_value(ArgumentPack* pack, OptionDescription* optionDesc, char const* valueText, size_t valueTextLength) {
    OptionParseResult parseResult = optionDesc->parse_fn(valueText, valueTextLength);
    if (parseResult.error != NULL) {
        char const* optionName = __argparse_infer_option_name(optionDesc);
        pack->error = __argparse_snprintf("failed to parse value for option '%s': %s", optionName, parseResult.error);
        ARGPARSE_FREE((void*)parseResult.error);
        return NULL;
    }
    return parseResult.value;
}

static bool __argparse_match_option_to_argument(ArgumentPack* pack, OptionDescription* optionDesc, int argc, char** argv, int* argIndex) {
    // NOTE: this function is guaranteed to be called with argIndex < argc, so argv[*argIndex] is always valid
    char const* argPart = argv[*argIndex];
    // Try to match either fully or partially with a separator following with a value, if the option takes a value
    bool fullMatch = (optionDesc->long_name != NULL && strcmp(argPart, optionDesc->long_name) == 0)
                  || (optionDesc->short_name != NULL && strcmp(argPart, optionDesc->short_name) == 0);
    if (fullMatch) {
        if (optionDesc->takes_value) {
            // The value must be the next argument
            if (argc <= *argIndex + 1) {
                char const* optionName = __argparse_infer_option_name(optionDesc);
                pack->error = __argparse_snprintf("option '%s' requires a value, but none was provided", optionName);
                return false;
            }
            // Parse the value using the option's parse function
            void* value = __argparse_parse_option_value(pack, optionDesc, argv[*argIndex + 1], strlen(argv[*argIndex + 1]));
            if (pack->error != NULL) {
                // Parsing failed, exit immediately
                return false;
            }
            // Success, add the option and its value to the pack
            __argparse_add_option_to_pack(pack, optionDesc, value);
            // Move past the option and its value
            *argIndex += 2;
            return true;
        }
        else {
            // Success, add the option with a NULL value to the pack
            __argparse_add_option_to_pack(pack, optionDesc, NULL);
            // Move past the option
            *argIndex += 1;
            return true;
        }
    }
    // Not a full match, so even in the best case this is an option with a value separator right after
    if (!optionDesc->takes_value) return false;
    // Try to match with a separator followed by a value
    char const* separators[] = { ARGPARSE_VALUE_SEPARATORS };
    const size_t separatorCount = sizeof(separators) / sizeof(separators[0]);
    // Check if the argument part starts with the long or short name of the option, followed by a separator
    for (size_t i = 0; i < separatorCount; ++i) {
        char const* separator = separators[i];
        size_t nameLength = 0;
        if (optionDesc->long_name != NULL && strncmp(argPart, optionDesc->long_name, strlen(optionDesc->long_name)) == 0) {
            nameLength = strlen(optionDesc->long_name);
        }
        else if (optionDesc->short_name != NULL && strncmp(argPart, optionDesc->short_name, strlen(optionDesc->short_name)) == 0) {
            nameLength = strlen(optionDesc->short_name);
        }
        // We assume 0 is no match
        if (nameLength == 0) continue;
        // Check if the name is followed by the separator
        if (strcmp(argPart + nameLength, separator) != 0) continue;
        // We have a match, so we try to parse the value after the separator
        char const* valueText = argPart + nameLength + strlen(separator);
        // Since the value can be enclosed in quotes here, we trim it so the parser doesn't have to deal with that
        size_t valueTextLength = strlen(valueText);
        if (valueText[0] == '"' || valueText[0] == '\'') {
            if (valueTextLength < 2 || valueText[valueTextLength - 1] != valueText[0]) {
                char const* optionName = __argparse_infer_option_name(optionDesc);
                pack->error = __argparse_snprintf("value for option '%s' is enclosed in quotes, but the closing quote is missing", optionName);
                return false;
            }
            valueText += 1;
            valueTextLength -= 2;
        }
        void* value = __argparse_parse_option_value(pack, optionDesc, valueText, valueTextLength);
        if (pack->error != NULL) {
            // Parsing failed, exit immediately
            return false;
        }
        // Success, add the option and its value to the pack
        __argparse_add_option_to_pack(pack, optionDesc, value);
        // Move past the option
        *argIndex += 1;
        return true;
    }
    // No match
    return false;
}

ArgumentPack argparse_parse(int argc, char** argv, CommandDescription* root_command) {
    ArgumentPack result = { 0 };

    if (argc < 1) {
        result.error = __argparse_strdup("an empty argument list was provided");
        return result;
    }

    result.program_name = argv[0];
    result.command = root_command;

    int argIndex = 1;

    // Look up the appropriate subcommand
    while (argIndex < argc) {
        char const* argPart = argv[argIndex];

        // Look for a subcommand with the given name
        CommandDescription* matchingCommand = NULL;
        for (size_t i = 0; i < result.command->subcommands_length; ++i) {
            if (strcmp(result.command->subcommands[i].name, argPart) == 0) {
                matchingCommand = &result.command->subcommands[i];
                break;
            }
        }

        // If we found a matching subcommand, move into it and continue parsing
        if (matchingCommand != NULL) {
            result.command = matchingCommand;
            ++argIndex;
        }
        else {
            // Otherwise, we assume that the remaining arguments are options for the current command
            break;
        }
    }

    // We have looked up the appropriate command, now we need to parse the options for it
    // NOTE: We ensure options-length capacity, which is just a rough estimation
    // In reality, some options are optional, some might allow multiple instances to be specified
    __argparse_pack_ensure_option_capacity(&result, result.command->options_length);
    while (argIndex < argc) {
        // Try to match the argument to an option
        for (size_t i = 0; i < result.command->options_length; ++i) {
            OptionDescription* optionDesc = &result.command->options[i];
            if (__argparse_match_option_to_argument(&result, optionDesc, argc, argv, &argIndex)) {
                // Match found
                goto found_option;
            }
            else if (result.error != NULL) {
                // Error, exit immediately
                return result;
            }
            else {
                // No match, try the next option
                continue;
            }
        }

        // If we get here, we couldn't match the argument to any option, so we return an error
        result.error = __argparse_snprintf("unrecognized argument '%s' for command '%s'", argv[argIndex], result.command->name);
        return result;

    found_option:
        continue;
    }

    // Check, if all required options were provided, and if not, return an error
    for (size_t i = 0; i < result.command->options_length; ++i) {
        OptionDescription* optionDesc = &result.command->options[i];
        if (!optionDesc->is_required) continue;

        // Check if we have a value for the required option
        bool found = false;
        for (size_t j = 0; j < result.options_length; ++j) {
            if (__argparse_options_eq(&result.options[j].description, optionDesc)) {
                found = true;
                break;
            }
        }
        if (!found) {
            char const* optionName = __argparse_infer_option_name(optionDesc);
            result.error = __argparse_snprintf("required argument '%s' was not provided for command '%s'", optionName, result.command->name);
            return result;
        }
    }

    return result;
}

void argparse_free(ArgumentPack* pack) {
    for (size_t i = 0; i < pack->options_length; ++i) {
        ARGPARSE_FREE(pack->options[i].value);
    }
    ARGPARSE_FREE(pack->options);
    if (pack->error != NULL) {
        ARGPARSE_FREE((void*)pack->error);
    }
}

void argparse_add_option(CommandDescription* command, OptionDescription optionDesc) {
    if (command->options_length >= command->options_capacity) {
        size_t newCapacity = (command->options_capacity == 0) ? 8 : command->options_capacity * 2;
        OptionDescription* newOptions = (OptionDescription*)ARGPARSE_REALLOC(command->options, sizeof(OptionDescription) * newCapacity);
        ARGPARSE_ASSERT(newOptions != NULL, "failed to allocate memory for command options");
        command->options = newOptions;
        command->options_capacity = newCapacity;
    }
    command->options[command->options_length++] = optionDesc;
}

void argparse_add_subcommand(CommandDescription* command, CommandDescription subcommand) {
    if (command->subcommands_length >= command->subcommands_capacity) {
        size_t newCapacity = (command->subcommands_capacity == 0) ? 8 : command->subcommands_capacity * 2;
        CommandDescription* newSubcommands = (CommandDescription*)ARGPARSE_REALLOC(command->subcommands, sizeof(CommandDescription) * newCapacity);
        ARGPARSE_ASSERT(newSubcommands != NULL, "failed to allocate memory for command subcommands");
        command->subcommands = newSubcommands;
        command->subcommands_capacity = newCapacity;
    }
    command->subcommands[command->subcommands_length++] = subcommand;
}

void argparse_free_command(CommandDescription* command) {
    ARGPARSE_FREE(command->options);
    command->options = NULL;
    command->options_length = 0;
    command->options_capacity = 0;

    for (size_t i = 0; i < command->subcommands_length; ++i) {
        argparse_free_command(&command->subcommands[i]);
    }
    ARGPARSE_FREE(command->subcommands);
    command->subcommands = NULL;
    command->subcommands_length = 0;
    command->subcommands_capacity = 0;
}

bool argparse_has_option(ArgumentPack* pack, OptionDescription* optionDesc) {
    for (size_t i = 0; i < pack->options_length; ++i) {
        if (&pack->options[i].description == optionDesc) {
            return true;
        }
    }
    return false;
}

void* argparse_get_value(ArgumentPack* pack, OptionDescription* optionDesc) {
    for (size_t i = 0; i < pack->options_length; ++i) {
        if (&pack->options[i].description == optionDesc) {
            return pack->options[i].value;
        }
    }
    return optionDesc->default_value;
}

#endif /* ARGPARSE_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_SELF_TEST

#include <ctype.h>
#include <stdio.h>

// Let's use our own test framework
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

static OptionParseResult parse_int_option(char const* text, size_t length) {
    // Just check, if it's all digits
    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)text[i])) {
            return (OptionParseResult){
                .value = NULL,
                // TODO: We shouldn't leak internal API like this
                // But we should also provide the user with a nicer API to report errors...
                .error = __argparse_snprintf("expected an integer value, but got '%s'", text),
            };
        }
    }
    // It is a valid integer, so we parse it and return the value
    int* value = (int*)ARGPARSE_REALLOC(NULL, sizeof(int));
    ARGPARSE_ASSERT(value != NULL, "failed to allocate memory for parsed integer value");
    *value = atoi(text);
    return (OptionParseResult){
        .value = value,
        .error = NULL,
    };
}

// We build a command-tree like so:
// <root>: two options, --number/-n that takes an integer value (required) and --flag/-f that is a simple bool (optional)
static CommandDescription build_sample_command(void) {
    CommandDescription rootCommand = { 0 };
    rootCommand.name = "<root>";
    rootCommand.description = "Root command";

    argparse_add_option(&rootCommand, (OptionDescription){
        .long_name = "--number",
        .short_name = "-n",
        .description = "A required option that takes an integer value",
        .is_required = true,
        .takes_value = true,
        .default_value = NULL,
        .allow_multiple = false,
        .parse_fn = parse_int_option,
    });
    argparse_add_option(&rootCommand, (OptionDescription){
        .long_name = "--flag",
        .short_name = "-f",
        .description = "An optional flag that doesn't take a value",
        .is_required = false,
        .takes_value = false,
        .default_value = NULL,
        .allow_multiple = false,
        .parse_fn = NULL,
    });

    // TODO: To suppress error
    if (false) {
        argparse_add_subcommand(&rootCommand, (CommandDescription){
            .name = "subcommand",
            .description = "A subcommand that doesn't do anything",
        });
    }

    return rootCommand;
}

CTEST_CASE(empty_argument_list) {
    CommandDescription rootCommand = build_sample_command();
    ArgumentPack pack = argparse_parse(0, NULL, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);
    // printf("Expected error for empty argument list: %s\n", pack.error);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(missing_required_option) {
    CommandDescription rootCommand = build_sample_command();
    char* argv[] = { "program" };
    ArgumentPack pack = argparse_parse(1, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);
    // printf("Expected error for missing required option: %s\n", pack.error);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(unrecognized_option) {
    CommandDescription rootCommand = build_sample_command();
    char* argv[] = { "program", "--number", "42", "--unknown" };
    ArgumentPack pack = argparse_parse(4, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);
    // printf("Expected error for unrecognized option: %s\n", pack.error);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(invalid_option_value) {
    CommandDescription rootCommand = build_sample_command();
    char* argv[] = { "program", "--number", "not_an_integer" };
    ArgumentPack pack = argparse_parse(3, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);
    // printf("Expected error for invalid option value: %s\n", pack.error);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(successful_parse_separate_argument) {
    CommandDescription rootCommand = build_sample_command();
    char* argv[] = { "program", "--number", "42", "--flag" };
    ArgumentPack pack = argparse_parse(4, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error == NULL);
    void* numberValue = argparse_get_value(&pack, &rootCommand.options[0]);
    CTEST_ASSERT_TRUE(numberValue != NULL);
    CTEST_ASSERT_TRUE(*(int*)numberValue == 42);
    bool hasFlag = argparse_has_option(&pack, &rootCommand.options[1]);
    CTEST_ASSERT_TRUE(hasFlag);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

#endif /* ARGPARSE_SELF_TEST */
