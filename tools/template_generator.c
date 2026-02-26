#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define ARGPARSE_IMPLEMENTATION
#define ARGPARSE_STATIC
#include "../src/argparse.h"

#define STRING_BUILDER_IMPLEMENTATION
#define STRING_BUILDER_STATIC
#include "../src/string_builder.h"

#define QUOTE(...) #__VA_ARGS__

char* upcase(char const* str) {
    size_t len = strlen(str);
    char* result = (char*)malloc((len + 1) * sizeof(char));
    for (size_t i = 0; i < len; ++i) {
        char c = str[i];
        result[i] = toupper((unsigned char)c);
    }
    result[len] = '\0';
    return result;
}

static void header_comment(CodeBuilder* cb, char const* libName) {
    code_builder_format(cb, ""
        "/**\n"
        " * %s.h is a single-header TODO.\n"
        " *\n"
        " * Configuration:\n"
        " *  - TODO\n"
        " *\n"
        " * API:\n"
        " *  - TODO\n"
        " *\n"
        " * Check the example section at the end of this file for a full example.\n"
        " */\n", libName);
}

static void declaration_section(CodeBuilder* cb, char const* libName) {
    char* upcasedName = upcase(libName);
    code_builder_format(cb, ""
        "////////////////////////////////////////////////////////////////////////////////\n"
        "// Declaration section                                                        //\n"
        "////////////////////////////////////////////////////////////////////////////////\n"
        "#ifndef %s_H\n"
        "#define %s_H\n"
        "\n"
        "#include <stddef.h>\n"
        "\n"
        "#ifdef %s_STATIC\n"
        "    #define %s_DEF static\n"
        "#else\n"
        "    #define %s_DEF extern\n"
        "#endif\n"
        "\n"
        "#ifndef %s_ASSERT\n"
        "    #define %s_ASSERT(condition, message) ((void)message, (condition))\n"
        "#endif\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "// TODO: Public API declarations\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif /* %s_H */\n"
        , upcasedName, upcasedName, upcasedName, upcasedName, upcasedName, upcasedName, upcasedName, upcasedName);
    free(upcasedName);
}

static void implementation_section(CodeBuilder* cb, char const* libName) {
    char* upcasedName = upcase(libName);
    code_builder_format(cb, ""
        "////////////////////////////////////////////////////////////////////////////////\n"
        "// Implementation section                                                     //\n"
        "////////////////////////////////////////////////////////////////////////////////\n"
        "#ifdef %s_IMPLEMENTATION\n"
        "\n"
        "#include <assert.h>\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "// TODO: Implementation goes here\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif /* %s_IMPLEMENTATION */\n"
        , upcasedName, upcasedName);
    free(upcasedName);
}

static void self_test_section(CodeBuilder* cb, char const* libName) {
    char* upcasedName = upcase(libName);
    code_builder_format(cb, ""
        "////////////////////////////////////////////////////////////////////////////////\n"
        "// Self-testing section                                                       //\n"
        "////////////////////////////////////////////////////////////////////////////////\n"
        "#ifdef %s_SELF_TEST\n"
        "\n"
        "// Use our own testing library for self-testing\n"
        "#define CTEST_STATIC\n"
        "#define CTEST_IMPLEMENTATION\n"
        "#define CTEST_MAIN\n"
        "#include \"ctest.h\"\n"
        "\n"
        "CTEST_CASE(sample_test) {\n"
        "    CTEST_ASSERT_FAIL(\"TODO\");\n"
        "}\n"
        "\n"
        "#endif /* %s_SELF_TEST */\n"
        , upcasedName, upcasedName);
    free(upcasedName);
}

static void example_section(CodeBuilder* cb, char const* libName) {
    char* upcasedName = upcase(libName);
    code_builder_format(cb, ""
        "////////////////////////////////////////////////////////////////////////////////\n"
        "// Example section                                                            //\n"
        "////////////////////////////////////////////////////////////////////////////////\n"
        "#ifdef %s_EXAMPLE\n"
        "#undef %s_EXAMPLE\n"
        "\n"
        "#include <stdio.h>\n"
        "\n"
        "#define %s_IMPLEMENTATION\n"
        "#define %s_STATIC\n"
        "#include \"%s.h\"\n"
        "\n"
        "int main(void) {\n"
        "    // TODO: Example usage goes here\n"
        "    return 0;\n"
        "}\n"
        "\n"
        "#endif /* %s_EXAMPLE */\n"
        , upcasedName, upcasedName, upcasedName, upcasedName, libName, upcasedName);
    free(upcasedName);
}

static int generate_template(Argparse_Pack* pack) {
    char const* libName = (char const*)argparse_get_argument(pack, "--name")->values.elements[0];
    CodeBuilder cb = { 0 };
    header_comment(&cb, libName);
    code_builder_putc(&cb, '\n');
    declaration_section(&cb, libName);
    code_builder_putc(&cb, '\n');
    implementation_section(&cb, libName);
    code_builder_putc(&cb, '\n');
    self_test_section(&cb, libName);
    code_builder_putc(&cb, '\n');
    example_section(&cb, libName);
    char* result = code_builder_to_cstr(&cb);
    printf("%s", result);
    free(result);
    code_builder_free(&cb);
    return 0;
}

int main(int argc, char* argv[]) {
    Argparse_Command rootCommand = { .name = "template_generator", .handler_fn = generate_template };
    Argparse_Option nameOption = { .long_name = "--name", .short_name = "-n", .description = "Name of the library", .arity = ARGPARSE_ARITY_EXACTLY_ONE };
    argparse_add_option(&rootCommand, nameOption);
    int result = argparse_run(argc, argv, &rootCommand);
    argparse_free_command(&rootCommand);
    return result;
}
