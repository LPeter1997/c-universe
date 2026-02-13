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
#include <stddef.h>
#include <stdlib.h>

#ifdef CTEST_STATIC
#   define CTEST_DEF static
#else
#   define CTEST_DEF extern
#endif

#ifndef CTEST_ALLOC
#   define CTEST_ALLOC malloc
#endif
#ifndef CTEST_REALLOC
#   define CTEST_REALLOC realloc
#endif
#ifndef CTEST_FREE
#   define CTEST_FREE free
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TestExecution TestExecution;

typedef struct TestCase {
    char const* name;
    void(*test_fn)(TestExecution*);
} TestCase;

typedef struct TestSuite {
    TestCase const* test_cases;
    size_t test_cases_length;
} TestSuite;

typedef struct TestExecution {
    TestCase const* test_case;
    bool passed;
} TestExecution;

typedef struct TestFilter {
    bool(*filter_fn)(TestCase const*, void*);
    void* user;
} TestFilter;

typedef struct TestReport {
    TestExecution* passing_cases;
    size_t passing_cases_length;
    size_t passing_cases_capacity;

    TestExecution* failing_cases;
    size_t failing_cases_length;
    size_t failing_cases_capacity;
} TestReport;

#define CTEST_CASE(n) \
static void n(TestExecution* __ctest_ctx); \
CTEST_CASE_ATTRIB TestCase n ## _ctest_case = { .name = #n, .test_fn = n }; \
void n(TestExecution* __ctest_ctx)

#define CTEST_ASSERT_FAIL(message) \
do { __ctest_ctx->passed = false; } while (false)

#define CTEST_ASSERT_TRUE(...) \
do { if (!(__VA_ARGS__)) CTEST_ASSERT_FAIL("the condition " #__VA_ARGS__ " was expected to be true, but was false"); } while (false)

#if defined(__GNUC__)

    #if defined(_WIN32)

        #define CTEST_CASE_ATTRIB __attribute__((used, section("ctest_test_methods$2cases"), aligned(sizeof(void*))))
        #define CTEST_CASES_START (&__ctest_test_start_sentinel + 1)
        #define CTEST_CASES_END &__ctest_test_end_sentinel

extern TestCase __start_ctest_test_methods[];
extern TestCase __stop_ctest_test_methods[];

    #else

        #define CTEST_CASE_ATTRIB __attribute__((used, section("ctest_test_methods"), aligned(sizeof(void*))))
        #define CTEST_CASES_START __start_ctest_test_methods
        #define CTEST_CASES_END __stop_ctest_test_methods

extern TestCase __start_ctest_test_methods[];
extern TestCase __stop_ctest_test_methods[];

    #endif

#elif defined(_MSC_VER)

    #define CTEST_CASE_ATTRIB __declspec(allocate("ctest_test_methods$2cases"))
    #define CTEST_CASES_START (&__ctest_test_start_sentinel + 1)
    #define CTEST_CASES_END (&__ctest_test_end_sentinel)

extern TestCase __ctest_test_start_sentinel;
extern TestCase __ctest_test_end_sentinel;

#else
    #error "unsupported C compiler"
#endif

// TODO: Better API
CTEST_DEF TestSuite ctest_get_suite();
CTEST_DEF TestReport ctest_run(TestSuite suite, TestFilter input);
CTEST_DEF TestExecution ctest_run_case(TestCase const* testCase);

#ifdef __cplusplus
}
#endif

#endif /* CTEST_H */

////////////////////////////////////////////////////////////////////////////////
// Implementation section                                                     //
////////////////////////////////////////////////////////////////////////////////
#ifdef CTEST_IMPLEMENTATION

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define CTEST_ASSERT(condition, message) assert(((void)message, condition))

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

TestSuite ctest_get_suite() {
    size_t caseCount = (CTEST_CASES_END - CTEST_CASES_START);
    TestCase* cases = (TestCase*)CTEST_ALLOC(sizeof(TestCase) * caseCount);
    CTEST_ASSERT(cases != NULL, "failed to allocate memory for test suite");
    for (size_t i = 0; i < caseCount; ++i) {
        cases[i] = CTEST_CASES_START[i];
    }
    return (TestSuite){
        .test_cases = cases,
        .test_cases_length = caseCount,
    };
}

TestReport ctest_run(TestSuite suite, TestFilter input) {
    TestReport report = {
        .passing_cases = NULL,
        .passing_cases_length = 0,
        .passing_cases_capacity = 0,
        .failing_cases = NULL,
        .failing_cases_length = 0,
        .failing_cases_capacity = 0,
    };
    for (size_t i = 0; i < suite.test_cases_length; ++i) {
        TestCase const* testCase = &suite.test_cases[i];

        // Use filter function, if specified
        if (input.filter_fn != NULL && !input.filter_fn(testCase, input.user)) continue;

        TestExecution execution = ctest_run_case(testCase);

        // Add execution to report
        TestExecution** targetList = execution.passed ? &report.passing_cases : &report.failing_cases;
        size_t* targetListLength = execution.passed ? &report.passing_cases_length : &report.failing_cases_length;
        size_t* targetListCapacity = execution.passed ? &report.passing_cases_capacity : &report.failing_cases_capacity;
        if (*targetListLength + 1 > *targetListCapacity) {
            size_t newCapacity = (*targetListCapacity == 0) ? 8 : (*targetListCapacity * 2);
            TestExecution* newList = (TestExecution*)CTEST_REALLOC(*targetList, newCapacity * sizeof(TestExecution));
            CTEST_ASSERT(newList != NULL, "failed to allocate memory for test report");
            *targetList = newList;
            *targetListCapacity = newCapacity;
        }
        (*targetList)[*targetListLength] = execution;
        ++(*targetListLength);
    }
    return report;
}

TestExecution ctest_run_case(TestCase const* testCase) {
    // Create execution context for the test case
    TestExecution execution = {
        .test_case = testCase,
        // By default, tests are passing until an assertion fail happens
        .passed = true,
    };
    // Actually run it
    testCase->test_fn(&execution);
    return execution;
}

#ifdef __cplusplus
}
#endif

#undef CTEST_ASSERT

#endif /* CTEST_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Self-testing section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef CTEST_SELF_TEST

#include <string.h>

typedef struct ExpectedTestCase {
    size_t runCount;
    bool shouldPass;
    void(*test_fn)(TestExecution*);
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
#define EXPECTED_CASE(n, s) (ExpectedTestCase) { .runCount = 0, .shouldPass = s, .name = #n, .test_fn = n }
    EXPECTED_CASE(case1, true),
    EXPECTED_CASE(case2, false),
    EXPECTED_CASE(case3, true),
#undef EXPECTED_CASE
};

TestCase* find_test_case_by_function(void(*testFn)(TestExecution*)) {
    for (TestCase* c = CTEST_CASES_START; c != CTEST_CASES_END; ++c) {
        if (c->test_fn == testFn) return c;
    }
    return NULL;
}

TestExecution* find_test_execution_in_report_by_function(TestReport report, void(*testFn)(TestExecution*)) {
    for (size_t i = 0; i < report.passing_cases_length; ++i) {
        if (report.passing_cases[i].test_case->test_fn == testFn) {
            return &report.passing_cases[i];
        }
    }
    for (size_t i = 0; i < report.failing_cases_length; ++i) {
        if (report.failing_cases[i].test_case->test_fn == testFn) {
            return &report.failing_cases[i];
        }
    }
    return NULL;
}

int main(void) {
    puts("Running CTEST self-test...");

    // Collect suite
    TestSuite suite = ctest_get_suite();

    // Assert number of cases
    const size_t expectedCaseCount = sizeof(expectedCases) / sizeof(ExpectedTestCase);
    size_t gotCaseCount = suite.test_cases_length;

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

    TestReport report = ctest_run(suite, (TestFilter){ .filter_fn = NULL, .user = NULL });

    // Assert that each test ran exactly once and that they have the appropriate fail state
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
        TestCase* gotCase = find_test_case_by_function(expectedCase->test_fn);
        TestExecution* gotExecution = find_test_execution_in_report_by_function(report, expectedCase->test_fn);
        if (expectedCase->runCount != 1) {
            printf("expected test case to run exactly once, but was run %zu time(s)\n", expectedCase->runCount);
            return 1;
        }
        if (gotExecution == NULL) {
            printf("could not find execution in report for test case %s\n", expectedCase->name);
            return 1;
        }
        if (expectedCase->shouldPass != gotExecution->passed) {
            char const* expectedStateName = expectedCase->shouldPass ? "pass" : "fail";
            char const* gotStateName = expectedCase->shouldPass ? "failed" : "passed";

            printf("expected test case %s to %s, but it %s\n", expectedCase->name, expectedStateName, gotStateName);
            return 1;
        }
    }

    puts("Self-test completed successfully!");
    return 0;
}

#endif /* CTEST_SELF_TEST */
