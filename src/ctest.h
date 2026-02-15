/**
 * ctest.h is a single-header C testing framework.
 *
 * A fairly minimalistic single-header testing framework for C.
 *
 * Configuration:
 *  - #define CTEST_STATIC before including this header to make all functions have internal linkage
 *  - #define CTEST_REALLOC and CTEST_FREE to use custom memory allocation functions (by default they use realloc and free from the C standard library)
 *  - #define CTEST_MAIN before including this header to compile a default main program that runs all test cases defined with CTEST_CASE, and accepts optional command-line arguments to filter which cases to run
 *  - #define CTEST_SELF_TEST before including this header to compile a self-test that verifies the framework's functionality
 *  - #define CTEST_EXAMPLE before including this header to compile a simple example that demonstrates how to use the framework
 *
 * API:
 *  - Use CTEST_CASE to define test cases, which automatically registers them in the default test suite
 *  - Use ctest_get_suite to get the default test suite, which contains all cases defined with CTEST_CASE
 *  - Use ctest_run_suite to run a test suite with a filter, which returns a report of the execution
 *  - Use ctest_run_case to run a single test case and get the execution result
 *  - Use ctest_free_suite and ctest_free_report to free the memory allocated for test suites and reports, respectively
 *  - Use CTEST_ASSERT_TRUE and CTEST_ASSERT_FAIL to make assertions in test cases, which will cause the case to fail if they are not met
 *
 * Check the example section at the end of this file for a full example.
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
    TestCase* test_cases;
    // The number of test cases in the suite
    size_t test_cases_length;
    // The capacity of the test_cases array
    size_t test_cases_capacity;
} TestSuite;

/**
 * The context and result of a test case execution.
 */
typedef struct TestExecution {
    // The test case that was executed
    TestCase const* test_case;
    // True, if the test case passed, false if it failed
    bool passed;
    // An optional message describing the failure, if the test case failed
    char const* fail_message;
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

// Used as a target to automatically register the cases
extern TestSuite __ctest_default_suite;

/**
 * Fails the current test case with the given message.
 * @param message The message to fail with.
 */
#define CTEST_ASSERT_FAIL(message) \
do { __ctest_ctx->passed = false; __ctest_ctx->fail_message = message; return; } while (false)

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
__CTEST_AUTOREGISTER_CASE(n) \
void n(TestExecution* __ctest_ctx)

#if defined(__GNUC__) || defined(__clang__)
    #define __CTEST_AUTOREGISTER_CASE(n) \
    __attribute__((constructor)) \
    static void __ctest_register_ ## n(void) { \
        ctest_register_case(&__ctest_default_suite, (TestCase){ .name = #n, .test_fn = n }); \
    }
#elif defined(_MSC_VER)
    #ifdef _WIN64
        #define __CTEST_LINKER_PREFIX ""
    #else
        #define __CTEST_LINKER_PREFIX "_"
    #endif

    #pragma section(".CRT$XCU",read)
    #define __CTEST_AUTOREGISTER_CASE(n) \
    static void __ctest_register_ ## n(void); \
    __declspec(allocate(".CRT$XCU")) void (*__ctest_register_ ## n ## _)(void) = __ctest_register_ ## n; \
    __pragma(comment(linker,"/include:" __CTEST_LINKER_PREFIX "__ctest_register_" #n "_")) \
    static void __ctest_register_ ## n(void) { \
        ctest_register_case(&__ctest_default_suite, (TestCase){ .name = #n, .test_fn = n }); \
    }
#else
    #error "unsupported C compiler"
#endif

/**
 * Registers the given test case in the given test suite.
 * @param suite The test suite to register the case in.
 * @param testCase The test case to register.
 */
CTEST_DEF void ctest_register_case(TestSuite* suite, TestCase testCase);

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

/**
 * Prints a human-readable report of the given test report to stdout.
 * @param report The test report to print.
 */
CTEST_DEF void ctest_print_report(TestReport report);

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

TestSuite __ctest_default_suite;

void ctest_register_case(TestSuite* suite, TestCase testCase) {
    if (suite->test_cases_length + 1 > suite->test_cases_capacity) {
        size_t newCapacity = (suite->test_cases_capacity == 0) ? 8 : (suite->test_cases_capacity * 2);
        TestCase* newCases = (TestCase*)CTEST_REALLOC((void*)suite->test_cases, sizeof(TestCase) * newCapacity);
        CTEST_ASSERT(newCases != NULL, "failed to allocate memory for test suite");
        suite->test_cases = newCases;
        suite->test_cases_capacity = newCapacity;
    }
    suite->test_cases[suite->test_cases_length] = testCase;
    ++suite->test_cases_length;
}

TestSuite ctest_get_suite(void) {
    return __ctest_default_suite;
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

void ctest_print_report(TestReport report) {
    printf("Test report:\n");
    printf("  Passing cases (%zu):\n", report.passing_cases_length);
    for (size_t i = 0; i < report.passing_cases_length; ++i) {
        TestExecution execution = report.passing_cases[i];
        printf("    - %s\n", execution.test_case->name);
    }
    printf("  Failing cases (%zu):\n", report.failing_cases_length);
    for (size_t i = 0; i < report.failing_cases_length; ++i) {
        TestExecution execution = report.failing_cases[i];
        printf("    - %s: %s\n", execution.test_case->name, execution.fail_message);
    }
    if (report.failing_cases_length == 0) {
        printf(" Success!\n");
    }
    else {
        printf(" Failure (%zu/%zu)!\n", report.passing_cases_length, report.passing_cases_length + report.failing_cases_length);
    }
}

#ifdef __cplusplus
}
#endif

#undef CTEST_CASES_START
#undef CTEST_CASES_END
#undef CTEST_ASSERT

#endif /* CTEST_IMPLEMENTATION */

////////////////////////////////////////////////////////////////////////////////
// Main-program section                                                       //
////////////////////////////////////////////////////////////////////////////////
#ifdef CTEST_MAIN

#include <string.h>

typedef struct CliFilters {
    char** words;
    size_t word_count;
} CliFilters;

static bool filter_cases_by_name(TestCase const* testCase, void* user) {
    CliFilters* filters = (CliFilters*)user;
    for (size_t i = 0; i < filters->word_count; ++i) {
        if (strstr(testCase->name, filters->words[i]) != NULL) {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    // We implement a default main program that runs all test cases, if no arguments are specified
    // If there are arguments, we use them as filters and only run the cases that contain any of the arguments as a substring in their name

    TestFilter filter = { 0 };
    CliFilters cliFilters = { 0 };
    if (argc > 1) {
        cliFilters.words = &argv[1];
        cliFilters.word_count = (size_t)(argc - 1);
        filter.filter_fn = filter_cases_by_name;
        filter.user = &cliFilters;
    }

    TestSuite suite = ctest_get_suite();
    TestReport report = ctest_run_suite(suite, filter);
    ctest_print_report(report);
    int exitCode = (report.failing_cases_length == 0) ? 0 : 1;

    ctest_free_report(&report);
    ctest_free_suite(&suite);

    return exitCode;
}

#endif /* CTEST_MAIN */

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

    puts("Sample output of a report:");
    ctest_print_report(report);

    ctest_free_report(&report);
    ctest_free_suite(&suite);

    puts("Self-test completed successfully!");
    return 0;
}

#endif /* CTEST_SELF_TEST */

////////////////////////////////////////////////////////////////////////////////
// Example section                                                            //
////////////////////////////////////////////////////////////////////////////////
#ifdef CTEST_EXAMPLE
#undef CTEST_EXAMPLE

// This example relies on the built-in main program provided by the library to implement a very simple test runner.

#define CTEST_IMPLEMENTATION
#define CTEST_STATIC
#define CTEST_MAIN
#include "ctest.h"

int factorial(int n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

CTEST_CASE(factorial_of_zero) {
    CTEST_ASSERT_TRUE(factorial(0) == 1);
}

CTEST_CASE(factorial_of_positive) {
    CTEST_ASSERT_TRUE(factorial(5) == 120);
}

#endif /* CTEST_EXAMPLE */
