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

ARGPARSE_DEF char* argparse_format(char const* format, ...);

#ifdef __cplusplus
}
#endif

#endif /* ARGPARSE_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_IMPLEMENTATION

#include <assert.h>
#include <ctype.h>
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

static bool argparse_is_quote(char c) {
    return c == '"' || c == '\'';
}

typedef struct Argparse_Response {
    char const* text;
    size_t length;
    size_t index;
    struct Argparse_Response* next;
} Argparse_Response;

typedef struct Argparse_Token {
    char const* text;
    size_t index;
    size_t length;
} Argparse_Token;

typedef struct Argparse_Tokenizer {
    int argc;
    char** argv;
    size_t argvIndex;
    Argparse_Response* currentResponse;
    Argparse_Token currentToken;
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

static void argparse_tokenizer_read_current_from_response(Argparse_Tokenizer* tokenizer) {
    ARGPARSE_ASSERT(tokenizer->currentResponse != NULL, "cannot read from response, no current response present");
    Argparse_Response* response = tokenizer->currentResponse;

start:
    // End of response
    if (response->index >= response->length) {
        tokenizer->currentToken.text = NULL;
        tokenizer->currentToken.length = 0;
        tokenizer->currentToken.index = 0;
        return;
    }
    // Whitespace, skip
    if (isspace(response->text[response->index])) {
        ++response->index;
        goto start;
    }
    // Token start
    tokenizer->currentToken.text = response->text + response->index;
    tokenizer->currentToken.index = 0;
    // We determine the length by looking for the next whitespace that's NOT between quotes
    char currentQuote = '\0';
    size_t length = 0;
    while (response->index + length < response->length) {
        char c = response->text[response->index + length];
        if (currentQuote == '\0') {
            if (isspace(c)) break;
            if (argparse_is_quote(c)) currentQuote = c;
        }
        else if (c == currentQuote) {
            currentQuote = '\0';
        }
        ++length;
    }
    tokenizer->currentToken.length = length;
}

static void argparse_tokenizer_read_current(Argparse_Tokenizer* tokenizer) {
    if (tokenizer->currentResponse == NULL) {
        // Easy, read from argv
        if (tokenizer->argvIndex >= (size_t)tokenizer->argc) {
            // No more tokens to read
            tokenizer->currentToken.text = NULL;
            tokenizer->currentToken.length = 0;
        }
        else {
            tokenizer->currentToken.text = tokenizer->argv[tokenizer->argvIndex];
            tokenizer->currentToken.length = strlen(tokenizer->currentToken.text);
        }
        tokenizer->currentToken.index = 0;
    }
    else {
        argparse_tokenizer_read_current_from_response(tokenizer);
    }
}

static void argparse_tokenizer_skip_current(Argparse_Tokenizer* tokenizer) {
    ARGPARSE_ASSERT(tokenizer->currentToken.text != NULL, "cannot skip current token, no current token present");

retry:
    if (tokenizer->currentResponse == NULL) {
        // Easy, just move to the next argv
        ++tokenizer->argvIndex;
    }
    else {
        // We just move the index forward by the length of the current token, and read the next token
        tokenizer->currentResponse->index = tokenizer->currentToken.index + tokenizer->currentToken.length;
    }
}

static void argparse_tokenizer_unwrap_current(Argparse_Pack* pack, Argparse_Tokenizer* tokenizer) {
    ARGPARSE_ASSERT(tokenizer->currentToken.text != NULL, "cannot unwrap current token, no current token present");

retry:
    // If we are at the start of the token and it's a response, we need to push it onto the stack
    if (tokenizer->currentToken.index == 0 && tokenizer->currentToken.text[0] == '@') {
        // We interpret the entire token as a file path, read the file and push its content as a new response
        char const* filePath = tokenizer->currentToken.text + 1;
        FILE* file = fopen(filePath, "r");
        if (file == NULL) {
            // Failed to open file, report error and return
            char* error = argparse_format("failed to open response file '%s'", filePath);
            argparse_add_error(pack, error);
            return;
        }
        fseek(file, 0, SEEK_END);
        long fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        char* fileContent = (char*)ARGPARSE_REALLOC(NULL, fileSize);
        ARGPARSE_ASSERT(fileContent != NULL, "failed to allocate memory for response file content");
        fread(fileContent, 1, fileSize, file);
        fclose(file);
        argparse_tokenizer_push_response(tokenizer, (Argparse_Response){
            .text = fileContent,
            .length = fileSize,
            .index = 0,
        });
        // After pushing the new response, we need to read the current token again (which will now read from the new response)
        argparse_tokenizer_read_current(tokenizer);
        goto retry;
    }

    // We are done unwrapping
}

static bool argparse_tokenizer_next(Argparse_Pack* pack, Argparse_Tokenizer* tokenizer, char** outToken, size_t* outLength) {
    // If we haven't read a token yet, try to read it
    Argparse_Token* token = &tokenizer->currentToken;
    if (token->text == NULL) {
        argparse_tokenizer_read_current(tokenizer);
        // If we still don't have a token, we are done
        if (token->text == NULL) return false;
        // Try to unwrap it
        argparse_tokenizer_unwrap_current(pack, tokenizer);
        // If we got an error reported, we are done
        if (pack->errors.length > 0) return false;
    }

    // We have a current token, parse until a value separator or the end of the token
    char* tokenText = token->text + token->index;
    size_t tokenLength = 0;
    while (token->index + tokenLength < token->length) {
        char c = token->text[token->index + tokenLength];
        if (argparse_is_value_delimiter(c)) break;
        ++tokenLength;
    }
    // Save result
    *outToken = tokenText;
    *outLength = tokenLength;
    // Move forward in the token
    tokenizer->currentToken.index += tokenLength;
    // If we stopped at a value delimiter, skip over that
    if (token->index < token->length && argparse_is_value_delimiter(token->text[token->index])) {
        ++token->index;
    }
    // If we got to the end of the token, skip it so that we read a new token on the next call
    if (token->index >= token->length) {
        argparse_tokenizer_skip_current(tokenizer);
    }
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

char* argparse_format(char const* format, ...) {
    va_list args;
    va_start(args, format);
    int length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    char* buffer = (char*)ARGPARSE_REALLOC(NULL, length + 1);
    ARGPARSE_ASSERT(buffer != NULL, "failed to allocate memory for formatted string");
    va_start(args, format);
    vsnprintf(buffer, length + 1, format, args);
    va_end(args);
    return buffer;
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
