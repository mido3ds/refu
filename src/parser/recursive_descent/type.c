#include "type.h"

#include <Utils/sanity.h>

#include <ast/ast.h>
#include <ast/identifier.h>
#include <info/info.h>

#include <parser/parser.h>
#include "common.h"
#include "identifier.h"

static struct ast_node *parser_acc_typefactor(struct parser *p)
{
    struct ast_node *n;
    struct token *tok;
    lexer_push(p->lexer);
    tok = lexer_lookeahead(p->lexer, 1);

    if (!tok) {
        return NULL;
    }

    if (tok->type == TOKEN_SM_OPAREN) {
        //consume parentheses
        lexer_next_token(p->lexer);
        n = parser_acc_typedesc(p);

        tok = lexer_next_token(p->lexer);
        if (!tok || tok->type != TOKEN_SM_CPAREN) {
            parser_synerr(p, lexer_last_token_end(p->lexer), NULL,
                          "expected ')' after type description");
            goto err;
        }
    } else if (tok->type == TOKEN_IDENTIFIER){
        struct ast_node *right;
        struct ast_node *left = token_get_identifier(tok);
        //consume identifier
        lexer_next_token(p->lexer);

        tok = lexer_lookeahead(p->lexer, 1);
        if (!tok || tok->type != TOKEN_SM_COLON) {
            n = ast_typedesc_create(ast_node_startmark(left),
                                    ast_node_endmark(left),
                                    left, NULL);
            if (!n) {
                //TODO: bad error
                goto err;
            }
            return n;
        }

        tok = lexer_lookeahead(p->lexer, 2);
        if (!XIDENTIFIER_START_COND(tok)) {
            parser_synerr(p, lexer_last_token_end(p->lexer), NULL,
                          "expected "XIDENTIFIER_START_STR" after ':'");
            goto err;
        }
        //consume ':'
        lexer_next_token(p->lexer);
        right = parser_acc_xidentifier(p);
        if (!right) {
            //TODO: bad error
            goto err;
        }
        n = ast_typedesc_create(ast_node_startmark(left),
                                ast_node_endmark(right),
                                left, right);
        if (!n) {
            //TODO: bad error
            goto err;
        }
    } else {
        return NULL;
    }

    lexer_pop(p->lexer);
    return n;

err:
    lexer_rollback(p->lexer);
    return NULL;
}

static struct ast_node *parser_acc_typeterm_prime(
    struct parser *p,
    bool *syntax_error,
    struct ast_node *left_hand_side)
{
    struct token *tok;
    struct ast_node *op;
    struct ast_node *right_hand_side;
    struct ast_node *ret;

    *syntax_error = false;
    tok = lexer_lookeahead(p->lexer, 1);
    if (!tok || tok->type != TOKEN_OP_TYPESUM) {
        return NULL;
    }
    //consume TYPESUM
    lexer_next_token(p->lexer);

    lexer_push(p->lexer);

    op = ast_typeop_create(ast_node_startmark(left_hand_side), NULL,
                           TYPEOP_SUM, left_hand_side, NULL);
    if (!op) {
        //TODO: bad error
    }

    right_hand_side = parser_acc_typefactor(p);
    if (!right_hand_side) {
        parser_synerr(p, token_get_end(tok), NULL,
                      "Expected a "TYPEFACTOR_START_STR" after '|'");
        *syntax_error = true;
        ast_node_destroy(op);
        return NULL;
    }
    ast_typeop_set_right(op, right_hand_side);

    ret = parser_acc_typeterm_prime(p, syntax_error, op);
    if (*syntax_error == true) {
        // no need to free term, since it's freed inside typeterm' accepting
        return NULL;
    }

    return ret ? ret : op;
}

static struct ast_node *parser_acc_typeterm(struct parser *p)
{
    struct ast_node *factor;
    struct ast_node *prime;
    bool syntax_error;

    factor = parser_acc_typefactor(p);
    if (!factor) {
        return NULL;
    }
    prime = parser_acc_typeterm_prime(p, &syntax_error, factor);
    if (syntax_error) {
        // no need to free factor, since it's freed inside typeterm' accepting
        return NULL;
    }

    return (prime) ? prime : factor;
}

static struct ast_node *parser_acc_typedesc_prime(
    struct parser *p,
    bool *syntax_error,
    struct ast_node *left_hand_side)
{
    struct token *tok;
    struct ast_node *op;
    struct ast_node *right_hand_side;
    struct ast_node *ret;

    *syntax_error = false;
    tok = lexer_lookeahead(p->lexer, 1);
    if (!tok || tok->type != TOKEN_OP_COMMA) {
        return NULL;
    }
    //consume comma
    lexer_next_token(p->lexer);

    lexer_push(p->lexer);

    op = ast_typeop_create(ast_node_startmark(left_hand_side), NULL,
                           TYPEOP_PRODUCT, left_hand_side, NULL);
    if (!op) {
        //TODO: bad error
        return NULL;
    }
    right_hand_side = parser_acc_typeterm(p);
    if (!right_hand_side) {
        parser_synerr(p, token_get_end(tok), NULL,
                      "Expected a "TYPETERM_START_STR" after ','");
        ast_node_destroy(op);
        *syntax_error = true;
        return NULL;
    }
    ast_typeop_set_right(op, right_hand_side);

    ret = parser_acc_typedesc_prime(p, syntax_error, op);
    if (*syntax_error == true) {
        // no need to free op, since it's freed inside typedesc' accepting
        return NULL;
    }

    return ret ? ret : op;
}

struct ast_node *parser_acc_typedesc(struct parser *p)
{
    struct ast_node *prime;
    struct ast_node *term;
    bool syntax_error;

    term = parser_acc_typeterm(p);
    if (!term) {
        return NULL;
    }

    prime = parser_acc_typedesc_prime(p, &syntax_error, term);
    if (syntax_error) {
        // no need to free term, since it's freed inside typedesc' accepting
        return NULL;
    }

    return prime ? prime : term;
}


struct ast_node *parser_acc_typedecl(struct parser *p)
{
    struct token *tok;
    struct ast_node *data_decl;
    struct ast_node *name;
    struct ast_node *desc;
    struct inplocation_mark *start;

    lexer_push(p->lexer);

    tok = lexer_next_token(p->lexer);
    if (!tok || tok->type != TOKEN_KW_TYPE) {
        goto not_found;
    }
    start = token_get_start(tok);

    name = parser_acc_identifier(p);
    if (!name) {
        goto not_found;
    }

    tok = lexer_next_token(p->lexer);
    if (!tok || tok->type != TOKEN_SM_OCBRACE) {
        goto not_found;
    }

    desc = parser_acc_typedesc(p);
    if (!desc) {
        parser_synerr(p, token_get_end(tok), NULL,
                      "Expected data description for data declaration "
                      "of \""RF_STR_PF_FMT"\"",
                      RF_STR_PF_ARG(ast_identifier_str(name)));
        ast_node_destroy(name);
        goto not_found;
    }

    /* from here and on we throw syntax errors if something goes wrong */
    data_decl = ast_typedecl_create(start, NULL, name, desc);
    if (!data_decl) {//memory error
        ast_node_destroy(name);
        ast_node_destroy(desc);
        goto not_found;
    }

    tok = lexer_next_token(p->lexer);
    if (!tok || tok->type != TOKEN_SM_CCBRACE) {
        parser_synerr(p, lexer_last_token_end(p->lexer), NULL,
                      "Expected a closing brace '}' in data "
                      "declaration for '"RF_STR_PF_FMT"'",
                      RF_STR_PF_ARG(ast_identifier_str(name)));
        goto err_free;
    }
    ast_node_set_end(data_decl, token_get_end(tok));

    lexer_pop(p->lexer);
    return data_decl;

err_free:
    ast_node_destroy(data_decl);
not_found:
    lexer_rollback(p->lexer);
    return NULL;
}