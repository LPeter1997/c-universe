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

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OptionParseResult {
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
    // The allocated memory might be longer than the actual number of options
    Option* options;
    size_t options_length;

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
    result.options = (Option*)ARGPARSE_REALLOC(NULL, sizeof(Option) * result.command->options_length);
    ARGPARSE_ASSERT(result.options != NULL, "failed to allocate memory for options");
    while (argIndex < argc) {
        char const* argPart = argv[argIndex];

        // If we already have the max number of argumnets, then we have an error
        if (result.options_length >= result.command->options_length) {
            result.error = __argparse_snprintf("argument '%s' is over the maximum number of allowed arguments for command '%s'", argPart, result.command->name);
            return result;
        }

        // Parse this option, find the matching option description for it
        // First we look for a full match for either the long or the short name of the option
        for (size_t i = 0; i < result.command->options_length; ++i) {
            OptionDescription* optionDesc = &result.command->options[i];

            // Check if this option matches the long name or the short name
            if (!((optionDesc->long_name != NULL && strcmp(optionDesc->long_name, argPart) == 0)
               || (optionDesc->short_name != NULL && strcmp(optionDesc->short_name, argPart) == 0)))  continue;

            // Full name match
            if (!optionDesc->takes_value) {
                // The option takes no value, we can just add it to the result and move on
                result.options[result.options_length++] = (Option){ .description = *optionDesc, .value = optionDesc->default_value };
                ++argIndex;
                goto next_arg;
            }

            // The option takes a value, so we need to look at the next argument for the value
            if (argIndex + 1 >= argc) {
                result.error = __argparse_snprintf("option '%s' requires a value, but no more arguments were provided", argPart);
                return result;
            }

            // We have a value for this option, so we can parse it using the option's parse function
            char const* valuePart = argv[argIndex + 1];
            OptionParseResult parseResult = optionDesc->parse_fn(valuePart, strlen(valuePart));
            if (parseResult.error != NULL) {
                result.error = __argparse_snprintf("failed to parse value '%s' for option '%s': %s", valuePart, argPart, parseResult.error);
                ARGPARSE_FREE(parseResult.error);
                return result;
            }

            // We successfully parsed the value, so we can add the option with the parsed value to the result and move on
            result.options[result.options_length++] = (Option){ .description = *optionDesc, .value = parseResult.value };
            argIndex += 2;
            goto next_arg;
        }
    next_arg:;
    }
}

#endif /* ARGPARSE_IMPLEMENTATION */
