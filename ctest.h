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
    bool executed;
    bool succeeded;
} TestCase;

#define CTEST_CASE(name) \
static void name(TestCase* __ctest_ctx); \
CTEST_TEST_CASE_ATTRIB TestCase name ## _ctest_case = { #name, name }; \
void name(TestCase* __ctest_ctx)

#define CTEST_ASSERT_FAIL(message) \
do { __ctest_ctx->succeeded = false; } while (false)

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
CTEST_DEF void ctest_run_case(TestCase* testCase);

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
        ctest_run_case(testCase);
    }
}

void ctest_run_case(TestCase* testCase) {
    testCase->succeeded = true;
    testCase->executed = true;
    testCase->test_fn();
}

#ifdef CTEST_SELF_TEST

#include <string.h>

typedef struct ExpectedTestCase {
    size_t runCount;
    bool shouldSucceed;
    void(*test_fn)();
    char const* name;
} ExpectedTestCase;

extern ExpectedTestCase expectedCases[];

CTEST_CASE(case1) {
    ++expectedCases[0].runCount;
}

CTEST_CASE(case2) {
    ++expectedCases[1].runCount;
    CTEST_ASSERT_FAIL("custom fail message");
}

CTEST_CASE(case3) {
    ++expectedCases[2].runCount;
}

ExpectedTestCase expectedCases[] = {
#define EXPECTED_CASE(n, s) (ExpectedTestCase) { .runCount = 0, .shouldSucceed = s, .name = #n, .test_fn = n }
    EXPECTED_CASE(case1, true),
    EXPECTED_CASE(case2, false),
    EXPECTED_CASE(case3, true),
#undef EXPECTED_CASE
};

TestCase* find_test_case_by_function(void(*testFn)()) {
    for (TestCase* c = CTEST_TEST_CASES_START; c != CTEST_TEST_CASES_END; ++c) {
        if (c->test_fn == testFn) return c;
    }
    return NULL;
}

int main(void) {
    puts("Running CTEST self-test...");
    
    // Assert number of cases
    const size_t expectedCaseCount = sizeof(expectedCases) / sizeof(ExpectedTestCase);
    size_t gotCaseCount = (CTEST_TEST_CASES_END - CTEST_TEST_CASES_START);
    if (gotCaseCount != expectedCaseCount) {
        printf("Expected %llu cases, but found %llu\n", expectedCaseCount, gotCaseCount);
        return 1;
    }
    
    // Assert initial test case contents
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
        // Look for the expected case in the test suite
        TestCase* gotCase = find_test_case_by_function(expectedCase->test_fn);
        if (gotCase == NULL) {
            printf("iteration did not yield test case %s\n", expectedCase->name);
            return 1;
        }
        // Check initial data
        if (strcmp(expectedCase->name, gotCase->name) != 0) {
            printf("test case %s has wrong name (%s)\n", expectedCase->name, gotCase->name);
            return 1;
        }
    }

    ctest_run_all();

    // Assert that each test ran exactly once and that they have the appropriate fail state
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
        TestCase* gotCase = find_test_case_by_function(expectedCase->test_fn);
        if (expectedCase->runCount != 1) {
            printf("expected test case to run exactly once, but was run %llu time(s)\n", expectedCase->runCount);
            return 1;
        }
        if (expectedCase->shouldSucceed != gotCase->succeeded) {
            char const* expectedStateName = expectedCase->shouldSucceed ? "succeed" : "fail";
            char const* gotStateName = expectedCase->shouldSucceed ? "failed" : "succeeded";

            printf("expected test case %s to %s, but it %s\n", expectedCase->name, expectedStateName, gotStateName);
            return 1;
        }
    }

    puts("Self-test completed successfully!");
    return 0;
}

#endif /* CTEST_SELF_TEST */

#ifdef __cplusplus
}
#endif

#endif /* CTEST_IMPLEMENTATION */
