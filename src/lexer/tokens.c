#include <lexer/tokens.h>

#include <String/rf_str_core.h>
#include <Utils/build_assert.h>

static struct RFstring strings_[] = {
    RF_STRING_STATIC_INIT("identifier"),
    RF_STRING_STATIC_INIT("constant integer"),
    RF_STRING_STATIC_INIT("constant float"),
    RF_STRING_STATIC_INIT("string literal"),

    /* keywords */
    RF_STRING_STATIC_INIT("const"),
    RF_STRING_STATIC_INIT("type"),
    RF_STRING_STATIC_INIT("fn"),
    RF_STRING_STATIC_INIT("class"),
    RF_STRING_STATIC_INIT("instance"),
    RF_STRING_STATIC_INIT("if"),
    RF_STRING_STATIC_INIT("elif"),
    RF_STRING_STATIC_INIT("else"),
    RF_STRING_STATIC_INIT("return"),

    /* symbols */
    RF_STRING_STATIC_INIT(":"),
    RF_STRING_STATIC_INIT("{"),
    RF_STRING_STATIC_INIT("}"),
    RF_STRING_STATIC_INIT("["),
    RF_STRING_STATIC_INIT("]"),
    RF_STRING_STATIC_INIT("("),
    RF_STRING_STATIC_INIT(")"),
    RF_STRING_STATIC_INIT("\""),

    /* binary operators */
    RF_STRING_STATIC_INIT("+"),
    RF_STRING_STATIC_INIT("-"),
    RF_STRING_STATIC_INIT("*"),
    RF_STRING_STATIC_INIT("/"),
    RF_STRING_STATIC_INIT("="),

    /* binary comparison operators */
    RF_STRING_STATIC_INIT("=="),
    RF_STRING_STATIC_INIT("!="),
    RF_STRING_STATIC_INIT(">"),
    RF_STRING_STATIC_INIT(">="),
    RF_STRING_STATIC_INIT("<"),
    RF_STRING_STATIC_INIT("<="),

    /* logic binary operators */
    RF_STRING_STATIC_INIT("&&"),
    RF_STRING_STATIC_INIT("||"),

    /* other binary operators */
    RF_STRING_STATIC_INIT("."),

    /* bitwise binary operators */
    RF_STRING_STATIC_INIT("|"),
    RF_STRING_STATIC_INIT("&"),
    RF_STRING_STATIC_INIT("^"),

    /* unary operators*/
    RF_STRING_STATIC_INIT("&"),
    RF_STRING_STATIC_INIT("++"),
    RF_STRING_STATIC_INIT("--"),

    /* type operators */
    RF_STRING_STATIC_INIT("|"),
    RF_STRING_STATIC_INIT(","),
    RF_STRING_STATIC_INIT("->"),

};

i_INLINE_INS const struct inplocation *token_get_loc(const struct token *tok);
i_INLINE_INS const struct inplocation_mark *token_get_start(const struct token *tok);
i_INLINE_INS const struct inplocation_mark *token_get_end(const struct token *tok);
i_INLINE_INS bool token_has_value(const struct token *tok);
i_INLINE_INS struct ast_node *token_get_value(struct token *tok);

const struct RFstring *tokentype_to_str(enum token_type type)
{
    BUILD_ASSERT(sizeof(strings_) / sizeof(struct RFstring) == TOKENS_MAX);

    return &strings_[type];
}
