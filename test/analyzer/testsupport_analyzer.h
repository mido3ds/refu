#ifndef LFR_TESTSUPPORT_ANALYZER_H
#define LFR_TESTSUPPORT_ANALYZER_H

#include <stdbool.h>
#include <stdint.h>

#include <rfbase/datastructs/darray.h>

#include <inplocation.h>
#include <parser/parser.h>
#include <lexer/lexer.h>

#include <types/type.h>
#include <types/type_comparisons.h>
#include <types/type_arr.h>
#include <analyzer/typecheck.h>

#include "../testsupport.h"

struct ast_node;

struct analyzer_testdriver {
    //! A buffer of types for quick type checks
    //! and easy freeing at test teardown
    struct {darray(struct type*);} types;
};
struct analyzer_testdriver *get_analyzer_testdriver();

bool analyzer_testdriver_init(struct analyzer_testdriver *d);
void analyzer_testdriver_deinit(struct analyzer_testdriver *d);

void setup_analyzer_tests();
void setup_analyzer_tests_no_stdlib();
void setup_analyzer_tests_with_filelog();
void setup_analyzer_tests_no_source();
void setup_analyzer_tests_before_firstpass();
void setup_analyzer_tests_before_firstpass_with_filelog();
void teardown_analyzer_tests();
void teardown_analyzer_tests_before_firstpass();

#define testsupport_show_front_errors(msg_)                             \
    do {                                                                \
        struct RFstringx *tmp_ = front_testdriver_geterrors(get_front_testdriver()); \
        if (tmp_) {                                                     \
            ck_abort_msg(msg_ " with errors:\n"RFS_PF, RFS_PA(tmp_));   \
        } else {                                                        \
            ck_abort_msg(msg_" with no specific errors");               \
        }                                                               \
    } while(0)

#define testsupport_scan_and_parse()                                    \
    do {                                                                \
        if (!compiler_preprocess_fronts()) {                            \
            testsupport_show_front_errors("Scanning/parsing failed");   \
        }                                                               \
} while (0)

#define testsupport_analyzer_prepare()                                  \
    do {                                                                \
        testsupport_scan_and_parse();                                   \
        ck_assert(module_symbol_table_init(front_testdriver_module())); \
    } while(0)

#define testsupport_symbol_table_add_node(st_, id_, node_)              \
    do {                                                                \
        ck_assert(symbol_table_add_node(st_, front_testdriver_module(), id_, node_)); \
    } while(0)

#define testsupport_symbol_table_lookup_node(st_, idstring_, retval_, first_st) \
    do {                                                                \
        bool at_first_st;                                               \
        retval_ = symbol_table_lookup_node(st_, idstring_, &at_first_st); \
        ck_assert_msg(retval_, "Symbol table lookup failed");           \
        ck_assert_msg(first_st == at_first_st,                          \
                      "symbol found but not, in the expected table");   \
    } while(0)

#define testsupport_symbol_table_lookup_record(st_, idstring_, retval_, first_st) \
    do {                                                                \
        bool at_first_st;                                               \
        retval_ = symbol_table_lookup_record(st_, idstring_, &at_first_st); \
        ck_assert_msg(retval_, "Symbol table lookup failed");           \
        ck_assert_msg(first_st == at_first_st,                          \
                      "symbol found but not, in the expected table");   \
    } while(0)

#define testsupport_types_equal(got_, expect_)                          \
    do {                                                                \
        ck_assert_msg(                                                  \
            type_compare(got_, expect_, TYPECMP_GENERIC),               \
            "expected type mismatch during generic comparison. Expected \"" \
            RFS_PF"\" but got \"" RFS_PF"\" which caused a mismatch."RFS_PF, \
            RFS_PA(type_str_or_die(expect_, TSTR_DEFAULT)),             \
            RFS_PA(type_str_or_die(got_, TSTR_DEFAULT)),                \
            RFS_PA(typecmp_ctx_get_error())                             \
        );                                                              \
    } while(0)

#define testsupport_types_same(got_, expect_)                           \
    do {                                                                \
        ck_assert_msg(                                                  \
            type_compare(got_, expect_, TYPECMP_IDENTICAL),             \
            "expected type mismatch during identical comparison. Expected \"" \
            RFS_PF"\" but got \"" RFS_PF"\" which caused a mismatch."RFS_PF, \
            RFS_PA(type_str_or_die(expect_, TSTR_DEFAULT)),             \
            RFS_PA(type_str_or_die(got_, TSTR_DEFAULT)),                \
            RFS_PA(typecmp_ctx_get_error())                             \
        );                                                              \
    } while(0)


struct type *testsupport_analyzer_type_create_simple_elementary(enum elementary_type etype);


#define testsupport_analyzer_type_create_operator(i_optype_, ...)       \
    i_testsupport_analyzer_type_create_operator(i_optype_, RF_NARG(__VA_ARGS__), __VA_ARGS__)
struct type *i_testsupport_analyzer_type_create_operator(
    enum typeop_type type,
    unsigned int argsn,
    ...
);

struct type *testsupport_analyzer_type_create_defined(
    const struct RFstring *name,
    struct type *type
);

struct type *testsupport_analyzer_type_create_function(
    struct type *arg,
    struct type *ret
);

#define testsupport_analyzer_type_create_array(mtype_, arr_)            \
    i_testsupport_analyzer_type_create_array(mtype_, arr_, sizeof(arr_))

struct type *i_testsupport_analyzer_type_create_array(
    const struct type *member_type,
    int64_t *arr,
    size_t arr_size
);

#define testsupport_analyzer_type_create_elemarray(etype_, arr_)    \
    i_testsupport_analyzer_type_create_array(                       \
        testsupport_analyzer_type_create_simple_elementary(etype_), \
        arr_,                                                       \
        sizeof(arr_)                                                \
    )

/* -- general analyzer/front context of the compiler support*/

#define ck_assert_analyzer_errors(expected_arr_)        \
    ck_assert_analyzer_errors_impl(                     \
        expected_arr_,                                  \
        sizeof(expected_arr_)/sizeof(struct info_msg),  \
        __FILE__, __LINE__)
bool ck_assert_analyzer_errors_impl(
    struct info_msg *errors,
    unsigned num,
    const char *filename,
    unsigned int line
);



//! Perform up to the parsing/finalizing stage and check for messages
#define ck_test_parse_fronts(expected_result_, expected_msgs_)          \
    do {                                                                \
        ck_assert_msg(expected_result_ == compiler_preprocess_fronts(), \
                      "unexpected front parsing result");               \
        ck_assert_analyzer_errors(expected_msgs_);                      \
    } while (0)

/* -- typecheck related support -- */

//! Assert all of the front context processing including typechecking is done
#define ck_assert_typecheck_ok()                                        \
    do {                                                                \
        testsupport_scan_and_parse();                                   \
        if (!compiler_analyze()) {                                      \
            testsupport_show_front_errors("Typechecking failed");       \
        }                                                               \
    } while(0)

#define ck_assert_typecheck_with_messages(success_, expected_msgs_)     \
    do {                                                                \
        testsupport_scan_and_parse();                                   \
        if (success_ != compiler_analyze()) {                           \
            testsupport_show_front_errors("Typechecking result was unexpected"); \
        }                                                               \
        ck_assert_analyzer_errors(expected_msgs_);                      \
    } while(0)

/* -- module dependencies related support -- */

/**
 * Assert a specific module dependency order
 * @param expected_modules_   An array of strings with the names of the modules
 *                            in the expected order
 */
#define ck_assert_modules_order(expected_modules_)                      \
    do {                                                                \
        ck_assert_typecheck_ok();                                       \
        i_ck_assert_modules_order(expected_modules_,                    \
                                  sizeof(expected_modules_) / sizeof(struct RFstring), \
                                  __FILE__,                             \
                                  __LINE__);                            \
    } while(0)

void i_ck_assert_modules_order(const struct RFstring *expected_module_names,
                               unsigned int expected_num,
                               const char *filename,
                               unsigned int line);

/**
 * Macro to check expected cyclic dependency error after compiler preprocessing
 */
#define ck_assert_modules_cyclic_dependency_detected(findex_, expected_msg_, sl_, sc_, el_, ec_) \
    do {                                                                \
        struct info_msg expected_msgs_[] = {                            \
            TESTSUPPORT_INFOMSG_INIT_BOTH_SPECIFIC_FRONT(               \
                findex_,                                                \
                MESSAGE_SEMANTIC_ERROR,                                 \
                expected_msg_, sl_, sc_, el_, ec_)                      \
        };                                                              \
        ck_assert_msg(!compiler_preprocess_fronts(),                    \
                      "Expected a modules cyclic dependency but no error detected at preprocess_fronts()"); \
        ck_assert_analyzer_errors(expected_msgs_);                      \
    } while (0)

#endif
