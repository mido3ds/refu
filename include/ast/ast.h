#ifndef LFR_AST_H
#define LFR_AST_H

#include <RFintrusive_list.h>
#include <RFstring.h>

#include <ast/location.h>
#include <ast/identifier.h>
#include <ast/datadecl.h>
#include <ast/genrdecl.h>
#include <ast/vardecl.h>
#include <ast/fndecl.h>

#define AST_PRINT_DEPTHMUL 4

enum ast_type {
    AST_ROOT = 0,
    AST_BLOCK,
    AST_VARIABLE_DECLARATION,
    AST_DATA_DECLARATION,
    AST_DATA_OPERATOR,
    AST_DATA_DESCRIPTION,
    AST_GENERIC_DECLARATION,
    AST_GENERIC_TYPE,
    AST_FUNCTION_DECLARATION,

    /* from this value and under all types should have no children */
    AST_STRING_LITERAL,
    AST_IDENTIFIER,
    AST_TYPES_COUNT /* always last */
};

struct ast_node {
    enum ast_type type;
    struct ast_location location;
    struct RFilist_node lh;
    union {
        struct RFstring identifier;
        struct ast_vardecl vardecl;
        struct ast_datadecl datadecl;
        struct ast_dataop dataop;
        struct ast_datadesc datadesc;
        struct ast_genrdecl genrdecl;
        struct ast_genrtype genrtype;
        struct ast_fndecl fndecl;

        struct RFilist_head children;
    };
};

struct ast_node *ast_node_create(enum ast_type type,
                                 struct parser_file *f,
                                 char *sp, char *ep);

void ast_node_destroy(struct ast_node *n);

bool ast_node_set_end(struct ast_node *n, char *end);

void ast_node_add_child(struct ast_node *parent,
                        struct ast_node *child);

const struct RFstring *ast_node_str(struct ast_node *n);

// temporary function, to visualize an ast tree
void ast_print(struct ast_node *root, int depth, const char *description);
#endif
