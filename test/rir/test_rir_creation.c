#include <check.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <String/rf_str_core.h>
#include <ast/ast.h>

#include "testsupport_rir.h"

#include CLIB_TEST_HELPERS



START_TEST(test_types_list_simple1) {

    static const struct RFstring s = RF_STRING_STATIC_INIT(
        "fn foo(a:u32) -> u64 {\n"
        "return 45 + a\n"
        "}"
    );
    struct rir_testdriver *d = get_rir_testdriver();
    rir_testdriver_assign(d, &s);
    testsupport_rir_process(d);

    static const struct RFstring id_a = RF_STRING_STATIC_INIT("a");
    struct rir_type *fn = testsupport_rir_type_create(d, COMPOSITE_IMPLICATION_RIR_TYPE, NULL);
    struct rir_type *fn_arg = testsupport_rir_type_create(d, ELEMENTARY_RIR_TYPE_UINT_32, &id_a);
    testsupport_rir_type_add_subtype(fn, fn_arg);
    // fn ret
    testsupport_rir_type_add_subtype(
        fn,
        testsupport_rir_type_create(d, ELEMENTARY_RIR_TYPE_UINT_64, NULL));

    struct rir_type *expected_types[] = {fn, fn_arg};
    rir_testdriver_compare_lists(d, expected_types, 2);
} END_TEST

Suite *rir_creation_suite_create(void)
{
    Suite *s = suite_create("rir_creation");

    TCase *st_basic = tcase_create("rir_creation_types_list");
    tcase_add_checked_fixture(st_basic,
                              setup_rir_tests,
                              teardown_rir_tests);
    tcase_add_test(st_basic, test_types_list_simple1);

    suite_add_tcase(s, st_basic);

    return s;
}
