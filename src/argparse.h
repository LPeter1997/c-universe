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
    bool success;
    void* value;
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
            break;
        }

        // Look for an option matching the given argument
        // TODO
    }
}

#endif /* ARGPARSE_IMPLEMENTATION */
