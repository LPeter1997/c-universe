// Common macros for the libraries
// Make sure to #include cleanup.h at the end of the file to remove all macro definitions

// foo
#ifndef LIBRARY_NAME_LOWER
    #error "LIBRARY_NAME_LOWER must be defined before including macros.h"
#endif
// Foo
#ifndef LIBRARY_NAME_CAPITALIZED
    #error "LIBRARY_NAME_CAPITALIZED must be defined before including macros.h"
#endif
// FOO
#ifndef LIBRARY_NAME_UPPER
    #error "LIBRARY_NAME_UPPER must be defined before including macros.h"
#endif

#define LIBRARY_CONCAT_IMPL(a, b) a ## _ ## b
#define LIBRARY_CONCAT(a, b) LIBRARY_CONCAT_IMPL(a, b)

// Used to reference library functions
#define LIBRARY_FUNC(name) LIBRARY_CONCAT(LIBRARY_NAME_LOWER, name)
// Used to reference library types
#define LIBRARY_TYPE(name) LIBRARY_CONCAT(LIBRARY_NAME_CAPITALIZED, name)
// Used to reference library macros
#define LIBRARY_MACRO(name) LIBRARY_CONCAT(LIBRARY_NAME_UPPER, name)

// References the library's assert macro
#define LIBRARY_ASSERT(cond, msg) LIBRARY_MACRO(ASSERT)(cond, msg)
