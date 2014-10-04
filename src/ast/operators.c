#include <ast/operators.h>

#include <lexer/tokens.h>
#include <lexer/lexer.h>

struct ast_node *ast_binaryop_create(struct inplocation_mark *start,
                                     struct inplocation_mark *end,
                                     enum binaryop_type type,
                                     struct ast_node *left,
                                     struct ast_node *right)
{
    struct ast_node *ret;
    ret = ast_node_create_marks(AST_BINARY_OPERATOR, start, end);
    if (!ret) {
        RF_ERRNOMEM();
        return NULL;
    }

    ret->binaryop.type = type;
    ast_node_register_child(ret, left, binaryop.left);
    ast_node_register_child(ret, right, binaryop.right);
    
    return ret;
}

i_INLINE_INS void ast_binaryop_set_right(struct ast_node *op, struct ast_node *r);
i_INLINE_INS enum binaryop_type ast_binaryop_op(struct ast_node *op);

static const enum binaryop_type  bop_type_lookup[] = {
    [TOKEN_OP_PLUS]     =   BINARYOP_ADD,
    [TOKEN_OP_MINUS]    =   BINARYOP_SUB,
    [TOKEN_OP_MULTI]    =   BINARYOP_MUL,
    [TOKEN_OP_DIV]      =   BINARYOP_DIV,

    [TOKEN_OP_ASSIGN]   =   BINARYOP_ASSIGN
};

enum binaryop_type binaryop_type_from_token(struct token *tok)
{
    RF_ASSERT(TOKEN_IS_BINARY_OP(tok));
    return bop_type_lookup[tok->type];
}

static const enum token_type  token_type_lookup[] = {
    [BINARYOP_ADD]     =   TOKEN_OP_PLUS,
    [BINARYOP_SUB]     =   TOKEN_OP_MINUS,
    [BINARYOP_MUL]     =   TOKEN_OP_MULTI,
    [BINARYOP_DIV]     =   TOKEN_OP_DIV,

    [BINARYOP_ASSIGN]  =   TOKEN_OP_ASSIGN
};

const struct RFstring * ast_binaryop_opstr(struct ast_node *op)
{
    RF_ASSERT(op->type == AST_BINARY_OPERATOR);
    return tokentype_to_str(token_type_lookup[op->binaryop.type]);
}

/* -- unary operator related functions -- */

struct ast_node *ast_unaryop_create(struct inplocation_mark *start,
                                    struct inplocation_mark *end,
                                    enum unaryop_type type,
                                    struct ast_node *operand)
{
    struct ast_node *ret;
    ret = ast_node_create_marks(AST_UNARY_OPERATOR, start, end);
    if (!ret) {
        RF_ERRNOMEM();
        return NULL;
    }

    ret->unaryop.type = type;
    ast_node_register_child(ret, operand, unaryop.operand);
    
    return ret;
}

static const enum unaryop_type  uop_type_lookup[] = {
    [TOKEN_OP_AMPERSAND]  =   UNARYOP_AMPERSAND,
    [TOKEN_OP_INC]        =   UNARYOP_INC,
    [TOKEN_OP_DEC]        =   UNARYOP_DEC
};

enum unaryop_type unaryop_type_from_token(struct token *tok)
{
    RF_ASSERT(TOKEN_IS_UNARY_OP(tok));
    return uop_type_lookup[tok->type];
}