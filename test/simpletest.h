#ifndef SIMPLETEST_H
#define SIMPLETEST_H

#include <stdbool.h>
#include <stdio.h>

#define ANSI_NRM  "\x1B[0m"
#define ANSI_RED  "\x1B[31m"
#define ANSI_GRN  "\x1B[32m"

static void simple_test_print_error(const char *result, const char *expected, const char* text_form) {
    printf(
        "%sFAILED:%s {%s} got %s, expected %s\n",
        ANSI_RED,
        ANSI_NRM,
        text_form,
        result,
        expected
    );
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

#define TESTS_INIT() \
    int _SIMPLE_TESTS_test_count = 0; \
    int _SIMPLE_TESTS_test_passed = 0;

#define TEST_BOOL(test_expr, expected) \
    do { \
    if (simple_test_bool(test_expr, expected, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_TRUE(test_expr) \
    do { \
    if (simple_test_bool(test_expr, true, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_FALSE(test_expr) \
    do { \
    if (simple_test_bool(test_expr, false, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_INT(test_expr, expected) \
    do { \
    if (simple_test_int((test_expr), (expected), #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TEST_UINT(test_expr, expected) \
    do { \
    if (simple_test_uint(test_expr, expected, #test_expr)) { \
        ++_SIMPLE_TESTS_test_passed; \
    } \
    ++_SIMPLE_TESTS_test_count; \
    } while (0)

#define TESTS_SHOW_RESULTS() \
    do { \
    printf("---\nTESTS (%.2f%%): Total %d %sPassed %d%s", \
        (float)_SIMPLE_TESTS_test_passed / (float)_SIMPLE_TESTS_test_count * 100, \
        _SIMPLE_TESTS_test_count, \
        ANSI_GRN, \
        _SIMPLE_TESTS_test_passed, \
        ANSI_NRM \
    ); \
    if (_SIMPLE_TESTS_test_passed < _SIMPLE_TESTS_test_count) { \
        printf(" %sFailed %d%s\n", \
            ANSI_RED, \
            _SIMPLE_TESTS_test_count - _SIMPLE_TESTS_test_passed, \
            ANSI_NRM \
        ); \
    } \
    else { printf("\n"); } \
    } while (0)

#endif // !SIMPLETEST_H
