#include "function.h"

#include <ast/function.h>
#include <ast/matchexpr.h>

#include <parser/parser.h>
#include "common.h"
#include "identifier.h"
#include "generics.h"
#include "type.h"
#include "expression.h"
#include "block.h"
#include "matchexpr.h"

struct ast_node *ast_parser_acc_fndecl(struct ast_parser *p, int fndecl_position)
{
    struct ast_node *n;
    struct token *tok;
    struct token *oparen_tok;
    struct ast_node *name;
    struct ast_node *genr = NULL;
    struct ast_node *args = NULL;
    struct ast_node *ret_type = NULL;
    const struct inplocation_mark *start;
    const struct inplocation_mark *end;

    lexer_push(parser_lexer(p));
    tok = lexer_lookahead(parser_lexer(p), 1);
    if (fndecl_position != FNDECL_PARTOF_FOREIGN_IMPORT) {
        if (!tok || tok->type != TOKEN_KW_FUNCTION) {
            goto err;
        }
        //consume function keyword
        lexer_curr_token_advance(parser_lexer(p));
    }
    // start should be either start of fn, or start of next token (an identifier)
    start = token_get_start(tok);

    name = ast_parser_acc_identifier(p);
    if (!name) {
        parser_synerr(p, lexer_last_token_start(parser_lexer(p)), NULL,
                      "Expected an identifier for the function name after 'fn'");
        goto err;
    }

    tok = lexer_lookahead(parser_lexer(p), 1);
    if (GENRDECL_START_COND(tok)) {
        genr = ast_parser_acc_genrdecl(p);
        if (!genr) {
            goto err;
        }
    }

    tok = lexer_curr_token_advance(parser_lexer(p));
    if (!tok || tok->type != TOKEN_SM_OPAREN) {
        parser_synerr(p, lexer_last_token_start(parser_lexer(p)), NULL,
                      "Expected '(' at function declaration");
        goto err_free_genr;
    }
    oparen_tok = tok;

    args = ast_parser_acc_typedesc(p);
    if (!args && ast_parser_has_syntax_error_reset(p)) {
        parser_synerr(p, token_get_end(tok), NULL,
                      "Expected either a type description for the function's "
                      "arguments or ')' after '('");
        goto err_free_genr;
    }

    tok = lexer_curr_token_advance(parser_lexer(p));
    if (!tok || tok->type != TOKEN_SM_CPAREN) {
        if (args) {
            parser_synerr(p, lexer_last_token_end(parser_lexer(p)), NULL,
                          "Expected ')' at function declaration after "
                          "type description");
        } else {
            parser_synerr(p, token_get_end(oparen_tok), NULL,
                          "Expected ')' at function declaration after '('");
        }
        goto err_free_args;
    }

    end = token_get_end(tok);
    tok = lexer_lookahead(parser_lexer(p), 1);
    if (tok && tok->type == TOKEN_OP_IMPL) {
        //consume '->'
        lexer_curr_token_advance(parser_lexer(p));
        ret_type = ast_parser_acc_typedesc(p);
        if (!ret_type) {
            parser_synerr(p, token_get_end(tok), NULL,
                          "Expected type description for the function's "
                          "return type after '->'");
            goto err_free_args;
        }
        end = ast_node_endmark(ret_type);
    }

    n = ast_fndecl_create(start, end, fndecl_position, name, genr, args, ret_type);
    if (!n) {
        RF_ERRNOMEM();
        goto err_free_rettype;
    }
    lexer_pop(parser_lexer(p));
    return n;

err_free_rettype:
    if (ret_type) {
        ast_node_destroy(ret_type);
    }
err_free_args:
    if (args) {
    ast_node_destroy(args);
    }
err_free_genr:
    if (genr) {
        ast_node_destroy(genr);
    }
err:
    lexer_rollback(parser_lexer(p));
    return NULL;
}


enum parser_fndecl_list_err ast_parser_acc_fndecl_list(
    struct ast_parser *p,
    struct ast_node *parent,
    int fndecl_position
)
{
    struct ast_node *decl;
    struct token *tok;
    tok = lexer_lookahead(parser_lexer(p), 1);

    if (!tok || tok->type != TOKEN_KW_FUNCTION) {
        return PARSER_FNDECL_LIST_EMPTY;
    }

    while (tok && tok->type == TOKEN_KW_FUNCTION) {
        decl = ast_parser_acc_fndecl(p, fndecl_position);
        if (!decl) {
            return PARSER_FNDECL_LIST_FAILURE;
        }
        ast_node_add_child(parent, decl);
        tok = lexer_lookahead(parser_lexer(p), 1);
    }

    return PARSER_FNDECL_LIST_SUCCESS;
}

struct ast_node *ast_parser_acc_fnimpl(struct ast_parser *p)
{
    struct ast_node *n;
    struct ast_node *decl;
    struct ast_node *body;
    struct token *tok;

    decl = ast_parser_acc_fndecl(p, FNDECL_PARTOF_IMPL);
    if (!decl) {
        return NULL;
    }    
    tok = lexer_lookahead(parser_lexer(p), 1);
    if (!tok) {
        goto fail_free_decl;
    }

    if (tok->type == TOKEN_SM_OCBRACE) {
        // normal function body
        body = ast_parser_acc_block(p, true);
        if (!body) {
            parser_synerr(
                p, ast_node_endmark(decl), NULL,
                "Expected a body for \""RFS_PF"\" function "
                "implementation",
                RFS_PA(ast_fndecl_name_str(decl))
            );
            goto fail_free_decl;
        }
    } else {
        // attempt to find a headless match expression as the function body
        body = ast_parser_acc_matchexpr(p, false, true);
        if (!body) {
            parser_synerr(
                p, ast_node_endmark(decl), NULL,
                "Expected a body for \""RFS_PF"\" function "
                "implementation",
                RFS_PA(ast_fndecl_name_str(decl))
            );
            goto fail_free_decl;
        }
        // now set the matchexpr's fn_args since this is a headless matchexpr
        ast_matchexpr_set_fnargs(body, ast_fndecl_args_get(decl));
    }
        
    n = ast_fnimpl_create(ast_node_startmark(decl),
                          ast_node_endmark(body),
                          decl,
                          body);
    if (!n) {
        RF_ERRNOMEM();
        ast_node_destroy(body);
        ast_node_destroy(decl);
        return NULL;
    }
    return n;

fail_free_decl:
    ast_node_destroy(decl);
    return NULL;
}

enum parser_fnimpl_list_err ast_parser_acc_fnimpl_list(struct ast_parser *p,
                                                       struct ast_node *parent)
{
   struct ast_node *impl;
   struct token *tok;
   tok = lexer_lookahead(parser_lexer(p), 1);

   if (!tok || tok->type != TOKEN_KW_FUNCTION) {
       return PARSER_FNIMPL_LIST_EMPTY;
   }

   while (tok && tok->type == TOKEN_KW_FUNCTION) {
       impl = ast_parser_acc_fnimpl(p);
       if (!impl) {
           return PARSER_FNIMPL_LIST_FAILURE;
       }
       ast_node_add_child(parent, impl);
       tok = lexer_lookahead(parser_lexer(p), 1);
   }

   return PARSER_FNIMPL_LIST_SUCCESS;
}

struct ast_node *ast_parser_acc_fncall(struct ast_parser *p, bool expect_it)
{
    struct ast_node *n;
    struct token *tok;
    struct ast_node *name;
    struct ast_node *genr = NULL;
    struct ast_node *args = NULL;
    lexer_push(parser_lexer(p));

    name = ast_parser_acc_identifier(p);
    if (!name) {
        goto err;
    }

    genr = ast_parser_acc_genrattr(p, false);
    if (!genr && ast_parser_has_syntax_error(p)) {
        // name is an identifier and even in failure does not need to get destroyed here
        goto err;
    }

    tok = lexer_lookahead(parser_lexer(p), 1);
    if (!tok || tok->type != TOKEN_SM_OPAREN) {
        if (expect_it) {
            parser_synerr(p, lexer_last_token_start(parser_lexer(p)), NULL,
                          "Expected '('");
        }
        goto err_free_genr;
    }
    //consume '('
    lexer_curr_token_advance(parser_lexer(p));

    args = ast_parser_acc_expression(p);
    if (!args) {
        if (ast_parser_has_syntax_error(p)) {
                parser_synerr(
                    p, lexer_last_token_start(parser_lexer(p)), NULL,
                    "Expected argument expression for function call"
                );
            goto err_free_genr;
        }
    }

    tok = lexer_lookahead(parser_lexer(p), 1);
    if (!tok || tok->type != TOKEN_SM_CPAREN) {
        if (expect_it) {
            parser_synerr(
                p, lexer_last_token_end(parser_lexer(p)), NULL,
                "Expected ')' at end of "RFS_PF" function call",
                RFS_PA(ast_identifier_str(name))
            );
        }
        goto err_free_args;
    }
    //consume ')'
    lexer_curr_token_advance(parser_lexer(p));

    n = ast_fncall_create(ast_node_startmark(name), token_get_end(tok),
                          name, args, genr);
    if (!n) {
        RF_ERRNOMEM();
        goto err_free_args;
    }

    lexer_pop(parser_lexer(p));
    return n;

err_free_args:
    if (args) {
        ast_node_destroy(args);
    }
err_free_genr:
    if (genr) {
        ast_node_destroy(genr);
    }
err:
    lexer_rollback(parser_lexer(p));
    return NULL;
}
