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
 *  - #define ARGPARSE_IMPLEMENTATION before including this header in exactly one source file to include the implementation section
 *  - #define ARGPARSE_STATIC before including this header to make all functions have internal linkage
 *  - #define ARGPARSE_REALLOC and ARGPARSE_FREE to use custom memory allocation functions (by default they use realloc and free from the C standard library)
 *  - #define ARGPARSE_SELF_TEST before including this header to compile a self-test that verifies the library's functionality
 *  - #define ARGPARSE_EXAMPLE before including this header to compile a simple example that demonstrates how to use the library
 *
 * API:
 *  - Define commands and options using the Argparse_Command and Argparse_Option structs, including custom parsing functions and default value functions if needed
 *  - Use argparse_add_option and argparse_add_subcommand to build the command hierarchy as needed
 *  - Call argparse_run to parse the command-line arguments and execute the corresponding handler function
 *  - Alternatively use argparse_parse to get a pack containing the parsed values and any errors, and call handlers manually
 *  - Use argparse_get_argument and argparse_get_positional to retrieve parsed arguments from the pack by name or position
 *  - Use argparse_print_usage to print usage information for a command
 *  - Use argparse_free_pack and argparse_free_command to free the memory associated with packs and commands when they are no longer needed
 *  - Use argparse_format to create formatted strings for error messages
 */

////////////////////////////////////////////////////////////////////////////////
// Declaration section                                                        //
////////////////////////////////////////////////////////////////////////////////
#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef ARGPARSE_STATIC
    #define ARGPARSE_DEF static
#else
    #define ARGPARSE_DEF extern
#endif

#ifndef ARGPARSE_REALLOC
    #define ARGPARSE_REALLOC realloc
#endif
#ifndef ARGPARSE_FREE
    #define ARGPARSE_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct Argparse_Command;
struct Argparse_Argument;
struct Argparse_Option;

/**
 * The result of parsing command-line arguments, containing the parsed values and any errors that were encountered during parsing.
 */
typedef struct Argparse_Pack {
    // The name of the ran program, which is the first argument.
    char const* program_name;

    // The resolved command.
    struct Argparse_Command* command;

    // Owned array of options that were parsed for the command
    struct {
        // The arguments that were parsed for the command.
        struct Argparse_Argument* elements;
        // The number of parsed arguments.
        size_t length;
        // The capacity of the elements array.
        size_t capacity;
    } arguments;

    // Owned list of errors
    struct {
        // The error messages that were produced.
        char const** elements;
        // The number of error messages.
        size_t length;
        // The capacity of the errors array.
        size_t capacity;
    } errors;
} Argparse_Pack;

/**
 * The result of a custom parsing function.
 */
typedef struct Argparse_ParseResult {
    // The value that was parsed, if parsing succeeded.
    // The library will call ARGPARSE_FREE on this value when the pack is freed, so it must be heap-allocated if parsing succeeded.
    void* value;
    // An error message if parsing failed, or NULL if parsing succeeded.
    // The library will call ARGPARSE_FREE on this error message when the pack is freed, so it must be heap-allocated if parsing failed.
    // Consider using @see argparse_format to create error messages for this.
    char const* error;
} Argparse_ParseResult;

/**
 * The arity of an option or argument, indicating how many values it can accept.
 */
typedef enum Argparse_Arity {
    ARGPARSE_ARITY_ZERO,
    ARGPARSE_ARITY_ZERO_OR_ONE,
    ARGPARSE_ARITY_EXACTLY_ONE,
    ARGPARSE_ARITY_ZERO_OR_MORE,
    ARGPARSE_ARITY_ONE_OR_MORE,
} Argparse_Arity;

typedef Argparse_ParseResult Argparse_ParseFn(char const* text, size_t length);
typedef void* Argparse_ValueFn(struct Argparse_Option* option);

/**
 * Describes an option that a command accepts, including its name(s), description, arity, default value and custom parsing function.
 * If both long_name and short_name are NULL, this option is treated as a positional argument,
 * and the order of declaration determines the expected position of the argument.
 */
typedef struct Argparse_Option {
    // The long name of the option including its prefix, for example "--help". Can be NULL.
    char const* long_name;
    // The short name of the option including its prefix, for example "-h". Can be NULL.
    char const* short_name;
    // A description of the option, used for help messages. Can be NULL.
    char const* description;
    // The arity of the option, indicating how many values it can accept.
    Argparse_Arity arity;
    // A custom parsing function for the option's values. If NULL, the raw text will be used as the value.
    Argparse_ParseFn* parse_fn;
    // A function that provides the default value for this option if it is not specified in the command line. Can be NULL if no default value is needed.
    // Note, that the library currently does not call this at all, merely here for future extensibility.
    Argparse_ValueFn* default_value_fn;
} Argparse_Option;

typedef int Argparse_HandlerFn(Argparse_Pack* pack);

/**
 * Describes a command, including its name, description, handler function, options and subcommands.
 */
typedef struct Argparse_Command {
    // The name of the command, used for matching and in help messages.
    char const* name;
    // A description of the command, used for help messages. Can be NULL.
    char const* description;
    // The handler function corresponding to this command. Can be NULL.
    Argparse_HandlerFn* handler_fn;

    struct {
        // The options that this command accepts. This includes both named options and positional arguments.
        Argparse_Option* elements;
        // The number of options.
        size_t length;
        // The capacity of the options array.
        size_t capacity;
    } options;

    struct {
        // The subcommands that this command accepts.
        struct Argparse_Command* elements;
        // The number of subcommands.
        size_t length;
        // The capacity of the subcommands array.
        size_t capacity;
    } subcommands;
} Argparse_Command;

/**
 * A specified option that was parsed from the command line, along with the values that were provided for that option.
 */
typedef struct Argparse_Argument {
    // The option that was parsed. This points to the corresponding option in the command's options array.
    Argparse_Option* option;
    struct {
        // The values that were provided for the option.
        void** elements;
        // The number of values provided.
        size_t length;
        // The capacity of the values array.
        size_t capacity;
    } values;
} Argparse_Argument;

/**
 * Parses the specified command-line arguments according to the specified root command,
 * and executes the corresponding handler function if parsing was successful.
 * If parsing failed, the errors are printed to stderr and the usage of the command is printed.
 * @param argc The number of command-line arguments, including the program name.
 * @param argv The command-line arguments, where argv[0] is the program name.
 * @param root The root command to parse against, which may contain subcommands and options.
 * @returns The return value of the executed handler function if parsing was successful and a handler was executed,
 * or -1 if parsing failed or no handler was executed.
 */
ARGPARSE_DEF int argparse_run(int argc, char** argv, Argparse_Command* root);

/**
 * Prints usage information for the specified command, including its description, options and subcommands.
 * The text is printed to stderr.
 * @param command The command to print usage information for.
 */
ARGPARSE_DEF void argparse_print_usage(Argparse_Command* command);

/**
 * Parses the command-line arguments according to the specified root command,
 * returning a pack containing the parsed values and any errors that were encountered.
 * @param argc The number of command-line arguments, including the program name.
 * @param argv The command-line arguments, where argv[0] is the program name.
 * @param root The root command to parse against, which may contain subcommands and options.
 * @returns A pack containing the parsed values and any errors that were encountered during parsing.
 */
ARGPARSE_DEF Argparse_Pack argparse_parse(int argc, char** argv, Argparse_Command* root);

/**
 * Retrieves the parsed argument corresponding to the specified option name from the pack.
 * @param pack The pack returned by @see argparse_parse.
 * @param name The long or short name of the option to retrieve, including its prefix (e.g. "--help" or "-h").
 * @returns The parsed argument corresponding to the specified option name, or NULL if no such option was parsed.
 */
ARGPARSE_DEF Argparse_Argument* argparse_get_argument(Argparse_Pack* pack, char const* name);

/**
 * Retrieves the parsed argument corresponding to the specified positional argument index from the pack.
 * @param pack The pack returned by @see argparse_parse.
 * @param position The index of the positional argument to retrieve.
 * @returns The parsed argument corresponding to the specified positional argument index, or NULL if no such argument was parsed.
 */
ARGPARSE_DEF Argparse_Argument* argparse_get_positional(Argparse_Pack* pack, size_t position);

/**
 * Frees the memory associated with the pack, including any parsed values and error messages.
 * @param pack The pack to free.
 */
ARGPARSE_DEF void argparse_free_pack(Argparse_Pack* pack);

/**
 * Adds an option to the specified command.
 * @param command The command to which the option will be added.
 * @param option The option to add to the command.
 */
ARGPARSE_DEF void argparse_add_option(Argparse_Command* command, Argparse_Option option);

/**
 * Adds a subcommand to the specified command.
 * @param command The command to which the subcommand will be added.
 * @param subcommand The subcommand to add to the command.
 */
ARGPARSE_DEF void argparse_add_subcommand(Argparse_Command* command, Argparse_Command subcommand);

/**
 * Frees the memory associated with the command, including its options and subcommands.
 * @param command The command to free.
 */
ARGPARSE_DEF void argparse_free_command(Argparse_Command* command);

/**
 * Formats a string using printf-style formatting, returning a heap-allocated string that must be freed by the caller.
 * This is a convenience function with the primary purpose of creating error messages for parser functions.
 * @param format The format string, followed by any additional arguments needed for formatting.
 * @param ... The additional arguments needed for formatting, as specified by the format string.
 * @returns A heap-allocated string containing the formatted result, will be freed by the library using ARGPARSE_FREE.
 */
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
    char* text;
    size_t length;
    size_t index;
    struct Argparse_Response* next;
} Argparse_Response;

typedef struct Argparse_Token {
    char* text;
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
    char* error = NULL;
    ARGPARSE_ASSERT(tokenizer->currentToken.text != NULL, "cannot process current token, no current token present");

    // If we are at the start of the token and it's a response, we need to push it onto the stack
    // Not a response
    if (tokenizer->currentToken.index != 0 || tokenizer->currentToken.text[0] != '@') return false;

    Argparse_Token* token = &tokenizer->currentToken;
    // We interpret the token (minus the @) as a file path
    char* filePath = argparse_format("%.*s", (int)(token->length - 1), token->text + 1);
    // Skip this token to not re-read it when the response is processed
    argparse_tokenizer_skip_current(tokenizer);
    // Open file for reading in binary mode to avoid CRLF translation issues
    FILE* file = fopen(filePath, "rb");
    if (file == NULL) goto io_fail;
    // Get file size
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    if (fileSize < 0) goto io_fail;
    fseek(file, 0, SEEK_SET);
    // Read file content
    char* fileContent = (char*)ARGPARSE_REALLOC(NULL, (size_t)fileSize);
    ARGPARSE_ASSERT(fileContent != NULL, "failed to allocate memory for response file content");
    size_t bytesRead = fread(fileContent, 1, (size_t)fileSize, file);
    fclose(file);
    ARGPARSE_FREE(filePath);
    // Push content as new response
    argparse_tokenizer_push_response(tokenizer, (Argparse_Response){
        .text = fileContent,
        .length = bytesRead,
        .index = 0,
    });
    return true;

io_fail:
    // Report error, we add a dummy response to the stack to allow processing to continue
    error = argparse_format("failed to read response file '%s'", filePath);
    ARGPARSE_FREE(filePath);
    argparse_add_error(pack, error);
    argparse_tokenizer_push_response(tokenizer, (Argparse_Response){
        .text = NULL,
        .length = 0,
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

static Argparse_Argument* argparse_try_get_or_add_option_by_name(Argparse_Pack* pack, char* name, size_t nameLength) {
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
            Argparse_Option* option = argparse_find_option_with_name_n(pack->command, nameBuffer, 2);
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

static void argparse_validate_option_arity(Argparse_Pack* pack, Argparse_Option* option, Argparse_Argument* argument) {
    size_t valueCount = (argument != NULL) ? argument->values.length : 0;
    Argparse_Arity arity = option->arity;
    bool valid = false;
    switch (arity) {
    case ARGPARSE_ARITY_ZERO:
        valid = valueCount == 0;
        break;
    case ARGPARSE_ARITY_ZERO_OR_ONE:
        valid = valueCount <= 1;
        break;
    case ARGPARSE_ARITY_EXACTLY_ONE:
        valid = valueCount == 1;
        break;
    case ARGPARSE_ARITY_ZERO_OR_MORE:
        valid = true;
        break;
    case ARGPARSE_ARITY_ONE_OR_MORE:
        valid = valueCount >= 1;
        break;
    }
    if (!valid) {
        char const* optionName = option->long_name == NULL ? option->short_name : option->long_name;
        char const* expectedAmountDesc =
            (arity == ARGPARSE_ARITY_ZERO) ? "no" :
            (arity == ARGPARSE_ARITY_ZERO_OR_ONE) ? "at most one" :
            (arity == ARGPARSE_ARITY_EXACTLY_ONE) ? "exactly one" :
            (arity == ARGPARSE_ARITY_ZERO_OR_MORE) ? "any number of" :
            (arity == ARGPARSE_ARITY_ONE_OR_MORE) ? "at least one" : "unknown number of";
        char* error = NULL;
        if (optionName == NULL) {
            // Positional argument, we report the index instead of the name
            size_t positionalIndex = 0;
            for (size_t i = 0; i < pack->command->options.length; ++i) {
                if (&pack->command->options.elements[i] == option) {
                    positionalIndex = i + 1;
                    break;
                }
            }
            error = argparse_format("positional argument %zu expects %s value(s), but got %zu", positionalIndex, expectedAmountDesc, valueCount);
        }
        else {
            error = argparse_format("option '%s' expects %s value(s), but got %zu", optionName, expectedAmountDesc, valueCount);
        }
        argparse_add_error(pack, error);
    }
}

// Public API //////////////////////////////////////////////////////////////////

int argparse_run(int argc, char** argv, Argparse_Command* root) {
    Argparse_Pack pack = argparse_parse(argc, argv, root);
    if (pack.errors.length > 0) {
        // Print errors
        for (size_t i = 0; i < pack.errors.length; ++i) {
            fprintf(stderr, "Error: %s\n", pack.errors.elements[i]);
        }
        // Print usage
        argparse_print_usage(root);
        argparse_free_pack(&pack);
        return -1;
    }
    if (pack.command->handler_fn == NULL) {
        fprintf(stderr, "Error: no handler specified for command '%s'\n", pack.command->name);
        argparse_print_usage(root);
        argparse_free_pack(&pack);
        return -1;
    }
    int result = pack.command->handler_fn(&pack);
    argparse_free_pack(&pack);
    return result;
}

void argparse_print_usage(Argparse_Command* command) {
    fprintf(stderr, "Usage: %s", command->name);
    if (command->options.length > 0) {
        fprintf(stderr, " [options]");
    }
    if (command->subcommands.length > 0) {
        fprintf(stderr, " <subcommand>");
    }
    fprintf(stderr, "\n");
    if (command->description != NULL) {
        fprintf(stderr, "%s\n", command->description);
    }
    if (command->options.length > 0) {
        fprintf(stderr, "Options:\n");
        for (size_t i = 0; i < command->options.length; ++i) {
            Argparse_Option* option = &command->options.elements[i];
            char optionNames[128] = { 0 };
            if (option->short_name != NULL) {
                strcat(optionNames, option->short_name);
                if (option->long_name != NULL) {
                    strcat(optionNames, ", ");
                }
            }
            if (option->long_name != NULL) {
                strcat(optionNames, option->long_name);
            }
            fprintf(stderr, "  %-20s %s\n", optionNames, option->description != NULL ? option->description : "");
        }
    }
    if (command->subcommands.length > 0) {
        fprintf(stderr, "Subcommands:\n");
        for (size_t i = 0; i < command->subcommands.length; ++i) {
            Argparse_Command* subcommand = &command->subcommands.elements[i];
            fprintf(stderr, "  %-20s %s\n", subcommand->name, subcommand->description != NULL ? subcommand->description : "");
        }
    }
}

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
    char* tokenText;
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
                ARGPARSE_ASSERT(pack.errors.length > 0, "an error was expected to be reported for throwaway value");
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
            Argparse_Argument* asOption = argparse_try_add_option_argument(&pack, tokenText, tokenLength);
            // If we failed, that's not an error here yet, we can still attempt to parse it as a value
            if (asOption != NULL) {
                // But if we succeeded, save it
                currentArgument = asOption;
                continue;
            }
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

    // Now we need to validate the arity of each option
    for (size_t i = 0; i < pack.command->options.length; ++i) {
        Argparse_Option* option = &pack.command->options.elements[i];
        Argparse_Argument* argument = NULL;
        for (size_t j = 0; j < pack.arguments.length; ++j) {
            if (pack.arguments.elements[j].option == option) {
                argument = &pack.arguments.elements[j];
                break;
            }
        }
        argparse_validate_option_arity(&pack, option, argument);
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

Argparse_Argument* argparse_get_positional(Argparse_Pack* pack, size_t position) {
    size_t positionalIndex = 0;
    for (size_t i = 0; i < pack->command->options.length; ++i) {
        Argparse_Option* option = &pack->command->options.elements[i];
        if (!argparse_is_positional_option(option)) continue;
        if (positionalIndex == position) {
            // Found the right positional index, look for the corresponding argument
            for (size_t j = 0; j < pack->arguments.length; ++j) {
                if (pack->arguments.elements[j].option == option) {
                    return &pack->arguments.elements[j];
                }
            }
            // No argument found, this means this positional was not provided, we return NULL
            return NULL;
        }
        ++positionalIndex;
    }
    // No such positional index
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
    // We null out the pointers to avoid double-free, in case this command is shared in the hierarchy
    command->subcommands.elements = NULL;
    command->options.elements = NULL;
}

char* argparse_format(char const* format, ...) {
    va_list args;
    va_start(args, format);
    int length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    char* buffer = (char*)ARGPARSE_REALLOC(NULL, (size_t)length + 1);
    ARGPARSE_ASSERT(buffer != NULL, "failed to allocate memory for formatted string");
    va_start(args, format);
    vsnprintf(buffer, (size_t)length + 1, format, args);
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
#include <string.h>

// Use our own test framework
#define CTEST_STATIC
#define CTEST_IMPLEMENTATION
#define CTEST_MAIN
#include "ctest.h"

// Helper macros for common assertions
#define ASSERT_NO_ERRORS(pack) CTEST_ASSERT_TRUE((pack).errors.length == 0)
#define ASSERT_HAS_ERRORS(pack) CTEST_ASSERT_TRUE((pack).errors.length > 0)
#define ASSERT_ERROR_COUNT(pack, n) CTEST_ASSERT_TRUE((pack).errors.length == (n))

// Custom parse function for integers
static Argparse_ParseResult parse_int(char const* text, size_t length) {
    // Check for optional leading minus sign
    size_t start = 0;
    bool negate = false;
    if (length > 0 && text[0] == '-') {
        start = 1;
        negate = true;
    }
    // Check remaining characters are digits
    for (size_t i = start; i < length; ++i) {
        if (!isdigit((unsigned char)text[i])) {
            return (Argparse_ParseResult){
                .value = NULL,
                .error = argparse_format("expected integer, got '%.*s'", (int)length, text),
            };
        }
    }
    // Parse and return
    int* value = (int*)malloc(sizeof(int));
    // Simple string to int conversion
    *value = 0;
    for (size_t i = start; i < length; ++i) {
        *value = *value * 10 + (text[i] - '0');
    }
    if (negate) *value = -*value;
    return (Argparse_ParseResult){
        .value = value,
        .error = NULL,
    };
}

// Helper to get first value as string from an argument
static char const* get_string_value(Argparse_Pack* pack, char const* name) {
    Argparse_Argument* arg = argparse_get_argument(pack, name);
    if (arg == NULL || arg->values.length == 0) return NULL;
    return (char const*)arg->values.elements[0];
}

// Helper to get first value as int from an argument
static int get_int_value(Argparse_Pack* pack, char const* name) {
    Argparse_Argument* arg = argparse_get_argument(pack, name);
    if (arg == NULL || arg->values.length == 0) return 0;
    return *(int*)arg->values.elements[0];
}

// Helper to check if an option was specified (for zero-arity flags)
static bool has_option(Argparse_Pack* pack, char const* name) {
    return argparse_get_argument(pack, name) != NULL;
}

// Helper to get first value as string from a positional argument
static char const* get_positional_string(Argparse_Pack* pack, size_t position) {
    Argparse_Argument* arg = argparse_get_positional(pack, position);
    if (arg == NULL || arg->values.length == 0) return NULL;
    return (char const*)arg->values.elements[0];
}

// Basic parsing tests /////////////////////////////////////////////////////////

CTEST_CASE(parse_empty_argc_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    Argparse_Pack pack = argparse_parse(0, NULL, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
}

CTEST_CASE(parse_program_name_only_succeeds) {
    Argparse_Command cmd = { .name = "test" };
    char* argv[] = { "program" };
    Argparse_Pack pack = argparse_parse(1, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(pack.program_name, "program") == 0);

    argparse_free_pack(&pack);
}

// Option long name tests //////////////////////////////////////////////////////

CTEST_CASE(parse_long_option_with_space_delimiter) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--name", "value" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--name"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_long_option_with_equals_delimiter) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--name=value" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--name"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_long_option_with_colon_delimiter) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--name:value" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--name"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Option short name tests /////////////////////////////////////////////////////

CTEST_CASE(parse_short_option_with_space_delimiter) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .short_name = "-n",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "-n", "value" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "-n"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_short_option_with_equals_delimiter) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .short_name = "-n",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "-n=value" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "-n"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_option_by_either_name) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--verbose",
        .short_name = "-v",
        .arity = ARGPARSE_ARITY_ZERO,
    });

    char* argv[] = { "program", "-v" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "--verbose"));
    CTEST_ASSERT_TRUE(has_option(&pack, "-v"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Option bundling tests ///////////////////////////////////////////////////////

CTEST_CASE(parse_bundled_short_options) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-a", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-b", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-c", .arity = ARGPARSE_ARITY_ZERO });

    char* argv[] = { "program", "-abc" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "-a"));
    CTEST_ASSERT_TRUE(has_option(&pack, "-b"));
    CTEST_ASSERT_TRUE(has_option(&pack, "-c"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_bundled_options_last_takes_value) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-a", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-b", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-c", .arity = ARGPARSE_ARITY_EXACTLY_ONE });

    char* argv[] = { "program", "-abc", "value" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "-a"));
    CTEST_ASSERT_TRUE(has_option(&pack, "-b"));
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "-c"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_invalid_bundle_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-a", .arity = ARGPARSE_ARITY_ZERO });
    // -x is not defined

    char* argv[] = { "program", "-ax" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Arity tests /////////////////////////////////////////////////////////////////

CTEST_CASE(arity_zero_accepts_no_value) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--flag",
        .arity = ARGPARSE_ARITY_ZERO,
    });

    char* argv[] = { "program", "--flag" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "--flag"));
    Argparse_Argument* arg = argparse_get_argument(&pack, "--flag");
    CTEST_ASSERT_TRUE(arg->values.length == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(arity_exactly_one_missing_value_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--name" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(arity_zero_or_one_accepts_zero) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--opt",
        .arity = ARGPARSE_ARITY_ZERO_OR_ONE,
    });

    char* argv[] = { "program", "--opt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "--opt"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(arity_zero_or_one_accepts_one) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--opt",
        .arity = ARGPARSE_ARITY_ZERO_OR_ONE,
    });

    char* argv[] = { "program", "--opt", "value" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--opt"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(arity_one_or_more_missing_value_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--items",
        .arity = ARGPARSE_ARITY_ONE_OR_MORE,
    });

    char* argv[] = { "program", "--items" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(arity_one_or_more_accepts_multiple) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--items",
        .arity = ARGPARSE_ARITY_ONE_OR_MORE,
    });

    char* argv[] = { "program", "--items", "a", "b", "c" };
    Argparse_Pack pack = argparse_parse(5, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    Argparse_Argument* arg = argparse_get_argument(&pack, "--items");
    CTEST_ASSERT_TRUE(arg != NULL);
    CTEST_ASSERT_TRUE(arg->values.length == 3);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(arity_zero_or_more_accepts_zero) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--items",
        .arity = ARGPARSE_ARITY_ZERO_OR_MORE,
    });

    char* argv[] = { "program", "--items" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Double-dash escape tests ////////////////////////////////////////////////////

CTEST_CASE(double_dash_treats_remaining_as_positional) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--flag",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    // Positional argument
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = NULL,
        .short_name = NULL,
        .arity = ARGPARSE_ARITY_ZERO_OR_MORE,
    });

    char* argv[] = { "program", "--", "--flag" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    // --flag should NOT be parsed as option, but as positional
    CTEST_ASSERT_TRUE(!has_option(&pack, "--flag"));
    // Verify it was captured as a positional value
    CTEST_ASSERT_TRUE(strcmp(get_positional_string(&pack, 0), "--flag") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(double_dash_allows_dash_prefixed_values) {
    Argparse_Command cmd = { .name = "test" };
    // Positional argument
    argparse_add_option(&cmd, (Argparse_Option){
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--", "-negative" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_positional_string(&pack, 0), "-negative") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Positional argument tests ///////////////////////////////////////////////////

CTEST_CASE(parse_single_positional_argument) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .description = "input file",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "file.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_positional_string(&pack, 0), "file.txt") == 0);
    CTEST_ASSERT_TRUE(argparse_get_positional(&pack, 1) == NULL);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_multiple_positional_arguments) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .description = "source",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .description = "destination",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "src.txt", "dst.txt" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_positional_string(&pack, 0), "src.txt") == 0);
    CTEST_ASSERT_TRUE(strcmp(get_positional_string(&pack, 1), "dst.txt") == 0);
    CTEST_ASSERT_TRUE(argparse_get_positional(&pack, 2) == NULL);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(mixed_options_and_positional) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--verbose",
        .short_name = "-v",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .description = "file",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "-v", "file.txt" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "-v"));
    CTEST_ASSERT_TRUE(strcmp(get_positional_string(&pack, 0), "file.txt") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Subcommand tests ////////////////////////////////////////////////////////////

CTEST_CASE(parse_subcommand) {
    Argparse_Command cmd = { .name = "git" };
    Argparse_Command commitCmd = { .name = "commit" };
    argparse_add_option(&commitCmd, (Argparse_Option){
        .long_name = "--message",
        .short_name = "-m",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_subcommand(&cmd, commitCmd);

    char* argv[] = { "git", "commit", "-m", "Initial commit" };
    Argparse_Pack pack = argparse_parse(4, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(pack.command->name, "commit") == 0);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "-m"), "Initial commit") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_nested_subcommands) {
    Argparse_Command root = { .name = "tool" };
    Argparse_Command sub1 = { .name = "remote" };
    Argparse_Command sub2 = { .name = "add" };
    argparse_add_option(&sub2, (Argparse_Option){
        .description = "name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_subcommand(&sub1, sub2);
    argparse_add_subcommand(&root, sub1);

    char* argv[] = { "tool", "remote", "add", "origin" };
    Argparse_Pack pack = argparse_parse(4, argv, &root);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(pack.command->name, "add") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&root);
}

CTEST_CASE(unknown_subcommand_treated_as_positional) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_subcommand(&cmd, (Argparse_Command){ .name = "run" });
    argparse_add_option(&cmd, (Argparse_Option){
        .description = "args",
        .arity = ARGPARSE_ARITY_ZERO_OR_MORE,
    });

    char* argv[] = { "test", "unknown" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    // Should stay at root command since "unknown" is not a subcommand
    CTEST_ASSERT_TRUE(strcmp(pack.command->name, "test") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Custom parse function tests /////////////////////////////////////////////////

CTEST_CASE(custom_parse_function_success) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--count",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });

    char* argv[] = { "program", "--count", "42" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(get_int_value(&pack, "--count") == 42);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(custom_parse_function_failure_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--count",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });

    char* argv[] = { "program", "--count", "not_a_number" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(custom_parse_negative_integer) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--offset",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });

    char* argv[] = { "program", "--offset", "-10" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(get_int_value(&pack, "--offset") == -10);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Error handling tests ////////////////////////////////////////////////////////

CTEST_CASE(unknown_option_reports_error) {
    Argparse_Command cmd = { .name = "test" };

    char* argv[] = { "program", "--unknown" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
}

CTEST_CASE(unexpected_argument_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    // No positional arguments defined

    char* argv[] = { "program", "unexpected" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
}

CTEST_CASE(option_after_escape_reports_error) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--opt",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--", "--opt=value" };
    Argparse_Pack pack = argparse_parse(3, argv, &cmd);

    // Should error because --opt=value is treated as positional after --
    // and no positional arguments are defined
    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Slash prefix tests (Windows-style) //////////////////////////////////////////

CTEST_CASE(parse_slash_prefixed_option) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "/help",
        .arity = ARGPARSE_ARITY_ZERO,
    });

    char* argv[] = { "program", "/help" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "/help"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(parse_slash_prefixed_bundled_options) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "/a", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "/b", .arity = ARGPARSE_ARITY_ZERO });

    char* argv[] = { "program", "/ab" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "/a"));
    CTEST_ASSERT_TRUE(has_option(&pack, "/b"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Quoted value tests (in response files context, tokenizer-level) /////////////

CTEST_CASE(parse_value_with_spaces_via_equals) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--message",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    // Shell would normally handle quoting, but with = the value is part of the same argv
    char* argv[] = { "program", "--message=hello world" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--message"), "hello world") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

// Response file tests /////////////////////////////////////////////////////////

CTEST_CASE(response_file_basic) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "@test_inputs/argparse/basic_args.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--name"), "value") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_multiple_options) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--verbose",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--count",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--output",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "@test_inputs/argparse/multiple_args.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "--verbose"));
    CTEST_ASSERT_TRUE(get_int_value(&pack, "--count") == 42);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--output"), "result.txt") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_quoted_values) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--message",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--path",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "@test_inputs/argparse/quoted_args.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--message"), "hello world") == 0);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--path"), "C:\\Program Files\\App") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_nested) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--first",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--middle",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--last",
        .arity = ARGPARSE_ARITY_ZERO,
    });

    char* argv[] = { "program", "@test_inputs/argparse/nested_outer.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "--first"));
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--middle"), "value") == 0);
    CTEST_ASSERT_TRUE(has_option(&pack, "--last"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_with_equals_delimiters) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--name",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--count",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });

    char* argv[] = { "program", "@test_inputs/argparse/equals_delimiter.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--name"), "john") == 0);
    CTEST_ASSERT_TRUE(get_int_value(&pack, "--count") == 99);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_bundled_options) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-a", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-b", .arity = ARGPARSE_ARITY_ZERO });
    argparse_add_option(&cmd, (Argparse_Option){ .short_name = "-c", .arity = ARGPARSE_ARITY_ZERO });

    char* argv[] = { "program", "@test_inputs/argparse/bundled_options.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(has_option(&pack, "-a"));
    CTEST_ASSERT_TRUE(has_option(&pack, "-b"));
    CTEST_ASSERT_TRUE(has_option(&pack, "-c"));

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_mixed_with_argv) {
    Argparse_Command cmd = { .name = "test" };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--first",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--second",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--third",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "program", "--first", "one", "@test_inputs/argparse/mixed_with_argv.txt", "--third", "three" };
    Argparse_Pack pack = argparse_parse(6, argv, &cmd);

    ASSERT_NO_ERRORS(pack);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--first"), "one") == 0);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--second"), "two") == 0);
    CTEST_ASSERT_TRUE(strcmp(get_string_value(&pack, "--third"), "three") == 0);

    argparse_free_pack(&pack);
    argparse_free_command(&cmd);
}

CTEST_CASE(response_file_not_found_reports_error) {
    Argparse_Command cmd = { .name = "test" };

    char* argv[] = { "program", "@test_inputs/argparse/nonexistent.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    ASSERT_HAS_ERRORS(pack);

    argparse_free_pack(&pack);
}

CTEST_CASE(response_file_empty_is_ok) {
    Argparse_Command cmd = { .name = "test" };

    char* argv[] = { "program", "@test_inputs/argparse/empty.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    // Empty response file should not cause errors
    ASSERT_NO_ERRORS(pack);

    argparse_free_pack(&pack);
}

CTEST_CASE(response_file_whitespace_only_is_ok) {
    Argparse_Command cmd = { .name = "test" };

    char* argv[] = { "program", "@test_inputs/argparse/whitespace_only.txt" };
    Argparse_Pack pack = argparse_parse(2, argv, &cmd);

    // Whitespace-only response file should not cause errors
    ASSERT_NO_ERRORS(pack);

    argparse_free_pack(&pack);
}

// argparse_run tests //////////////////////////////////////////////////////////

// Static variables to track handler invocation
static int g_handler_called = 0;
static int g_handler_verbose = 0;
static int g_handler_count = 0;
static char g_handler_output[256] = { 0 };
static char g_handler_subcommand[64] = { 0 };

static int test_main_handler(Argparse_Pack* pack) {
    g_handler_called = 1;

    if (has_option(pack, "--verbose")) {
        g_handler_verbose = 1;
    }

    Argparse_Argument* countArg = argparse_get_argument(pack, "--count");
    if (countArg != NULL && countArg->values.length > 0) {
        g_handler_count = *(int*)countArg->values.elements[0];
    }

    char const* output = get_string_value(pack, "--output");
    if (output != NULL) {
        strncpy(g_handler_output, output, sizeof(g_handler_output) - 1);
    }

    return 0;
}

static int test_build_handler(Argparse_Pack* pack) {
    g_handler_called = 1;
    strncpy(g_handler_subcommand, "build", sizeof(g_handler_subcommand) - 1);

    if (has_option(pack, "--release")) {
        return 100; // Special return code to indicate release build
    }
    return 0;
}

static int test_run_handler(Argparse_Pack* pack) {
    g_handler_called = 1;
    strncpy(g_handler_subcommand, "run", sizeof(g_handler_subcommand) - 1);

    Argparse_Argument* argsArg = argparse_get_argument(pack, NULL); // positional
    if (argsArg != NULL) {
        return (int)argsArg->values.length; // Return count of positional args
    }
    return 0;
}

static void reset_handler_state(void) {
    g_handler_called = 0;
    g_handler_verbose = 0;
    g_handler_count = 0;
    g_handler_output[0] = '\0';
    g_handler_subcommand[0] = '\0';
}

CTEST_CASE(run_invokes_handler_with_parsed_args) {
    reset_handler_state();

    Argparse_Command cmd = { .name = "myapp", .handler_fn = test_main_handler };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--verbose",
        .short_name = "-v",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--count",
        .short_name = "-c",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--output",
        .short_name = "-o",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });

    char* argv[] = { "myapp", "--verbose", "--count", "42", "--output", "result.txt" };
    int result = argparse_run(6, argv, &cmd);

    CTEST_ASSERT_TRUE(result == 0);
    CTEST_ASSERT_TRUE(g_handler_called == 1);
    CTEST_ASSERT_TRUE(g_handler_verbose == 1);
    CTEST_ASSERT_TRUE(g_handler_count == 42);
    CTEST_ASSERT_TRUE(strcmp(g_handler_output, "result.txt") == 0);

    argparse_free_command(&cmd);
}

CTEST_CASE(run_returns_error_on_parse_failure) {
    reset_handler_state();

    Argparse_Command cmd = { .name = "myapp", .handler_fn = test_main_handler };
    argparse_add_option(&cmd, (Argparse_Option){
        .long_name = "--count",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });

    char* argv[] = { "myapp", "--count", "not_a_number" };
    int result = argparse_run(3, argv, &cmd);

    CTEST_ASSERT_TRUE(result == -1);
    CTEST_ASSERT_TRUE(g_handler_called == 0); // Handler should not be called

    argparse_free_command(&cmd);
}

CTEST_CASE(run_returns_error_when_no_handler) {
    reset_handler_state();

    Argparse_Command cmd = { .name = "myapp", .handler_fn = NULL };

    char* argv[] = { "myapp" };
    int result = argparse_run(1, argv, &cmd);

    CTEST_ASSERT_TRUE(result == -1);
    CTEST_ASSERT_TRUE(g_handler_called == 0);
}

CTEST_CASE(run_routes_to_subcommand_handler) {
    reset_handler_state();

    Argparse_Command cmd = { .name = "tool" };
    Argparse_Command buildCmd = { .name = "build", .handler_fn = test_build_handler };
    argparse_add_option(&buildCmd, (Argparse_Option){
        .long_name = "--release",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    Argparse_Command runCmd = { .name = "run", .handler_fn = test_run_handler };
    argparse_add_subcommand(&cmd, buildCmd);
    argparse_add_subcommand(&cmd, runCmd);

    // Test build subcommand
    char* argv1[] = { "tool", "build", "--release" };
    int result1 = argparse_run(3, argv1, &cmd);

    CTEST_ASSERT_TRUE(result1 == 100); // Special release return code
    CTEST_ASSERT_TRUE(g_handler_called == 1);
    CTEST_ASSERT_TRUE(strcmp(g_handler_subcommand, "build") == 0);

    // Test run subcommand
    reset_handler_state();
    char* argv2[] = { "tool", "run" };
    int result2 = argparse_run(2, argv2, &cmd);

    CTEST_ASSERT_TRUE(result2 == 0);
    CTEST_ASSERT_TRUE(g_handler_called == 1);
    CTEST_ASSERT_TRUE(strcmp(g_handler_subcommand, "run") == 0);

    argparse_free_command(&cmd);
}

CTEST_CASE(run_handler_return_value_propagates) {
    reset_handler_state();

    Argparse_Command cmd = { .name = "tool" };
    Argparse_Command buildCmd = { .name = "build", .handler_fn = test_build_handler };
    argparse_add_option(&buildCmd, (Argparse_Option){
        .long_name = "--release",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_subcommand(&cmd, buildCmd);

    // Without --release, returns 0
    char* argv1[] = { "tool", "build" };
    int result1 = argparse_run(2, argv1, &cmd);
    CTEST_ASSERT_TRUE(result1 == 0);

    // With --release, returns 100
    char* argv2[] = { "tool", "build", "--release" };
    int result2 = argparse_run(3, argv2, &cmd);
    CTEST_ASSERT_TRUE(result2 == 100);

    argparse_free_command(&cmd);
}

#endif /* ARGPARSE_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef ARGPARSE_EXAMPLE
#undef ARGPARSE_EXAMPLE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARGPARSE_IMPLEMENTATION
#define ARGPARSE_STATIC
#include "argparse.h"

// Example: A simple "project" CLI tool with subcommands
// Usage:
//   project build [--release] [--output FILE]
//   project run [--verbose] [ARGS...]
//   project clean [--all]

// Custom parser for integers
static Argparse_ParseResult parse_int(char const* text, size_t length) {
    char* temp = (char*)malloc(length + 1);
    memcpy(temp, text, length);
    temp[length] = '\0';
    int* value = (int*)malloc(sizeof(int));
    *value = atoi(temp);
    free(temp);
    return (Argparse_ParseResult){ .value = value, .error = NULL };
}

// Handler for 'build' subcommand
static int handle_build(Argparse_Pack* pack) {
    printf("Building project...\n");

    Argparse_Argument* releaseArg = argparse_get_argument(pack, "--release");
    if (releaseArg != NULL) {
        printf("  Mode: Release\n");
    } else {
        printf("  Mode: Debug\n");
    }

    Argparse_Argument* outputArg = argparse_get_argument(pack, "--output");
    if (outputArg != NULL && outputArg->values.length > 0) {
        printf("  Output: %s\n", (char*)outputArg->values.elements[0]);
    } else {
        printf("  Output: ./a.out\n");
    }

    Argparse_Argument* jobsArg = argparse_get_argument(pack, "--jobs");
    if (jobsArg != NULL && jobsArg->values.length > 0) {
        printf("  Jobs: %d\n", *(int*)jobsArg->values.elements[0]);
    }

    printf("Build complete!\n");
    return 0;
}

// Handler for 'run' subcommand
static int handle_run(Argparse_Pack* pack) {
    printf("Running project...\n");

    Argparse_Argument* verboseArg = argparse_get_argument(pack, "--verbose");
    if (verboseArg != NULL) {
        printf("  Verbose mode enabled\n");
    }

    // Get positional arguments (program args) using argparse_get_positional
    Argparse_Argument* argsArg = argparse_get_positional(pack, 0);
    if (argsArg != NULL && argsArg->values.length > 0) {
        printf("  Program arguments:\n");
        for (size_t j = 0; j < argsArg->values.length; ++j) {
            printf("    [%zu]: %s\n", j, (char*)argsArg->values.elements[j]);
        }
    }

    printf("Run complete!\n");
    return 0;
}

// Handler for 'clean' subcommand
static int handle_clean(Argparse_Pack* pack) {
    Argparse_Argument* allArg = argparse_get_argument(pack, "--all");

    if (allArg != NULL) {
        printf("Cleaning all build artifacts and caches...\n");
    } else {
        printf("Cleaning build artifacts...\n");
    }

    printf("Clean complete!\n");
    return 0;
}

int main(int argc, char** argv) {
    // Build the command hierarchy
    Argparse_Command rootCmd = {
        .name = "project",
        .description = "A simple project management tool",
    };

    // 'build' subcommand
    Argparse_Command buildCmd = {
        .name = "build",
        .description = "Build the project",
        .handler_fn = handle_build,
    };
    argparse_add_option(&buildCmd, (Argparse_Option){
        .long_name = "--release",
        .short_name = "-r",
        .description = "Build in release mode",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_option(&buildCmd, (Argparse_Option){
        .long_name = "--output",
        .short_name = "-o",
        .description = "Output file path",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
    });
    argparse_add_option(&buildCmd, (Argparse_Option){
        .long_name = "--jobs",
        .short_name = "-j",
        .description = "Number of parallel jobs",
        .arity = ARGPARSE_ARITY_EXACTLY_ONE,
        .parse_fn = parse_int,
    });
    argparse_add_subcommand(&rootCmd, buildCmd);

    // 'run' subcommand
    Argparse_Command runCmd = {
        .name = "run",
        .description = "Run the project",
        .handler_fn = handle_run,
    };
    argparse_add_option(&runCmd, (Argparse_Option){
        .long_name = "--verbose",
        .short_name = "-v",
        .description = "Enable verbose output",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    // Positional arguments for the program being run
    argparse_add_option(&runCmd, (Argparse_Option){
        .description = "Arguments to pass to the program",
        .arity = ARGPARSE_ARITY_ZERO_OR_MORE,
    });
    argparse_add_subcommand(&rootCmd, runCmd);

    // 'clean' subcommand
    Argparse_Command cleanCmd = {
        .name = "clean",
        .description = "Clean build artifacts",
        .handler_fn = handle_clean,
    };
    argparse_add_option(&cleanCmd, (Argparse_Option){
        .long_name = "--all",
        .short_name = "-a",
        .description = "Remove all artifacts including caches",
        .arity = ARGPARSE_ARITY_ZERO,
    });
    argparse_add_subcommand(&rootCmd, cleanCmd);

    // Parse and run
    int result = argparse_run(argc, argv, &rootCmd);

    // Cleanup
    argparse_free_command(&rootCmd);

    return result;
}

// Example invocations:
//   project build --release -o myapp
//   project build -j 4
//   project run --verbose -- arg1 arg2 arg3
//   project run -v foo bar baz
//   project clean --all

#endif /* ARGPARSE_EXAMPLE */
