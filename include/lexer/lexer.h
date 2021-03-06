#ifndef LFR_LEXER_H
#define LFR_LEXER_H

#include <stdbool.h>

#include <utils/common.h>
#include <ast/identifier.h>
#include <lexer/tokens.h>
#include <ir/parser/rirtoken.h>


struct inpfile;

struct lexer_vtable;

struct lexer {
    struct lexer_vtable *vt;
    struct {darray(struct token);} tokens;
    struct {darray(int);} indices;
    unsigned int tok_index;
    struct inpfile *file;
    struct info_ctx *info;
    //! Denotes that the lexer has reached the end of its input
    bool at_eof;
};


bool lexer_init(struct lexer *l, struct inpfile *f, struct info_ctx *info, enum rir_pos pos);
struct lexer *lexer_create(struct inpfile *f, struct info_ctx *info, enum rir_pos pos);
void lexer_deinit(struct lexer *l);
void lexer_destroy(struct lexer *l);

bool lexer_scan(struct lexer *l);

struct token *lexer_curr_token(const struct lexer *l);
/**
 * Return current token and then move to the next token
 */
struct token *lexer_curr_token_advance(struct lexer *l);
/**
 * Move to the next token and return it
 */
struct token *lexer_next_token(struct lexer *l);
struct token *lexer_lookahead(struct lexer *l, unsigned int num);
struct token *lexer_lookback(struct lexer *l, unsigned int num);
struct token *lexer_last_token_valid(struct lexer *l);

bool lexer_token_has_value(const struct lexer *l, struct token *tok);
i_INLINE_DECL struct ast_node *lexer_token_get_value_impl(
    const struct lexer *l,
    struct token *tok,
    bool remove_from_lexer)
{
    RF_ASSERT(lexer_token_has_value(l, tok), "Requesting value of illegal token type");
    tok->value.owned_by_lexer = !remove_from_lexer;
    return tok->value.value.ast;
}

#define lexer_token_get_value(lexer_, token_)               \
    lexer_token_get_value_impl((lexer_), (token_), true)    \

#define lexer_token_get_value_but_keep_ownership(lexer_, token_)    \
    lexer_token_get_value_impl((lexer_), (token_), false)

/**
 * Consumes and returns the next token iff it's a token of @c type
*/
i_INLINE_DECL struct token *lexer_expect_token(struct lexer *l, unsigned int type)
{
    struct token *tok = lexer_lookahead(l, 1);
    return (tok && tok->type == type && lexer_curr_token_advance(l)) ? tok : NULL;
}

i_INLINE_DECL struct inplocation *lexer_last_token_location(struct lexer *l)
{
    return &(lexer_last_token_valid(l))->location;
}

i_INLINE_DECL struct inplocation_mark *lexer_last_token_start(struct lexer *l)
{
    return &(lexer_last_token_valid(l))->location.start;
}

i_INLINE_DECL struct inplocation_mark *lexer_last_token_end(struct lexer *l)
{
    return &(lexer_last_token_valid(l))->location.end;
}

i_INLINE_DECL void lexer_inject_input_file(struct lexer *l, struct inpfile *f)
{
    l->file = f;
}

void lexer_push(struct lexer *l);
void lexer_pop(struct lexer *l);
void lexer_rollback(struct lexer *l);

bool lexer_scan(struct lexer *l);

#endif
