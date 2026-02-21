#define ARGPARSE_IMPLEMENTATION
#define ARGPARSE_STATIC
#include "../src/argparse.h"

#define STRING_BUILDER_IMPLEMENTATION
#define STRING_BUILDER_STATIC
#include "../src/string_builder.h"

#define QUOTE(...) #__VA_ARGS__

static int generate_template(Argparse_Pack* pack) {
    char const* libName = (char const*)argparse_get_argument(pack, "--name")->values.elements[0];
    CodeBuilder cb = { 0 };
    code_builder_format(&cb, ""
"/**\n"
" * %s.h is a single-header DESCRIPTION.\n"
" *\n"
" * Configuration:\n"
" *  - TODO\n"
" *\n"
" * API:\n"
" *  - TODO\n"
" *\n"
" * Check the example section at the end of this file for a full example.\n"
" */\n"
"\n"
"////////////////////////////////////////////////////////////////////////////////\n"
"// Declaration section                                                        //\n"
"////////////////////////////////////////////////////////////////////////////////\n"
"#ifndef %s_H\n"
"#define %s_H\n"
"\n"
"#ifdef %s_STATIC\n"
"    #define %s_DEF static\n"
"#else\n"
"    #define %s_DEF extern\n"
"#endif\n"
"\n"
"#ifndef %s_REALLOC\n"
"    #define %s_REALLOC realloc\n"
"#endif\n"
"#ifndef %s_FREE\n"
"    #define %s_FREE free\n"
"#endif\n"
"\n"
"#ifdef __cplusplus\n"
"extern \"C\" {\n"
"#endif\n"
"\n",
"// TODO: APU declarations go here\n"
"", libName, libName, libName, libName, libName, libName, libName, libName, libName, libName);
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
