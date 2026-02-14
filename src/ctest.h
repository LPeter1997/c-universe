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

struct TestExecution;

/**
 * A single test case in the test suite.
 */
typedef struct TestCase {
    // The name of the test case
    char const* name;
    // The test function that gets executed when this case is ran
    void(*test_fn)(struct TestExecution*);
} TestCase;

/**
 * A test suite, which is a collection of test cases.
 */
typedef struct TestSuite {
    // The test cases in the suite
    TestCase const* test_cases;
    // The number of test cases in the suite
    size_t test_cases_length;
} TestSuite;

/**
 * The context and result of a test case execution.
 */
typedef struct TestExecution {
    // The test case that was executed
    TestCase const* test_case;
    // True, if the test case passed, false if it failed
    bool passed;
} TestExecution;

/**
 * A filter for test cases, used to select which cases to run when running a test suite.
 */
typedef struct TestFilter {
    // A function that gets called for each test case in the suite, and returns true if the case should be run, or false if it should be skipped
    bool(*filter_fn)(TestCase const*, void*);
    // User data that gets passed to the filter function
    void* user;
} TestFilter;

/**
 * The report of a test suite execution, containing the results of all ran test cases.
 */
typedef struct TestReport {
    // The test executions that passed
    TestExecution* passing_cases;
    // The number of test executions that passed
    size_t passing_cases_length;
    // The capacity of the passing_cases array
    size_t passing_cases_capacity;

    // The test executions that failed
    TestExecution* failing_cases;
    // The number of test executions that failed
    size_t failing_cases_length;
    // The capacity of the failing_cases array
    size_t failing_cases_capacity;
} TestReport;

/**
 * Fails the current test case with the given message.
 * @param message The message to fail with.
 */
#define CTEST_ASSERT_FAIL(message) \
do { __ctest_ctx->passed = false; } while (false)

/**
 * Asserts that the given condition is true, and fails the current test case with a message containing the condition if it is false.
 */
#define CTEST_ASSERT_TRUE(...) \
do { if (!(__VA_ARGS__)) CTEST_ASSERT_FAIL("the condition " #__VA_ARGS__ " was expected to be true, but was false"); } while (false)

/**
 * Defines a test case with the given identifier as a name.
 * @param n The identifier to use as the test case name.
 */
#define CTEST_CASE(n) \
static void n(TestExecution* __ctest_ctx); \
CTEST_CASE_ATTRIB TestCase n ## _ctest_case = { .name = #n, .test_fn = n }; \
void n(TestExecution* __ctest_ctx)

#if defined(__GNUC__) && defined(__APPLE__) && defined(__MACH__)
    #define CTEST_CASE_ATTRIB __attribute__((used, section("__DATA,ctest_test_methods"), aligned(sizeof(void*))))
#elif defined(__GNUC__) && defined(_WIN32)
    #define CTEST_CASE_ATTRIB __attribute__((used, section("ctest_test_methods$2cases"), aligned(sizeof(void*))))
#elif defined(__GNUC__)
    #define CTEST_CASE_ATTRIB __attribute__((used, section("ctest_test_methods"), aligned(sizeof(void*))))
#elif defined(_MSC_VER)
    #define CTEST_CASE_ATTRIB __declspec(allocate("ctest_test_methods$2cases"))
#else
    #error "unsupported C compiler"
#endif

/**
 * Automatically collects all test cases defined with @see CTEST_CASE and returns them as a test suite.
 * @returns A test suite containing all test cases defined with @see CTEST_CASE.
 */
CTEST_DEF TestSuite ctest_get_suite(void);

/**
 * Runs the given test suite with the given filter, and returns a report of the execution.
 * @param suite The test suite to run.
 * @param filter The filter to use when running the test suite.
 * @returns A report of the test suite execution.
 */
CTEST_DEF TestReport ctest_run_suite(TestSuite suite, TestFilter filter);

/**
 * Runs the given test case and returns the execution result.
 * @param testCase The test case to run.
 * @returns The execution result of the test case.
 */
CTEST_DEF TestExecution ctest_run_case(TestCase const* testCase);

/**
 * Frees the memory allocated for the given test suite.
 * @param suite The test suite to free.
 */
CTEST_DEF void ctest_free_suite(TestSuite* suite);

/**
 * Frees the memory allocated for the given test report.
 * @param report The test report to free.
 */
CTEST_DEF void ctest_free_report(TestReport* report);

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

#if defined(__GNUC__) && defined(__APPLE__) && defined(__MACH__)
    extern TestCase __start___DATA_ctest_test_methods[];
    extern TestCase __stop___DATA_ctest_test_methods[];

    #define CTEST_CASES_START __start___DATA_ctest_test_methods
    #define CTEST_CASES_END __stop___DATA_ctest_test_methods
#elif defined(__GNUC__) && defined(_WIN32)
    __attribute__((section("ctest_test_methods$1start"), aligned(sizeof(void*))))
    TestCase __ctest_test_start_sentinel;

    __attribute__((section("ctest_test_methods$3end"), aligned(sizeof(void*))))
    TestCase __ctest_test_end_sentinel;

    #define CTEST_CASES_START (&__ctest_test_start_sentinel + 1)
    #define CTEST_CASES_END &__ctest_test_end_sentinel
#elif defined(__GNUC__)
    extern TestCase __start_ctest_test_methods[];
    extern TestCase __stop_ctest_test_methods[];

    #define CTEST_CASES_START __start_ctest_test_methods
    #define CTEST_CASES_END __stop_ctest_test_methods
#elif defined(_MSC_VER)
    #pragma section("ctest_test_methods$1start", read)
    #pragma section("ctest_test_methods$2cases", read)
    #pragma section("ctest_test_methods$3end", read)
    #pragma comment(linker, "/include:__ctest_test_start_sentinel")

    #define CTEST_CASES_START (&__ctest_test_start_sentinel + 1)
    #define CTEST_CASES_END (&__ctest_test_end_sentinel)
#else
    #error "unsupported C compiler"
#endif

TestSuite ctest_get_suite(void) {
    size_t caseCount = (size_t)(CTEST_CASES_END - CTEST_CASES_START);
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

TestReport ctest_run_suite(TestSuite suite, TestFilter filter) {
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
        if (filter.filter_fn != NULL && !filter.filter_fn(testCase, filter.user)) continue;

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

void ctest_free_suite(TestSuite* suite) {
    CTEST_FREE((void*)suite->test_cases);
    suite->test_cases = NULL;
    suite->test_cases_length = 0;
}

void ctest_free_report(TestReport* report) {
    CTEST_FREE(report->passing_cases);
    CTEST_FREE(report->failing_cases);
    report->passing_cases = NULL;
    report->failing_cases = NULL;
    report->passing_cases_length = 0;
    report->failing_cases_length = 0;
    report->passing_cases_capacity = 0;
    report->failing_cases_capacity = 0;
}

#ifdef __cplusplus
}
#endif

#undef CTEST_CASES_START
#undef CTEST_CASES_END
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
#define EXPECTED_CASE(n, s) { .runCount = 0, .shouldPass = s, .name = #n, .test_fn = n }
    EXPECTED_CASE(case1, true),
    EXPECTED_CASE(case2, false),
    EXPECTED_CASE(case3, true),
#undef EXPECTED_CASE
};

TestCase const* find_test_case_in_suite_by_function(TestSuite suite, void(*testFn)(TestExecution*)) {
    for (size_t i = 0; i < suite.test_cases_length; ++i) {
        if (suite.test_cases[i].test_fn == testFn) return &suite.test_cases[i];
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
    if (gotCaseCount != expectedCaseCount) {
        printf("Expected %zu cases, but found %zu\n", expectedCaseCount, gotCaseCount);
        return 1;
    }

    // Assert initial test case contents
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
        // Look for the expected case in the test suite
        TestCase const* gotCase = find_test_case_in_suite_by_function(suite, expectedCase->test_fn);
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

    TestReport report = ctest_run_suite(suite, (TestFilter){ .filter_fn = NULL, .user = NULL });

    // Assert that each test ran exactly once and that they have the appropriate fail state
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
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

    ctest_free_report(&report);
    ctest_free_suite(&suite);

    puts("Self-test completed successfully!");
    return 0;
}

#endif /* CTEST_SELF_TEST */
