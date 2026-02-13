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

typedef struct TestExecution TestExecution;

typedef struct TestCase {
    char const* name;
    void(*test_fn)(TestExecution*);
} TestCase;

typedef struct TestExecution {
    TestCase const* test_case;
    bool passed;
} TestExecution;

typedef struct TestExecutionInput {
    bool(*filter_fn)(TestCase const*, void*);
    void* user;
} TestExecutionInput;

typedef struct TestReport {
    TestExecution* passing_cases;
    size_t passing_cases_length;

    TestExecution* failing_cases;
    size_t failing_cases_length;
} TestReport;

#define CTEST_CASE(n) \
static void n(TestExecution* __ctest_ctx); \
CTEST_CASE_ATTRIB TestCase n ## _ctest_case = { .name = #n, .test_fn = n }; \
void n(TestExecution* __ctest_ctx)

#define CTEST_ASSERT_FAIL(message) \
do { __ctest_ctx->passed = false; } while (false)

#define CTEST_ASSERT_TRUE(...) \
do { if (!(__VA_ARGS__)) CTEST_ASSERT_FAIL("the condition " #__VA_ARGS__ "was expected to be true, but was false"); } while (false)

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
CTEST_DEF TestReport ctest_run_all();
CTEST_DEF TestReport ctest_run(TestExecutionInput input);
CTEST_DEF TestExecution ctest_run_case(TestReport* report, TestCase const* testCase);

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

TestReport ctest_run_all() {
    TestExecutionInput input = { .filter_fn = NULL, .user = NULL };
    return ctest_run(input);
}

TestReport ctest_run(TestExecutionInput input) {
    TestReport report = {
        .passing_cases = NULL,
        .passing_cases_length = 0,
        .failing_cases = NULL,
        .failing_cases_length = 0,
    };
    for (TestCase* testCase = CTEST_CASES_START; testCase < CTEST_CASES_END; ++testCase) {
        // Use filter function, if specified
        if (input.filter_fn != NULL && !input.filter_fn(testCase, input.user)) continue;

        ctest_run_case(&report, testCase);
    }
    return report;
}

TestExecution ctest_run_case(TestReport* report, TestCase const* testCase) {
    // Create execution context for the test case
    TestExecution execution = {
        .test_case = testCase,
        // By default, tests are passing until an assertion fail happens
        .passed = true,
    };
    // Actually run it
    testCase->test_fn(&execution);
    if (report != NULL) {
        // Observe result and append to appropriate list
        // TODO
    }
    return execution;
}

#ifdef __cplusplus
}
#endif

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
    
    // Assert number of cases
    const size_t expectedCaseCount = sizeof(expectedCases) / sizeof(ExpectedTestCase);
    size_t gotCaseCount = (CTEST_CASES_END - CTEST_CASES_START);
    if (gotCaseCount != expectedCaseCount) {
        printf("Expected %zu cases, but found %zu\n", expectedCaseCount, gotCaseCount);
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

    TestReport report = ctest_run_all();

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
