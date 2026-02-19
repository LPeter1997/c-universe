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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
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

static bool argparse_is_positional_option(Argparse_Option* option) {
    return option->long_name == NULL && option->short_name == NULL;
}

static Argparse_Command* argparse_find_subcommand_with_name_n(Argparse_Command* command, char const* name, size_t nameLength) {
    for (size_t i = 0; i < command->subcommands.length; ++i) {
        Argparse_Command* subcommand = &command->subcommands.elements[i];
        if (strlen(subcommand->name) == nameLength && strncmp(subcommand->name, name, nameLength) == 0) {
            return subcommand;
        }
    }
    return NULL;
}

static Argparse_Option* argparse_find_option_with_name_n(Argparse_Command* command, char const* name, size_t nameLength) {
    for (size_t i = 0; i < command->options.length; ++i) {
        Argparse_Option* option = &command->options.elements[i];
        if ((option->long_name != NULL && strlen(option->long_name) == nameLength && strncmp(option->long_name, name, nameLength) == 0)
         || (option->short_name != NULL && strlen(option->short_name) == nameLength && strncmp(option->short_name, name, nameLength) == 0)) {
            return option;
        }
    }
    return NULL;
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

static bool argparse_is_legal_prefix_for_option(char const* name, size_t length) {
    // We accept '-', '--' and '/'
    if (length == 0) return false;
    return name[0] == '-' || name[0] == '/';
}

static bool argparse_is_legal_prefix_for_bundling(char const* name, size_t length) {
    // We accept '-' and '/', explicitly disallow '--'
    if (length == 0) return false;
    if (name[0] == '-') return length < 2 || name[1] != '-';
    if (name[0] == '/') return true;
    return false;
}

static bool argparse_argument_can_take_value(Argparse_Argument* argument) {
    if (argument == NULL) return false;

    Argparse_Arity arity = argument->option->arity;
    switch (arity) {
    case ARGPARSE_ARITY_ZERO:
        return false;
    case ARGPARSE_ARITY_ZERO_OR_ONE:
    case ARGPARSE_ARITY_EXACTLY_ONE:
        return argument->values.length < 1;
    case ARGPARSE_ARITY_ZERO_OR_MORE:
    case ARGPARSE_ARITY_ONE_OR_MORE:
        return true;
    }
    return false;
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
    ARGPARSE_ASSERT(toPop != NULL, "cannot pop response, response stack is empty");
    tokenizer->currentResponse = toPop->next;
    // Free the popped response's text and the response itself
    ARGPARSE_FREE(toPop->text);
    ARGPARSE_FREE(toPop);
}

static void argparse_tokenizer_read_current_from_response(Argparse_Tokenizer* tokenizer) {
    ARGPARSE_ASSERT(tokenizer->currentResponse != NULL, "cannot read from response, no current response present");
    Argparse_Response* response = tokenizer->currentResponse;
    Argparse_Token* token = &tokenizer->currentToken;

start:
    // End of response
    if (response->text == NULL || response->index >= response->length) {
        token->text = NULL;
        token->length = 0;
        token->index = 0;
        return;
    }
    // Whitespace, skip
    if (isspace(response->text[response->index])) {
        ++response->index;
        goto start;
    }
    // Token start
    token->text = response->text + response->index;
    token->index = 0;
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
    token->length = length;
}

// Reads the current token from whatever the current active source is (argv or response stack)
// Does not do any extra handling
// If the source is exhausted, sets currentToken.text to NULL
static void argparse_tokenizer_read_current(Argparse_Tokenizer* tokenizer) {
    if (tokenizer->currentResponse == NULL) {
        Argparse_Token* token = &tokenizer->currentToken;
        // Easy, read from argv
        if (tokenizer->argvIndex >= (size_t)tokenizer->argc) {
            // No more tokens to read
            token->text = NULL;
            token->length = 0;
        }
        else {
            token->text = tokenizer->argv[tokenizer->argvIndex];
            token->length = strlen(token->text);
        }
        token->index = 0;
    }
    else {
        argparse_tokenizer_read_current_from_response(tokenizer);
    }
}

// Skips over the current token, moving the tokenizer forward to the next token
// Does not read in the next token, merely advances the source (argv or response stack)
static void argparse_tokenizer_skip_current(Argparse_Tokenizer* tokenizer) {
    Argparse_Token* token = &tokenizer->currentToken;
    ARGPARSE_ASSERT(token->text != NULL, "cannot skip current token, no current token present");

    if (tokenizer->currentResponse == NULL) {
        // Easy, just move to the next argv
        ++tokenizer->argvIndex;
    }
    else {
        // We just move the index forward by the length of the current token
        tokenizer->currentResponse->index += token->length;
    }

    // Either way, we clear the currnet token's state, so that we read a new token on the next read
    token->text = NULL;
    token->length = 0;
    token->index = 0;
}

// Handles the current token, unwrapping it if it's a response file token and pushing it on the stack if so
// Returns true if the token was handled as a response file token, false otherwise
static bool argparse_tokenizer_handle_current_as_response(Argparse_Pack* pack, Argparse_Tokenizer* tokenizer) {
    ARGPARSE_ASSERT(tokenizer->currentToken.text != NULL, "cannot process current token, no current token present");

    // If we are at the start of the token and it's a response, we need to push it onto the stack
    // Not a response
    if (tokenizer->currentToken.index != 0 || tokenizer->currentToken.text[0] != '@') return false;

    Argparse_Token* token = &tokenizer->currentToken;
    // We interpret the entire token as a file path
    char const* filePath = token->text + 1;
    // Skip this token to not re-read it when the response is processed
    argparse_tokenizer_skip_current(tokenizer);
    // Open file for reading
    FILE* file = fopen(filePath, "r");
    if (file == NULL) {
        // Failed to open file, report error, we add a dummy response to the stack to allow processing to continue
        char* error = argparse_format("failed to open response file '%s'", filePath);
        argparse_add_error(pack, error);
        argparse_tokenizer_push_response(tokenizer, (Argparse_Response){
            .text = NULL,
            .length = 0,
            .index = 0,
        });
        return true;
    }
    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    // Read file content
    char* fileContent = (char*)ARGPARSE_REALLOC(NULL, fileSize);
    ARGPARSE_ASSERT(fileContent != NULL, "failed to allocate memory for response file content");
    fread(fileContent, 1, fileSize, file);
    fclose(file);
    // Push content as new response
    argparse_tokenizer_push_response(tokenizer, (Argparse_Response){
        .text = fileContent,
        .length = fileSize,
        .index = 0,
    });
    return true;
}

// Reads in the next full token, handling it as a response file if needed
static bool argparse_tokenizer_next_internal(Argparse_Pack* pack, Argparse_Tokenizer* tokenizer) {
start:
    if (tokenizer->currentToken.text == NULL) {
        // Try to read the current token
        argparse_tokenizer_read_current(tokenizer);
        // If we still don't have a token and the response stack is empty, we are done
        if (tokenizer->currentToken.text == NULL && tokenizer->currentResponse == NULL) return false;
        // If we got a token, check if it's a response file token and handle it if so
        if (tokenizer->currentToken.text != NULL) {
            if (argparse_tokenizer_handle_current_as_response(pack, tokenizer)) {
                // We handled it as a response file token, so we need to read the next token from the new response
                goto start;
            }
            else {
                // Not a response file token, ready to be parsed
                return true;
            }
        }
        // Response stack is not empty but we could not read from it, pop
        argparse_tokenizer_pop_response(tokenizer);
        // Try again with the next response
        goto start;
    }
    // There is a current token, we need to skip it
    argparse_tokenizer_skip_current(tokenizer);
    // Retry
    goto start;
}

static bool argparse_tokenizer_next(
    Argparse_Pack* pack, Argparse_Tokenizer* tokenizer, char** outToken, size_t* outLength, bool* outEndsInValueDelimiter) {
    Argparse_Token* token = &tokenizer->currentToken;

    if (token->index >= token->length) {
        // End of previous token, try to read the next one
        if (!argparse_tokenizer_next_internal(pack, tokenizer)) {
            // No more tokens to read
            *outToken = NULL;
            *outLength = 0;
            *outEndsInValueDelimiter = false;
            return false;
        }
    }

    // We have a current token, parse until a value separator or the end of the token
    char* tokenText = token->text + token->index;
    size_t tokenLength = 0;
    char currentQuote = '\0';
    while (token->index + tokenLength < token->length) {
        char c = token->text[token->index + tokenLength];
        if (currentQuote == '\0') {
            if (argparse_is_quote(c)) currentQuote = c;
        }
        else if (c == currentQuote) {
            currentQuote = '\0';
        }
        // A value delimiter is only legal when not between quotes and this is the start of the token
        if (argparse_is_value_delimiter(c) && currentQuote == '\0' && token->index == 0) break;
        ++tokenLength;
    }
    // Move forward in the token
    token->index += tokenLength;
    // If we stopped at a value delimiter, skip over that
    if (token->index < token->length && argparse_is_value_delimiter(token->text[token->index])) {
        // A value delimiter indicates that the next part of the token is a value
        *outEndsInValueDelimiter = true;
        ++token->index;
    }
    else {
        *outEndsInValueDelimiter = false;
    }
    // Strip quotes, if present
    if (tokenLength > 1 && argparse_is_quote(tokenText[0]) && tokenText[tokenLength - 1] == tokenText[0]) {
        ++tokenText;
        tokenLength -= 2;
    }
    // Save result
    *outToken = tokenText;
    *outLength = tokenLength;
    return true;
}

// Construction ////////////////////////////////////////////////////////////////

static Argparse_Option* argparse_try_get_or_add_option_by_name(Argparse_Pack* pack, char* name, size_t nameLength) {
    Argparse_Option* option = argparse_find_option_with_name_n(pack->command, name, nameLength);
    if (option == NULL) return NULL;

    // The command accepts such argument, check if we already added it, if so, return that
    for (size_t i = 0; i < pack->arguments.length; ++i) {
        if (pack->arguments.elements[i].option == option) {
            return &pack->arguments.elements[i];
        }
    }
    // No match, we need to add a new argument for this option
    Argparse_Argument argument = {
        .option = option,
        .values = { 0 },
    };
    argparse_add_argument(pack, argument);
    return &pack->arguments.elements[pack->arguments.length - 1];
}

static Argparse_Argument* argparse_try_add_option_argument(Argparse_Pack* pack, char* name, size_t nameLength) {
    // First, try a full option name match
    Argparse_Argument* argument = argparse_try_get_or_add_option_by_name(pack, name, nameLength);
    if (argument != NULL) return argument;

    // Try bundling
    if (argparse_is_legal_prefix_for_bundling(name, nameLength)) {
        char nameBuffer[2];
        nameBuffer[0] = name[0];
        // Check, if all characters correspond to a short name
        for (size_t i = 1; i < nameLength; ++i) {
            nameBuffer[1] = name[i];
            // NOTE: We don't add here yet, we first need to confirm a legal bundle
            Argparse_Option* option = argparse_find_option_with_name_n(pack, nameBuffer, 2);
            if (option == NULL) {
                // Illegal bundling, some character did not correspond to a short name
                return NULL;
            }
        }
        // Legal, add all arguments for the bundled options
        for (size_t i = 1; i < nameLength; ++i) {
            nameBuffer[1] = name[i];
            argument = argparse_try_get_or_add_option_by_name(pack, nameBuffer, 2);
            // We know this won't be NULL since we checked above
            ARGPARSE_ASSERT(argument != NULL, "unexpectedly failed to get argument for bundled option");
        }
        // We return the last one to allow adding values to it
        return argument;
    }

    // No luck
    return NULL;
}

static void argparse_parse_value_to_argument(Argparse_Pack* pack, Argparse_Argument* argument, char const* value, size_t valueLength) {
    // If there is a parser function, invoke it
    void* resultValue = NULL;
    if (argument->option->parse_fn == NULL) {
        // Paste the raw text as the value
        char* valueCopy = (char*)ARGPARSE_REALLOC(NULL, valueLength + 1);
        ARGPARSE_ASSERT(valueCopy != NULL, "failed to allocate memory for option value");
        memcpy(valueCopy, value, valueLength);
        valueCopy[valueLength] = '\0';
        resultValue = valueCopy;
    }
    else {
        // Use the specified parser function
        Argparse_ParseResult parseResult = argument->option->parse_fn(value, valueLength);
        if (parseResult.error != NULL) {
            // Parsing failed, report error
            // This error is already expected to be heap-allocated, so we can just add it directly
            argparse_add_error(pack, parseResult.error);
            return;
        }
        resultValue = parseResult.value;
    }
    // Add the value to the argument
    argparse_add_value_to_argument(argument, resultValue);
}

static Argparse_Argument* argparse_get_current_positional_argument_for_value(Argparse_Pack* pack) {
    // Look through the positional arguments in order they are declared in the command
    for (size_t optionArgIndex = 0; optionArgIndex < pack->command->options.length; ++optionArgIndex) {
        Argparse_Option* option = &pack->command->options.elements[optionArgIndex];
        if (!argparse_is_positional_option(option)) continue;

        // This is a positional argument, look for the corresponding argument in the pack
        Argparse_Argument* argument = NULL;
        for (size_t i = 0; i < pack->arguments.length; ++i) {
            if (pack->arguments.elements[i].option == option) {
                argument = &pack->arguments.elements[i];
                break;
            }
        }
        if (argument == NULL) {
            // We want this argument next, but it hasn't been added to the pack yet, so we add it
            Argparse_Argument newArgument = {
                .option = option,
                .values = { 0 },
            };
            argparse_add_argument(pack, newArgument);
            return &pack->arguments.elements[pack->arguments.length - 1];
        }
        // This argument is already present, check if it can take more values
        if (argparse_argument_can_take_value(argument)) {
            return argument;
        }
        // This argument cannot take more values, continue to the next one
    }

    return NULL;
}

// Public API //////////////////////////////////////////////////////////////////

Argparse_Pack argparse_parse(int argc, char** argv, Argparse_Command* root) {
    Argparse_Pack pack = { 0 };
    pack.command = root;

    if (argc == 0) {
        // NOTE: We call argparse_format to move the error to the heap, we expect errors to be freeable
        char* error = argparse_format("no arguments provided");
        argparse_add_error(&pack, error);
        return pack;
    }

    pack.program_name = argv[0];
    Argparse_Tokenizer tokenizer = {
        .argc = argc,
        .argv = argv,
        .argvIndex = 1,
        .currentResponse = NULL,
        .currentToken = { 0 },
    };

    bool allowSubcommands = true;
    bool allowOptions = true;
    Argparse_Argument* currentArgument = NULL;
    char const* tokenText;
    size_t tokenLength;
    bool endsInValueDelimiter = false;
    bool prevExpectsValue = false;
    while (argparse_tokenizer_next(&pack, &tokenizer, &tokenText, &tokenLength, &endsInValueDelimiter)) {
        if (endsInValueDelimiter) {
            ARGPARSE_ASSERT(!prevExpectsValue, "cannot have two consecutive tokens that end with value delimiters");
            // A value specification bans subcommands
            allowSubcommands = false;
            // If we have already banned options, this is illegal
            if (!allowOptions) {
                char* error = argparse_format("unexpected option value '%.*s' after option escape", (int)tokenLength, tokenText);
                argparse_add_error(&pack, error);
                continue;
            }
            currentArgument = argparse_try_add_option_argument(&pack, tokenText, tokenLength);
            if (currentArgument == NULL) {
                char* error = argparse_format("unknown option '%.*s'", (int)tokenLength, tokenText);
                argparse_add_error(&pack, error);
                // NOTE: We allow fallthrough here, we still parse a value to match intent closer
            }
            prevExpectsValue = true;
            continue;
        }
        if (prevExpectsValue) {
            // Has to be a value for prev. option
            if (currentArgument == NULL) {
                // NOTE: We just throw it away, error should have been reported
                ARGPARSE_ASSERT(pack->errors.length > 0, "an error was expected to be reported for throwaway value");
                continue;
            }
            else {
                argparse_parse_value_to_argument(&pack, currentArgument, tokenText, tokenLength);
            }
            currentArgument = NULL;
            prevExpectsValue = false;
            continue;
        }
        // Check for option escape (double-dash)
        if (allowOptions && tokenLength == 2 && tokenText[0] == '-' && tokenText[1] == '-') {
            allowSubcommands = false;
            allowOptions = false;
            currentArgument = NULL;
            continue;
        }
        // Subcommands take priority
        if (allowSubcommands) {
            ARGPARSE_ASSERT(currentArgument == NULL, "cannot have a subcommand token after an option value");
            // Try to look up a subcommand first
            Argparse_Command* sub = argparse_find_subcommand_with_name_n(pack.command, tokenText, tokenLength);
            if (sub != NULL) {
                // Step down, continue
                pack.command = sub;
                continue;
            }
            // From here on out, subcommands are not allowed anymore, we have to parse options or arguments
            allowSubcommands = false;
        }
        // Try to parse as option
        if (allowOptions && argparse_is_legal_prefix_for_option(tokenText, tokenLength)) {
            currentArgument = argparse_try_add_option_argument(&pack, tokenText, tokenLength);
            if (currentArgument == NULL) {
                char* error = argparse_format("unknown option '%.*s'", (int)tokenLength, tokenText);
                argparse_add_error(&pack, error);
            }
            continue;
        }
        if (argparse_argument_can_take_value(currentArgument)) {
            argparse_parse_value_to_argument(&pack, currentArgument, tokenText, tokenLength);
            continue;
        }
        // If the current argument can take more values, parse into that
        if (argparse_argument_can_take_value(currentArgument)) {
            argparse_parse_value_to_argument(&pack, currentArgument, tokenText, tokenLength);
            continue;
        }
        // We have exhausted our options, look for the first positional argument that can take a value
        currentArgument = argparse_get_current_positional_argument_for_value(&pack);
        if (currentArgument != NULL) {
            argparse_parse_value_to_argument(&pack, currentArgument, tokenText, tokenLength);
            continue;
        }
        // No argument could take this value, report error
        char* error = argparse_format("unexpected argument '%.*s'", (int)tokenLength, tokenText);
        argparse_add_error(&pack, error);
    }

    return pack;
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
