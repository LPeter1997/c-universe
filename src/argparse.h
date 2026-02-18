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

#ifndef ARGPARSE_ARGUMENT_SEPARATORS
#   define ARGPARSE_ARGUMENT_SEPARATORS "=", ":"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Argparse_ParseResult {
    // Owned pointer to the parsed value, which must be freed by the caller in case of a parsing success
    void* value;
    // Owned error message in case of a parsing failure, must be freed by the caller
    // A non-NULL value indicates a parsing failure, while a NULL value indicates a parsing success
    char const* error;
} Argparse_ParseResult;

typedef Argparse_ParseResult Argparse_ParseFn(char const* text, size_t length);

typedef struct Argparse_Option {
    char const* long_name;
    char const* short_name;
    char const* description;
    bool is_required;
    bool takes_value;
    void* default_value;
    bool allow_multiple;
    Argparse_ParseFn* parse_fn;
} Argparse_Option;

struct Argparse_Pack;
typedef int Argparse_HandlerFn(struct Argparse_Pack* pack);

typedef struct Argparse_Command {
    char const* name;
    char const* description;
    Argparse_HandlerFn* handler_fn;

    struct {
        Argparse_Option* elements;
        size_t length;
        size_t capacity;
    } options;

    struct {
        struct Argparse_Command* elements;
        size_t length;
        size_t capacity;
    } subcommands;
} Argparse_Command;

typedef struct Argparse_Argument {
    Argparse_Option* option;
    void* value;
    struct {
        void** elements;
        size_t length;
        size_t capacity;
    } multiple_values;
} Argparse_Argument;

typedef struct Argparse_Pack {
    char const* program_name;

    Argparse_Command* command;

    // Owned array of options that were parsed for the command
    struct {
         Argparse_Argument* elements;
         size_t length;
         size_t capacity;
    } arguments;

    // Owned list of errors
    struct {
        char const** elements;
        size_t length;
        size_t capacity;
    } errors;
} Argparse_Pack;

ARGPARSE_DEF Argparse_Pack argparse_parse(int argc, char** argv, Argparse_Command* root);
ARGPARSE_DEF void argparse_free(Argparse_Pack* pack);

ARGPARSE_DEF void argparse_add_option(Argparse_Command* command, Argparse_Option option);
ARGPARSE_DEF void argparse_add_subcommand(Argparse_Command* command, Argparse_Command subcommand);
ARGPARSE_DEF void argparse_free_command(Argparse_Command* command);

ARGPARSE_DEF Argparse_Argument* argparse_get_argument(Argparse_Pack* pack, char const* name);

#ifdef __cplusplus
}
#endif

#endif /* ARGPARSE_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_IMPLEMENTATION

#include <assert.h>

#define ARGPARSE_ASSERT(condition, message) assert(((void)message, condition))

#ifdef __cplusplus
extern "C" {
#endif

// Public API //////////////////////////////////////////////////////////////////

Argparse_Pack argparse_parse(int argc, char** argv, Argparse_Command* root) {
    ARGPARSE_ASSERT(false, "not implemented yet");
}

void argparse_free(Argparse_Pack* pack) {
    // Free all allocated memory for the parsed arguments
    // Do not deallocate the command or options, as they are owned by the command hierarchy
    for (size_t i = 0; i < pack->arguments.length; ++i) {
        Argparse_Argument* argument = &pack->arguments.elements[i];
        if (argument->option->allow_multiple) {
            for (size_t j = 0; j < argument->multiple_values.length; ++j) {
                ARGPARSE_FREE(argument->multiple_values.elements[j]);
            }
            ARGPARSE_FREE(argument->multiple_values.elements);
        }
        else {
            ARGPARSE_FREE(argument->value);
        }
    }
    ARGPARSE_FREE(pack->arguments.elements);
    for (size_t i = 0; i < pack->errors.length; ++i) {
        ARGPARSE_FREE((void*)pack->errors.elements[i]);
    }
    ARGPARSE_FREE(pack->errors.elements);
}

void argparse_add_option(Argparse_Command* command, Argparse_Option option) {
    ARGPARSE_ASSERT(false, "not implemented yet");
}

void argparse_add_subcommand(Argparse_Command* command, Argparse_Command subcommand) {
    ARGPARSE_ASSERT(false, "not implemented yet");
}

void argparse_free_command(Argparse_Command* command) {
    // We assume name and description are static, so we don't free them
    // We only free the options and subcommands arrays, as well as the subcommands themselves
    for (size_t i = 0; i < command->subcommands.length; ++i) {
        argparse_free_command(&command->subcommands.elements[i]);
    }
    ARGPARSE_FREE(command->subcommands.elements);
    ARGPARSE_FREE(command->options.elements);
}

Argparse_Argument* argparse_get_argument(Argparse_Pack* pack, char const* name) {
    ARGPARSE_ASSERT(false, "not implemented yet");
}

#ifdef __cplusplus
}
#endif

#undef ARGPARSE_ASSERT

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

static Argparse_ParseResult parse_int_option(char const* text, size_t length) {
    // Just check, if it's all digits
    for (size_t i = 0; i < length; ++i) {
        if (!isdigit((unsigned char)text[i])) {
            return (Argparse_ParseResult){
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
    return (Argparse_ParseResult){
        .value = value,
        .error = NULL,
    };
}

// We build a command-tree like so:
// <root>: two options, --number/-n that takes an integer value (required) and --flag/-f that is a simple bool (optional)
static Argparse_Command build_sample_command(void) {
    Argparse_Command rootCommand = { 0 };
    rootCommand.name = "<root>";
    rootCommand.description = "Root command";

    argparse_add_option(&rootCommand, (Argparse_Option){
        .long_name = "--number",
        .short_name = "-n",
        .description = "A required option that takes an integer value",
        .is_required = true,
        .takes_value = true,
        .default_value = NULL,
        .allow_multiple = false,
        .parse_fn = parse_int_option,
    });
    argparse_add_option(&rootCommand, (Argparse_Option){
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
        argparse_add_subcommand(&rootCommand, (Argparse_Command){
            .name = "subcommand",
            .description = "A subcommand that doesn't do anything",
        });
    }

    return rootCommand;
}

CTEST_CASE(empty_argument_list) {
    Argparse_Command rootCommand = build_sample_command();
    Argparse_Pack pack = argparse_parse(0, NULL, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(missing_required_option) {
    Argparse_Command rootCommand = build_sample_command();
    char* argv[] = { "program" };
    Argparse_Pack pack = argparse_parse(1, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(unrecognized_option) {
    Argparse_Command rootCommand = build_sample_command();
    char* argv[] = { "program", "--number", "42", "--unknown" };
    Argparse_Pack pack = argparse_parse(4, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(invalid_option_value) {
    Argparse_Command rootCommand = build_sample_command();
    char* argv[] = { "program", "--number", "not_an_integer" };
    Argparse_Pack pack = argparse_parse(3, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error != NULL);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(successful_parse_separate_argument) {
    Argparse_Command rootCommand = build_sample_command();
    char* argv[] = { "program", "--number", "42", "--flag" };
    Argparse_Pack pack = argparse_parse(4, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error == NULL);
    void* numberValue = argparse_get_value(&pack, "--number");
    CTEST_ASSERT_TRUE(numberValue != NULL);
    CTEST_ASSERT_TRUE(*(int*)numberValue == 42);
    bool hasFlag = argparse_has_option(&pack, "--flag");
    CTEST_ASSERT_TRUE(hasFlag);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

CTEST_CASE(successful_parse_combined_argument) {
    Argparse_Command rootCommand = build_sample_command();
    char* argv[] = { "program", "--number=42", "-f" };
    Argparse_Pack pack = argparse_parse(3, argv, &rootCommand);

    CTEST_ASSERT_TRUE(pack.error == NULL);
    void* numberValue = argparse_get_value(&pack, "--number");
    CTEST_ASSERT_TRUE(numberValue != NULL);
    CTEST_ASSERT_TRUE(*(int*)numberValue == 42);
    bool hasFlag = argparse_has_option(&pack, "--flag");
    CTEST_ASSERT_TRUE(hasFlag);

    argparse_free(&pack);
    argparse_free_command(&rootCommand);
}

#endif /* ARGPARSE_SELF_TEST */
