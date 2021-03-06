#include <check.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <info/msg.h>

#include <ast/function.h>
#include <ast/matchexpr.h>
#include <ast/operators.h>
#include "../testsupport_front.h"
#include "../parser/testsupport_parser.h"
#include "testsupport_analyzer.h"

#include CLIB_TEST_HELPERS

START_TEST (test_typecheck_valid_for1) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    array:u16[3] = [1, 2, 3]\n"
        "    b:u64\n"
        "    for a in array {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    ck_assert_typecheck_ok();
} END_TEST

START_TEST (test_typecheck_valid_for2) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    array:u16[3] = [1, 2, 3]\n"
        "    b:u64\n"
        "    newlist:u16[3] = for a in array {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    ck_assert_typecheck_ok();
} END_TEST

START_TEST (test_typecheck_valid_range_for1) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    b:u64\n"
        "    for a in 1:10 {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    ck_assert_typecheck_ok();
} END_TEST

START_TEST (test_typecheck_valid_range_for2) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    b:u64\n"
        "    for a in 10:-1:1 {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    ck_assert_typecheck_ok();
} END_TEST

START_TEST (test_typecheck_valid_range_for3) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    start:i64 = 0\n"
        "    step:i64 = 1\n"
        "    end:i64 = 10\n"
        "    b:u64\n"
        "    for a in start:step:end {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    ck_assert_typecheck_ok();
} END_TEST

START_TEST (test_typecheck_invalid_for1) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    b:u64\n"
        "    for a in array {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Undeclared identifier \"array\" as the iterable of a for expression",
            2, 13, 2, 17)
    };

    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

START_TEST (test_typecheck_invalid_for2) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    whatever:string\n"
        "    b:u64\n"
        "    for a in whatever {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Trying to iterate over non-iterable type \"string\".",
            3, 13, 3, 20)
    };

    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

START_TEST (test_typecheck_invalid_for3) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    array:u16[3] = [1, 2, 3]\n"
        "    b:u64\n"
        "    newlist:string[3] = for a in array {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Assignment between incompatible types. Can't assign \"u64[3]\" to"
            " \"string[3]\". Array member type mismatch. \"u64\" != \"string\".",
            3, 4, 5, 4)
    };

    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

START_TEST (test_typecheck_invalid_for4) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    array:u64[3] = [1, 2, 3]\n"
        "    b:u64\n"
        "    newlist:u64[5] = for a in array {\n"
        "        b = b + a\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Assignment between incompatible types. Can't assign \"u64[3]\" to"
            " \"u64[5]\". Mismatch at the size of the 1st array dimension 3 != 5.",
            3, 4, 5, 4)
    };

    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

START_TEST (test_typecheck_invalid_for_range1) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    for a in 5:1 {\n"
        "        print(a)\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Provided range start \"5\" is greater or equal to the end \"1\""
            " while the step is increasing.",
            1, 13, 1, 15)
    };
    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

START_TEST (test_typecheck_invalid_for_range2) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    for a in 0:-2:10 {\n"
        "        print(a)\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Provided range start \"0\" is less or equal to the end \"10\""
            " while the step is decreasing.",
            1, 13, 1, 19)
    };
    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

START_TEST (test_typecheck_invalid_for_range3) {
    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "{\n"
        "    for a in 1:0:10 {\n"
        "        print(a)\n"
        "    }\n"
        "}\n"
    );
    front_testdriver_new_ast_main_source(&s);
    struct info_msg messages[] = {
        TESTSUPPORT_INFOMSG_INIT_BOTH(
            MESSAGE_SEMANTIC_ERROR,
            "Providing zero as range step is invalid.",
            1, 13, 1, 18)
    };
    ck_assert_typecheck_with_messages(false, messages);
} END_TEST

Suite *analyzer_typecheck_forexpr_suite_create(void)
{
    Suite *s = suite_create("typecheck_forexpr");

    TCase *tc1 = tcase_create("typecheck_valid_forexpr");
    tcase_add_checked_fixture(
        tc1,
        setup_analyzer_tests,
        teardown_analyzer_tests
    );
    tcase_add_test(tc1, test_typecheck_valid_for1);
    tcase_add_test(tc1, test_typecheck_valid_for2);

    TCase *tc2 = tcase_create("typecheck_valid_range_forexpr");
    tcase_add_checked_fixture(
        tc2,
        setup_analyzer_tests,
        teardown_analyzer_tests
    );
    tcase_add_test(tc2, test_typecheck_valid_range_for1);
    tcase_add_test(tc2, test_typecheck_valid_range_for2);
    tcase_add_test(tc2, test_typecheck_valid_range_for3);

    TCase *tc3 = tcase_create("typecheck_invalid_forexpr");
    tcase_add_checked_fixture(
        tc3,
        setup_analyzer_tests_with_filelog,
        teardown_analyzer_tests
    );
    tcase_add_test(tc3, test_typecheck_invalid_for1);
    tcase_add_test(tc3, test_typecheck_invalid_for2);
    tcase_add_test(tc3, test_typecheck_invalid_for3);
    tcase_add_test(tc3, test_typecheck_invalid_for4);

    TCase *tc4 = tcase_create("typecheck_invalid_for_with_range");
    tcase_add_checked_fixture(
        tc4,
        setup_analyzer_tests_with_filelog,
        teardown_analyzer_tests
    );
    tcase_add_test(tc4, test_typecheck_invalid_for_range1);
    tcase_add_test(tc4, test_typecheck_invalid_for_range2);
    tcase_add_test(tc4, test_typecheck_invalid_for_range3);

    suite_add_tcase(s, tc1);
    suite_add_tcase(s, tc2);
    suite_add_tcase(s, tc3);
    suite_add_tcase(s, tc4);
    return s;
}
