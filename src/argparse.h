/**
 * argparse.h is a single-header library for parsing command-line arguments in C.
 *
 * The goal was to get something relatively extensible and similar to System.CommandLine in .NET.
 *
 * Features:
 *  - Root command, subcommands
 *  - Options with or without names (latter being positional arguments) prefixed with '-', '--' or '/'
 *  - Arguments with different arities
 *  - Default values
 *  - Custom parsing functions for options
 *  - Double-dash (--) to escape options and treat all following arguments as positional
 *  - Option-value delimiters with a space, '=' or ':'
 *  - Option bundling for short-named options (e.g. '-abc' is equivalent to '-a -b -c')
 *  - Response files (e.g. '@args.txt' to read additional arguments from a file)
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

#ifdef __cplusplus
extern "C" {
#endif

struct Argparse_Command;
struct Argparse_Argument;
struct Argparse_Option;

typedef struct Argparse_Pack {
    char const* program_name;

    struct Argparse_Command* command;

    // Owned array of options that were parsed for the command
    struct {
        struct Argparse_Argument* elements;
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

typedef struct Argparse_ParseResult {
    // Owned pointer to the parsed value, which must be freed by the caller in case of a parsing success
    void* value;
    // Owned error message in case of a parsing failure, must be freed by the caller
    // A non-NULL value indicates a parsing failure, while a NULL value indicates a parsing success
    char const* error;
} Argparse_ParseResult;

typedef enum Argparse_Arity {
    ARGPARSE_ARITY_ZERO,
    ARGPARSE_ARITY_ZERO_OR_ONE,
    ARGPARSE_ARITY_EXACTLY_ONE,
    ARGPARSE_ARITY_ZERO_OR_MORE,
    ARGPARSE_ARITY_ONE_OR_MORE,
} Argparse_Arity;

typedef Argparse_ParseResult Argparse_ParseFn(char const* text, size_t length);
typedef void* Argparse_ValueFn(struct Argparse_Option* option);

typedef struct Argparse_Option {
    char const* long_name;
    char const* short_name;
    char const* description;
    Argparse_Arity arity;
    void* default_value;
    Argparse_ParseFn* parse_fn;
    Argparse_ValueFn* default_value_fn;
} Argparse_Option;

typedef int Argparse_HandlerFn(Argparse_Pack* pack);

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
    struct {
        void** elements;
        size_t length;
        size_t capacity;
    } values;
} Argparse_Argument;

ARGPARSE_DEF Argparse_Pack argparse_parse(int argc, char** argv, Argparse_Command* root);
ARGPARSE_DEF Argparse_Argument* argparse_get_argument(Argparse_Pack* pack, char const* name);
ARGPARSE_DEF void argparse_free_pack(Argparse_Pack* pack);

ARGPARSE_DEF void argparse_add_option(Argparse_Command* command, Argparse_Option option);
ARGPARSE_DEF void argparse_add_subcommand(Argparse_Command* command, Argparse_Command subcommand);
ARGPARSE_DEF void argparse_free_command(Argparse_Command* command);

#ifdef __cplusplus
}
#endif

#endif /* ARGPARSE_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_IMPLEMENTATION

#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#define ARGPARSE_ASSERT(condition, message) assert(((void)message, condition))
#define ARGPARSE_ADD_TO_ARRAY(array, length, capacity, element) \
    do { \
        if ((length) + 1 > (capacity)) { \
            size_t newCapacity = ((capacity) == 0) ? 8 : ((capacity) * 2); \
            void* newElements = ARGPARSE_REALLOC((array), newCapacity * sizeof(*(array))); \
            ARGPARSE_ASSERT(newElements != NULL, "failed to allocate memory for array"); \
            (array) = newElements; \
            (capacity) = newCapacity; \
        } \
        (array)[(length)++] = element; \
    } while (false)

#ifdef __cplusplus
extern "C" {
#endif

static bool argparse_option_has_name(Argparse_Option* option, char const* name) {
    return (option->long_name != NULL && strcmp(option->long_name, name) == 0)
        || (option->short_name != NULL && strcmp(option->short_name, name) == 0);
}

static void argparse_add_error(Argparse_Pack* pack, char const* error) {
    ARGPARSE_ADD_TO_ARRAY(pack->errors.elements, pack->errors.length, pack->errors.capacity, error);
}

static void argparse_add_argument(Argparse_Pack* pack, Argparse_Argument arg) {
    ARGPARSE_ADD_TO_ARRAY(pack->arguments.elements, pack->arguments.length, pack->arguments.capacity, arg);
}

static void argparse_add_value_to_argument(Argparse_Argument* argument, void* value) {
    ARGPARSE_ADD_TO_ARRAY(argument->values.elements, argument->values.length, argument->values.capacity, value);
}

// Tokenization logic //////////////////////////////////////////////////////////

static bool argparse_is_value_delimiter(char c) {
    return c == '=' || c == ':';
}

typedef struct Argparse_Response {
    char const* text;
    size_t length;
    size_t index;
    struct Argparse_Response* next;
} Argparse_Response;

typedef struct Argparse_Tokenizer {
    int argc;
    char** argv;
    size_t argvIndex;
    size_t charIndex;
    Argparse_Response* currentResponse;
} Argparse_Tokenizer;

static void argparse_tokenizer_push_response(Argparse_Tokenizer* tokenizer, Argparse_Response response) {
    response.next = tokenizer->currentResponse;
    tokenizer->currentResponse = (Argparse_Response*)ARGPARSE_REALLOC(NULL, sizeof(Argparse_Response));
    ARGPARSE_ASSERT(tokenizer->currentResponse != NULL, "failed to allocate memory for tokenizer response");
    *tokenizer->currentResponse = response;
}

static void argparse_tokenizer_pop_response(Argparse_Tokenizer* tokenizer) {
    Argparse_Response* toPop = tokenizer->currentResponse;
    tokenizer->currentResponse = toPop->next;
    ARGPARSE_FREE(toPop);
}

static bool argparse_tokenizer_next(Argparse_Tokenizer* tokenizer, char** outToken, size_t* outLength) {
    if (tokenizer->currentResponse != NULL) {
        // We need to parse from the response file
        // TODO
        ARGPARSE_ASSERT(false, "response file tokenization not implemented yet");
    }

    // End of arguments
    if (tokenizer->argvIndex >= (size_t)tokenizer->argc) return false;

    char* currentArg = tokenizer->argv[tokenizer->argvIndex];
    if (tokenizer->charIndex == 0 && currentArg[0] == '@') {
        // TODO: Implement response file tokenization
        ARGPARSE_ASSERT(false, "response file tokenization not implemented yet");
    }

    // We eat the current token until either a value delimiter (for options) or the end of the argument
    size_t offset = 0;
    while (currentArg[tokenizer->charIndex + offset] != '\0'
        && !argparse_is_value_delimiter(currentArg[tokenizer->charIndex + offset])) ++offset;

    // If we hit the end of the token, just report it as is
    if (currentArg[tokenizer->charIndex + offset] == '\0') {
        *outToken = &currentArg[tokenizer->charIndex];
        *outLength = offset;
        ++tokenizer->argvIndex;
        tokenizer->charIndex = 0;
        return true;
    }

    // If we hit a value delimiter, we skip that and report the token until that point, and we will report the value in the next call
    *outToken = &currentArg[tokenizer->charIndex];
    *outLength = offset;
    tokenizer->charIndex += offset + 1;
    return true;
}

// Public API //////////////////////////////////////////////////////////////////

Argparse_Pack argparse_parse(int argc, char** argv, Argparse_Command* root) {
    ARGPARSE_ASSERT(false, "not implemented yet");
}

Argparse_Argument* argparse_get_argument(Argparse_Pack* pack, char const* name) {
    for (size_t i = 0; i < pack->arguments.length; ++i) {
        if (argparse_option_has_name(pack->arguments.elements[i].option, name)) {
            return &pack->arguments.elements[i];
        }
    }
    return NULL;
}

void argparse_free_pack(Argparse_Pack* pack) {
    // Free all allocated memory for the parsed arguments
    // Do not deallocate the command or options, as they are owned by the command hierarchy
    for (size_t i = 0; i < pack->arguments.length; ++i) {
        Argparse_Argument* argument = &pack->arguments.elements[i];
        for (size_t j = 0; j < argument->values.length; ++j) {
            ARGPARSE_FREE(argument->values.elements[j]);
        }
        ARGPARSE_FREE(argument->values.elements);
    }
    ARGPARSE_FREE(pack->arguments.elements);
    for (size_t i = 0; i < pack->errors.length; ++i) {
        ARGPARSE_FREE((void*)pack->errors.elements[i]);
    }
    ARGPARSE_FREE(pack->errors.elements);
}

void argparse_add_option(Argparse_Command* command, Argparse_Option option) {
    ARGPARSE_ADD_TO_ARRAY(command->options.elements, command->options.length, command->options.capacity, option);
}

void argparse_add_subcommand(Argparse_Command* command, Argparse_Command subcommand) {
    ARGPARSE_ADD_TO_ARRAY(command->subcommands.elements, command->subcommands.length, command->subcommands.capacity, subcommand);
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

#ifdef __cplusplus
}
#endif

#undef ARGPARSE_ADD_TO_ARRAY
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
