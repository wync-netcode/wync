#ifndef SIMPLETEST_H
#define SIMPLETEST_H

#include <stdbool.h>
#include <stdio.h>

#define ANSI_RESET   "\x1b[0m"
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_YELLOW  "\x1b[33m"
#define ANSI_BLUE    "\x1b[34m"
#define ANSI_MAGENTA "\x1b[35m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_GRAY    "\x1b[90m"

static int SIMPLE_TEST_CODE = 0;

static void simple_test_print_error(const char *result, const char *expected, const char* text_form) {
    printf(
        "%sFAILED:%s {%s} got %s, expected %s\n",
        ANSI_RED,
        ANSI_RESET,
        text_form,
        result,
        expected
    );
    SIMPLE_TEST_CODE = 1;
}

bool simple_test_int(long long result, long long expected, const char* text_form) {
    if (result != expected) {
        char result_str[100], expected_str[100];
        sprintf(result_str, "%lld", result); sprintf(expected_str, "%lld", expected);
        simple_test_print_error(result_str, expected_str, text_form);
    }
    return result == expected;
}

bool simple_test_uint(unsigned long long result, unsigned long long expected, const char* text_form) {
    if (result != expected) {
        char result_str[100], expected_str[100];
        sprintf(result_str, "%llu", result); sprintf(expected_str, "%llu", expected);
        simple_test_print_error(result_str, expected_str, text_form);
    }
    return result == expected;
}

//bool simple_test_float(float result, float expected, const char* text_form) {
    //if (result != expected) {
        //char result_str[100], expected_str[100];
        //sprintf(result_str, "%", result); sprintf(expected_str, "%llu", result);
        //simple_test_print_error(result_str, expected_str, text_form);
    //}
    //return result == expected;
//}

bool simple_test_bool(bool result, bool expected, const char* text_form) {
    if (result != expected) {
        char result_str[100], expected_str[100];
        sprintf(result_str, "%s", result ? "true" : "false");
        sprintf(expected_str, "%s", expected ? "true" : "false");
        simple_test_print_error(result_str, expected_str, text_form);
    }
    return result == expected;
}

#define PRINT_LINE_INFO() \
    do { \
    printf("%s", ANSI_GRAY); \
    printf(" %s|%s:%d", __func__, __FILE__, __LINE__); \
    printf("%s\n", ANSI_RESET); \
    } while (0)

#define TESTS_INIT() \
    int _SIMPLE_TESTS_test_count = 0; \
    int _SIMPLE_TESTS_test_passed = 0;

#define TEST_BOOL(test_expr, expected) \
    do { \
    if (simple_test_bool(test_expr, expected, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    else { PRINT_LINE_INFO(); } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_TRUE(test_expr) \
    do { \
    if (simple_test_bool(test_expr, true, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    else { PRINT_LINE_INFO(); } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_FALSE(test_expr) \
    do { \
    if (simple_test_bool(test_expr, false, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    else { PRINT_LINE_INFO(); } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_INT(test_expr, expected) \
    do { \
    if (simple_test_int((test_expr), (expected), #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    else { PRINT_LINE_INFO(); } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_UINT(test_expr, expected) \
    do { \
    if (simple_test_uint(test_expr, expected, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    else { PRINT_LINE_INFO(); } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TESTS_SHOW_RESULTS() \
    do { \
    printf("---\nTESTS (%.2f%%): Total %d %sPassed %d%s", \
        (float)_SIMPLE_TESTS_test_passed / (float)_SIMPLE_TESTS_test_count * 100, \
        _SIMPLE_TESTS_test_count, \
        ANSI_GREEN, \
        _SIMPLE_TESTS_test_passed, \
        ANSI_RESET \
    ); \
    if (_SIMPLE_TESTS_test_passed < _SIMPLE_TESTS_test_count) { \
        printf(" %sFailed %d%s\n", \
            ANSI_RED, \
            _SIMPLE_TESTS_test_count - _SIMPLE_TESTS_test_passed, \
            ANSI_RESET \
        ); \
    } \
    else { printf("\n"); } \
    } while (0)

#endif // !SIMPLETEST_H
