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
    Option* options;
    size_t options_length;

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

#include <string.h>

ArgumentPack argparse_parse(int argc, char** argv, CommandDescription* root_command) {
    ArgumentPack result = { 0 };

    if (argc < 1) {
        result.error = "an empty argument list was provided";
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
    // TODO
}

#endif /* ARGPARSE_IMPLEMENTATION */
