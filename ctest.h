/**
 * ctest.h is a single-header C testing framework.
 *
 * Useful for embedding for tools that need to emit code but also require user-facing templating.
 *
 * Before #include-ing this header, #define CTEST_IMPLEMENTATION in the file you want to paste the implementation.
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
#ifndef CTEST_H
#define CTEST_H

#include <stdbool.h>

#ifdef CTEST_STATIC
#   define CTEST_DEF static
#else
#   define CTEST_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TestCase {
    char const* name;
    void(*test_fn)();
    bool failed;
} TestCase;

#define CTEST_CASE(name) \
static void name(TestCase* __ctest_ctx); \
CTEST_TEST_CASE_ATTRIB TestCase name ## _ctest_case = { #name, name }; \
void name(TestCase* __ctest_ctx)

#define CTEST_ASSERT_FAIL(message) \
do { __ctest_ctx->failed = true; } while (false)

#define CTEST_ASSERT_TRUE(...) \
do { if (!(__VA_ARGS__)) CTEST_ASSERT_FAIL("the condition " #__VA_ARGS__ "was expected to be true, but was false"); } while (false)

#if defined(__GNUC__)

    #if defined(_WIN32)

        #define CTEST_TEST_CASE_ATTRIB __attribute__((used, section("ctest_test_methods$2cases"), aligned(sizeof(void*))))
        #define CTEST_TEST_CASES_START (&__ctest_test_start_sentinel + 1)
        #define CTEST_TEST_CASES_END &__ctest_test_end_sentinel

extern TestCase __start_ctest_test_methods[];
extern TestCase __stop_ctest_test_methods[];

    #else

        #define CTEST_TEST_CASE_ATTRIB __attribute__((used, section("ctest_test_methods"), aligned(sizeof(void*))))
        #define CTEST_TEST_CASES_START __start_ctest_test_methods
        #define CTEST_TEST_CASES_END __stop_ctest_test_methods

extern TestCase __start_ctest_test_methods[];
extern TestCase __stop_ctest_test_methods[];

    #endif

#elif defined(_MSC_VER)

    #define CTEST_TEST_CASE_ATTRIB __declspec(allocate("ctest_test_methods$2cases"))
    #define CTEST_TEST_CASES_START (&__ctest_test_start_sentinel + 1)
    #define CTEST_TEST_CASES_END (&__ctest_test_end_sentinel)

extern TestCase __ctest_test_start_sentinel;
extern TestCase __ctest_test_end_sentinel;

#else
    #error "unsupported C compiler"
#endif

// TODO: Better API
CTEST_DEF void ctest_run_all();
CTEST_DEF void ctest_init_case(TestCase* testCase);

#ifdef __cplusplus
}
#endif

#endif /* CTEST_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef CTEST_IMPLEMENTATION

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__)
    #if defined(_WIN32)

__attribute__((section("ctest_test_methods$1start"), aligned(sizeof(void*))))
TestCase __ctest_test_start_sentinel;

__attribute__((section("ctest_test_methods$3end"), aligned(sizeof(void*))))
TestCase __ctest_test_end_sentinel;

    #endif
#elif defined(_MSC_VER)

    #pragma section("ctest_test_methods$1start", read)
    #pragma section("ctest_test_methods$2cases", read)
    #pragma section("ctest_test_methods$3end", read)
    #pragma comment(linker, "/include:__ctest_test_start_sentinel")

__declspec(allocate("ctest_test_methods$1start"))
TestCase __ctest_test_start_sentinel = { 0 };

__declspec(allocate("ctest_test_methods$3end"))
TestCase __ctest_test_end_sentinel = { 0 };

#else
    #error "unsupported C compiler"
#endif

void ctest_run_all() {
    for (TestCase* testCase = CTEST_TEST_CASES_START; testCase < CTEST_TEST_CASES_END; ++testCase) {
        printf("Running %s... ", testCase->name);
        ctest_init_case(testCase);
        testCase->test_fn();
        if (testCase->failed) {
            printf("Failed!\n");
        }
        else {
            printf("Ok!\n");
        }
    }
}

void ctest_init_case(TestCase* testCase) {
    testCase->failed = false;
}

#ifdef __cplusplus
}
#endif

#endif /* CTEST_IMPLEMENTATION */
