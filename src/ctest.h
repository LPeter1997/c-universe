/**
 * ctest.h is a single-header C testing framework.
 *
 * A fairly minimalistic single-header testing framework for C.
 * If needed, the tested code can override its assertion macros to seamlessly integrate with the framework, check the
 * self-test section or the example section at the end of this file for examples of how to do that.
 *
 * Configuration:
 *  - #define CTEST_IMPLEMENTATION before including this header in exactly one source file to include the implementation section
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

#include <setjmp.h>
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

struct CTest_Execution;

/**
 * A single test case in the test suite.
 */
typedef struct CTest_Case {
    // The name of the test case
    char const* name;
    // The test function that gets executed when this case is ran
    void(*test_fn)(void);
    // If true, the test case is expected to fail
    bool should_fail;
} CTest_Case;

/**
 * A test suite, which is a collection of test cases.
 */
typedef struct CTest_Suite {
    // The test cases in the suite
    CTest_Case* cases;
    // The number of test cases in the suite
    size_t length;
    // The capacity of the cases array
    size_t capacity;
} CTest_Suite;

/**
 * The context and result of a test case execution.
 */
typedef struct CTest_Execution {
    // The test case that was executed
    CTest_Case const* test_case;
    // True, if the test case passed, false if it failed
    bool passed;
    // Information about failure
    struct {
        // An optional message describing the failure, if the test case failed
        char const* message;
        // An optional file path to the file where the failure happened
        char const* file;
        // An optional function name where the failure happened
        char const* function;
        // An optional line number where the failure happened
        int line;
    } fail_info;
    // Used internally to be able to catch assertions from within the test functions and even across the SUT code, if necessary
    jmp_buf jmp_env;
} CTest_Execution;

/**
 * A filter for test cases, used to select which cases to run when running a test suite.
 */
typedef struct CTest_Filter {
    // A function that gets called for each test case in the suite, and returns true if the case should be run, or false if it should be skipped
    bool(*filter_fn)(CTest_Case const*, void*);
    // User data that gets passed to the filter function
    void* user;
} CTest_Filter;

/**
 * The report of a test suite execution, containing the results of all ran test cases.
 */
typedef struct CTest_Report {
    // Passing execution info
    struct {
        // The test executions that passed
        CTest_Execution* cases;
        // The number of test executions that passed
        size_t length;
        // The capacity of the passing cases array
        size_t capacity;
    } passing;

    // Failing execution info
    struct {
        // The test executions that failed
        CTest_Execution* cases;
        // The number of test executions that failed
        size_t length;
        // The capacity of the failing cases array
        size_t capacity;
    } failing;
} CTest_Report;

// Used as a target to automatically register the cases
extern CTest_Suite __ctest_default_suite;

// The context for the currently running test case
extern CTest_Execution* __ctest_ctx;

/**
 * Fails the current test case with the given message.
 * @param message The message to fail with.
 */
#define CTEST_ASSERT_FAIL(message) ctest_fail(message, __FILE__, __func__, __LINE__)

/**
 * An assert that can be used to override the tested library's assertion macro to seamlessly integrate with the test framework,
 * and fail the current test case with a message containing the failed assertion if the condition is false.
 */
#define CTEST_NATIVE_ASSERT(...) \
    do { \
        if (!(__VA_ARGS__)) { \
            CTEST_ASSERT_FAIL("native assertion " #__VA_ARGS__ " failed"); \
        } \
    } while (false)

/**
 * Asserts that the given condition is true, and fails the current test case with a message containing the condition if it is false.
 */
#define CTEST_ASSERT_TRUE(...) \
    do { \
        if (!(__VA_ARGS__)) { \
            CTEST_ASSERT_FAIL("the condition " #__VA_ARGS__ " was expected to be true, but was false"); \
        } \
    } while (false)

/**
 * Defines a test case with the given identifier as a name.
 * @param n The identifier to use as the test case name.
 * @param ... Any extra configuration passed onto the test case.
 */
#define CTEST_CASE(n, ...) \
static void n(void); \
__CTEST_AUTOREGISTER_CASE(n, __VA_ARGS__) \
void n(void)

#if defined(__GNUC__) || defined(__clang__)
    #define __CTEST_AUTOREGISTER_CASE(n, ...) \
    __attribute__((constructor)) \
    static void __ctest_register_ ## n(void) { \
        ctest_register_case(&__ctest_default_suite, (CTest_Case){ .name = #n, .test_fn = n, __VA_ARGS__ }); \
    }
#elif defined(_MSC_VER)
    #ifdef _WIN64
        #define __CTEST_LINKER_PREFIX ""
    #else
        #define __CTEST_LINKER_PREFIX "_"
    #endif

    #pragma section(".CRT$XCU",read)
    #define __CTEST_AUTOREGISTER_CASE(n, ...) \
    static void __ctest_register_ ## n(void); \
    __declspec(allocate(".CRT$XCU")) void (*__ctest_register_ ## n ## _)(void) = __ctest_register_ ## n; \
    __pragma(comment(linker,"/include:" __CTEST_LINKER_PREFIX "__ctest_register_" #n "_")) \
    static void __ctest_register_ ## n(void) { \
        ctest_register_case(&__ctest_default_suite, (CTest_Case){ .name = #n, .test_fn = n, __VA_ARGS__ }); \
    }
#else
    #error "unsupported C compiler"
#endif

/**
 * Fails the current test case with the given message, file, function and line information.
 * Can be used to fail from outside of the test functions, if necessary.
 * The tested library can override the assertion macro to use this function to seamlessly integrate with the test framework.
 * @param message The message to fail with.
 * @param file The file where the failure happened.
 * @param function The function where the failure happened.
 * @param line The line number where the failure happened.
 */
CTEST_DEF void ctest_fail(char const* message, char const* file, char const* function, int line);

/**
 * Registers the given test case in the given test suite.
 * @param suite The test suite to register the case in.
 * @param testCase The test case to register.
 */
CTEST_DEF void ctest_register_case(CTest_Suite* suite, CTest_Case testCase);

/**
 * Automatically collects all test cases defined with @see CTEST_CASE and returns them as a test suite.
 * @returns A test suite containing all test cases defined with @see CTEST_CASE.
 */
CTEST_DEF CTest_Suite ctest_get_suite(void);

/**
 * Runs the given test suite with the given filter, and returns a report of the execution.
 * @param suite The test suite to run.
 * @param filter The filter to use when running the test suite.
 * @returns A report of the test suite execution.
 */
CTEST_DEF CTest_Report ctest_run_suite(CTest_Suite suite, CTest_Filter filter);

/**
 * Runs the given test case and returns the execution result.
 * @param testCase The test case to run.
 * @returns The execution result of the test case.
 */
CTEST_DEF CTest_Execution ctest_run_case(CTest_Case const* testCase);

/**
 * Frees the memory allocated for the given test suite.
 * @param suite The test suite to free.
 */
CTEST_DEF void ctest_free_suite(CTest_Suite* suite);

/**
 * Frees the memory allocated for the given test report.
 * @param report The test report to free.
 */
CTEST_DEF void ctest_free_report(CTest_Report* report);

/**
 * Prints a human-readable report of the given test report to stdout.
 * @param report The test report to print.
 */
CTEST_DEF void ctest_print_report(CTest_Report report);

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

CTest_Suite __ctest_default_suite;
CTest_Execution* __ctest_ctx;

void ctest_fail(char const* message, char const* file, char const* function, int line) {
    __ctest_ctx->passed = false;
    __ctest_ctx->fail_info.message = message;
    __ctest_ctx->fail_info.file = file;
    __ctest_ctx->fail_info.function = function;
    __ctest_ctx->fail_info.line = line;
    longjmp(__ctest_ctx->jmp_env, 1);
}

void ctest_register_case(CTest_Suite* suite, CTest_Case testCase) {
    if (suite->length + 1 > suite->capacity) {
        size_t newCapacity = (suite->capacity == 0) ? 8 : (suite->capacity * 2);
        CTest_Case* newCases = (CTest_Case*)CTEST_REALLOC((void*)suite->cases, sizeof(CTest_Case) * newCapacity);
        CTEST_ASSERT(newCases != NULL, "failed to allocate memory for test suite");
        suite->cases = newCases;
        suite->capacity = newCapacity;
    }
    suite->cases[suite->length] = testCase;
    ++suite->length;
}

CTest_Suite ctest_get_suite(void) {
    return __ctest_default_suite;
}

CTest_Report ctest_run_suite(CTest_Suite suite, CTest_Filter filter) {
    CTest_Report report = {
        .passing = {
            .cases = NULL,
            .length = 0,
            .capacity = 0,
        },
        .failing = {
            .cases = NULL,
            .length = 0,
            .capacity = 0,
        },
    };
    for (size_t i = 0; i < suite.length; ++i) {
        CTest_Case const* testCase = &suite.cases[i];

        // Use filter function, if specified
        if (filter.filter_fn != NULL && !filter.filter_fn(testCase, filter.user)) continue;

        CTest_Execution execution = ctest_run_case(testCase);

        // Add execution to report
        CTest_Execution** targetList = execution.passed ? &report.passing.cases : &report.failing.cases;
        size_t* targetListLength = execution.passed ? &report.passing.length : &report.failing.length;
        size_t* targetListCapacity = execution.passed ? &report.passing.capacity : &report.failing.capacity;
        if (*targetListLength + 1 > *targetListCapacity) {
            size_t newCapacity = (*targetListCapacity == 0) ? 8 : (*targetListCapacity * 2);
            CTest_Execution* newList = (CTest_Execution*)CTEST_REALLOC(*targetList, newCapacity * sizeof(CTest_Execution));
            CTEST_ASSERT(newList != NULL, "failed to allocate memory for test report");
            *targetList = newList;
            *targetListCapacity = newCapacity;
        }
        (*targetList)[*targetListLength] = execution;
        ++(*targetListLength);
    }
    return report;
}

CTest_Execution ctest_run_case(CTest_Case const* testCase) {
    // Create execution context for the test case
    CTest_Execution execution = {
        .test_case = testCase,
        // By default, tests are passing until an assertion fail happens
        .passed = true,
    };
    if (setjmp(execution.jmp_env) == 0) {
        // Set up environment
        __ctest_ctx = &execution;
        // Actually run it
        testCase->test_fn();
        // If we got here but the test case was expected to fail, it means that it didn't fail as expected, so we mark it as a failure
        if (testCase->should_fail) {
            execution.passed = false;
            execution.fail_info.message = "test case was expected to fail, but it passed";
            execution.fail_info.function = testCase->name;
        }
    }
    else {
        // If we are here, it means that a failure happened and longjmp was called to jump back to the setjmp point
        if (testCase->should_fail) {
            // If the test case was expected to fail, we consider it a pass instead, since it failed as expected
            execution.passed = true;
        }
        else {
            // Actual failure
        }
    }
    return execution;
}

void ctest_free_suite(CTest_Suite* suite) {
    CTEST_FREE((void*)suite->cases);
    suite->cases = NULL;
    suite->length = 0;
}

void ctest_free_report(CTest_Report* report) {
    CTEST_FREE(report->passing.cases);
    CTEST_FREE(report->failing.cases);
    report->passing.cases = NULL;
    report->failing.cases = NULL;
    report->passing.length = 0;
    report->failing.length = 0;
    report->passing.capacity = 0;
    report->failing.capacity = 0;
}

void ctest_print_report(CTest_Report report) {
    printf("Test report:\n");
    printf("  Passing cases (%zu):\n", report.passing.length);
    for (size_t i = 0; i < report.passing.length; ++i) {
        CTest_Execution execution = report.passing.cases[i];
        printf("    - %s\n", execution.test_case->name);
    }
    printf("  Failing cases (%zu):\n", report.failing.length);
    for (size_t i = 0; i < report.failing.length; ++i) {
        CTest_Execution execution = report.failing.cases[i];
        printf("    - %s: %s (file: %s, function: %s, line: %d)\n", execution.test_case->name, execution.fail_info.message, execution.fail_info.file, execution.fail_info.function, execution.fail_info.line);
    }
    if (report.failing.length == 0) {
        printf(" Success!\n");
    }
    else {
        printf(" Failure (%zu/%zu)!\n", report.passing.length, report.passing.length + report.failing.length);
    }
}

#ifdef __cplusplus
}
#endif

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

static bool filter_cases_by_name(CTest_Case const* testCase, void* user) {
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

    CTest_Filter filter = { 0 };
    CliFilters cliFilters = { 0 };
    if (argc > 1) {
        cliFilters.words = &argv[1];
        cliFilters.word_count = (size_t)(argc - 1);
        filter.filter_fn = filter_cases_by_name;
        filter.user = &cliFilters;
    }

    CTest_Suite suite = ctest_get_suite();
    CTest_Report report = ctest_run_suite(suite, filter);
    ctest_print_report(report);
    int exitCode = (report.failing.length == 0) ? 0 : 1;

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
    void(*test_fn)(void);
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

CTEST_CASE(case4, .should_fail = true) {
    ++expectedCases[3].runCount;
    CTEST_ASSERT_FAIL("needs to fail");
}

CTEST_CASE(case5, .should_fail = true) {
    ++expectedCases[4].runCount;
    // Should fail but we won't fail it
}

ExpectedTestCase expectedCases[] = {
#define EXPECTED_CASE(n, s) { .runCount = 0, .shouldPass = s, .name = #n, .test_fn = n }
    EXPECTED_CASE(case1, true),
    EXPECTED_CASE(case2, false),
    EXPECTED_CASE(case3, true),
    EXPECTED_CASE(case4, true),
    EXPECTED_CASE(case5, false),
#undef EXPECTED_CASE
};

CTest_Case const* find_test_case_in_suite_by_function(CTest_Suite suite, void(*testFn)(void)) {
    for (size_t i = 0; i < suite.length; ++i) {
        if (suite.cases[i].test_fn == testFn) return &suite.cases[i];
    }
    return NULL;
}

CTest_Execution* find_test_execution_in_report_by_function(CTest_Report report, void(*testFn)(void)) {
    for (size_t i = 0; i < report.passing.length; ++i) {
        if (report.passing.cases[i].test_case->test_fn == testFn) {
            return &report.passing.cases[i];
        }
    }
    for (size_t i = 0; i < report.failing.length; ++i) {
        if (report.failing.cases[i].test_case->test_fn == testFn) {
            return &report.failing.cases[i];
        }
    }
    return NULL;
}

int main(void) {
    puts("Running CTEST self-test...");

    // Collect suite
    CTest_Suite suite = ctest_get_suite();

    // Assert number of cases
    const size_t expectedCaseCount = sizeof(expectedCases) / sizeof(ExpectedTestCase);
    size_t gotCaseCount = suite.length;
    if (gotCaseCount != expectedCaseCount) {
        printf("Expected %zu cases, but found %zu\n", expectedCaseCount, gotCaseCount);
        return 1;
    }

    // Assert initial test case contents
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
        // Look for the expected case in the test suite
        CTest_Case const* gotCase = find_test_case_in_suite_by_function(suite, expectedCase->test_fn);
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

    CTest_Report report = ctest_run_suite(suite, (CTest_Filter){ .filter_fn = NULL, .user = NULL });

    // Assert that each test ran exactly once and that they have the appropriate fail state
    for (size_t i = 0; i < expectedCaseCount; ++i) {
        ExpectedTestCase* expectedCase = &expectedCases[i];
        CTest_Execution* gotExecution = find_test_execution_in_report_by_function(report, expectedCase->test_fn);
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

#include <assert.h>

// This example relies on the built-in main program provided by the library to implement a very simple test runner.

#define CTEST_IMPLEMENTATION
#define CTEST_STATIC
#define CTEST_MAIN
#include "ctest.h"

// Library code ////////////////////////////////////////////////////////////////

// Let's say the library is aware that it can be tested and we want to seamlessly integrate its native assertions
// with the test framework's, this is how we could do that
#ifdef CTEST_H
    #undef assert
    #define assert CTEST_NATIVE_ASSERT
#endif

int factorial(int n) {
    assert(n >= 0);
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

// Test code ///////////////////////////////////////////////////////////////////

CTEST_CASE(factorial_of_zero) {
    CTEST_ASSERT_TRUE(factorial(0) == 1);
}

CTEST_CASE(factorial_of_positive) {
    CTEST_ASSERT_TRUE(factorial(5) == 120);
}

CTEST_CASE(factorial_of_negative, .should_fail = true) {
    // This will cause an assertion failure in the factorial function, which will be caught by the test framework and reported as a test failure
    factorial(-1);
}

#endif /* CTEST_EXAMPLE */
