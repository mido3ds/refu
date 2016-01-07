#include <ir/parser/rirparser.h>

#include <lexer/lexer.h>
#include <ast/constants.h>
#include <ir/parser/rirtoken.h>
#include <ir/rir_value.h>
#include <ir/rir_constant.h>
#include <ir/rir_object.h>
#include <ir/rir.h>

struct rir_value *rir_parse_value(struct rir_parser *p, struct rir *r, const char *msg)
{
    struct token *tok;
    if (!(tok = lexer_lookahead(&p->lexer, 1))) {
        return false;
    }

    switch(rir_toktype(tok)) {
    case RIR_TOK_CONTANT_INTEGER:
    {
        int64_t v;
        if (!ast_constant_get_integer(&tok->value.value.ast->constant, &v)) {
            RF_ERROR("Failed to convert ast constant to int");
            return false;
        }
        return rir_constantval_create_fromint64(v, r);
    }
    case RIR_TOK_CONSTANT_FLOAT:
    {
        double v;
        if (!ast_constant_get_float(&tok->value.value.ast->constant, &v)) {
            RF_ERROR("Failed to convert ast constant to float");
            return false;
        }
        return rir_constantval_create_fromint64(v, r);
    }
    default:
        rirparser_synerr(
            p,
            token_get_start(tok),
            NULL,
            "Expected a rir value but got token \""RF_STR_PF_FMT"\" %s.",
            RF_STR_PF_ARG(rir_tokentype_to_str(rir_toktype(tok))),
            msg
        );
        break;
    }

    return NULL;
}