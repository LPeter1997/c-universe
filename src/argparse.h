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

    CommandDescription* subcommands;
    size_t subcommands_length;
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

ArgumentPack argparse_parse(int argc, char** argv, CommandDescription* root_command);

#ifdef __cplusplus
}
#endif

#endif /* ARGPARSE_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_IMPLEMENTATION

#include <assert.h>
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
    size_t length = vsnprintf(NULL, 0, format, args);
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
            if (__argparse_match_option_to_argument(&result, optionDesc, argv, &argIndex)) {
                // Match found
                break;
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
    }

    // Check, if all required options were provided, and if not, return an error
    for (size_t i = 0; i < result.command->options_length; ++i) {
        OptionDescription* optionDesc = &result.command->options[i];
        if (!optionDesc->is_required) continue;

        // Check if we have a value for the required option
        bool found = false;
        for (size_t j = 0; j < result.options_length; ++j) {
            if (result.options[j].description == optionDesc) {
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

#endif /* ARGPARSE_IMPLEMENTATION */
