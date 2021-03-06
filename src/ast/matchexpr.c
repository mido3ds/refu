#include <ast/matchexpr.h>
#include <types/type.h>
#include <types/type_operators.h>

struct ast_node *ast_matchcase_create(const struct inplocation_mark *start,
                                      const struct inplocation_mark *end,
                                      struct ast_node *pattern,
                                      struct ast_node *expression)
{
    struct ast_node *ret;
    ret = ast_node_create_marks(AST_MATCH_CASE, start, end);
    if (!ret) {
        return NULL;
    }

    ast_node_register_child(ret, pattern, matchcase.pattern);
    ast_node_register_child(ret, expression, matchcase.expression);
    ret->matchcase.matched_type = NULL;
    ret->matchcase.match_idx = -1;

    return ret;
}

i_INLINE_INS struct ast_node *ast_matchcase_pattern(const struct ast_node *n);
i_INLINE_INS const struct type *ast_matchcase_matched_type(const struct ast_node *n);
i_INLINE_INS struct ast_node *ast_matchcase_expression(const struct ast_node *n);
i_INLINE_INS struct symbol_table *ast_matchcase_symbol_table_get(const struct ast_node *n);
i_INLINE_INS void *ast_matchcase_symbol_table_set(struct ast_node *n, struct symbol_table *st);
i_INLINE_INS int ast_matchcase_index_get(const struct ast_node *n);

struct ast_node *ast_matchexpr_create(const struct inplocation_mark *start,
                                      const struct inplocation_mark *end,
                                      struct ast_node *id)
{
    struct ast_node *ret;
    ret = ast_node_create_marks(AST_MATCH_EXPRESSION, start, end);
    if (!ret) {
        return NULL;
    }
    ret->matchexpr.match_cases_num = 0;
    ret->matchexpr.matching_type = NULL;
    if (id) {
        ast_node_register_child(ret, id, matchexpr.identifier_or_fnargtype);
    } else {
        ret->matchexpr.identifier_or_fnargtype = NULL;
    }
    return ret;
}

i_INLINE_INS size_t ast_matchexpr_cases_num(const struct ast_node *n);
i_INLINE_INS bool ast_matchexpr_is_bodyless(const struct ast_node *n);
i_INLINE_INS bool ast_matchexpr_has_header(const struct ast_node *n);
i_INLINE_INS struct ast_node *ast_matchexpr_identifier(const struct ast_node *n);
i_INLINE_INS struct ast_node *ast_matchexpr_headless_args(const struct ast_node *n);
i_INLINE_INS void ast_matchexpr_set_fnargs(struct ast_node *n,
                                           struct ast_node *fn_args);

struct symbol_table_record* ast_matchexpr_headless_strec(const struct ast_node *n,
                                                         const struct symbol_table *st)
{
    bool at_first;
    struct symbol_table_record *rec = symbol_table_lookup_typedesc(
        st,
        ast_matchexpr_headless_args(n),
        &at_first
    );
    RF_ASSERT(rec, "Headless typedesc should have been found");
    return rec;
}

const struct type *ast_matchexpr_matched_type_compute(struct ast_node *n,
                                                      const struct symbol_table *st)
{
    // make sure we don't get two times in here
    RF_ASSERT(!n->matchexpr.matching_type, "A match expression's matching type has already been computed");
    const struct type *matching_type = ast_matchexpr_has_header(n)
       ? matching_type = type_lookup_identifier_string(
           ast_identifier_str(n->matchexpr.identifier_or_fnargtype),
           st
       )
       :
       // else it's the type of the function arguments themselves
       ast_node_get_type(n->matchexpr.identifier_or_fnargtype);
    if (!matching_type) {
        RF_ERROR("Failure to compute a matchexpression's matching type");
        return NULL;
    }
    n->matchexpr.matching_type = matching_type;
    return matching_type;
}

const struct RFstring *ast_matchexpr_matched_type_str(const struct ast_node *n)
{
    if (ast_matchexpr_has_header(n)) {
        return ast_identifier_str(ast_matchexpr_identifier(n));
    }
    // else it's the type of the function arguments themselves
    return type_str_or_die(
        ast_node_get_type_or_die(n->matchexpr.identifier_or_fnargtype),
        TSTR_DEFAULT
    );
}

i_INLINE_INS const struct type *ast_matchexpr_matched_type(const struct ast_node *n);

const struct RFstring *ast_matchexpr_matched_value_str(const struct ast_node *n)
{
    if (ast_matchexpr_has_header(n)) {
        return ast_identifier_str(ast_matchexpr_identifier(n));
    }
    // else it's an anonymous type matching
    return type_get_unique_type_str(
        ast_node_get_type_or_die(
            n->matchexpr.identifier_or_fnargtype
        )
    );
}

bool ast_matchexpr_cases_indices_set(struct ast_node *n)
{
    struct ast_matchexpr_it it;
    struct ast_node *mcase;
    struct {darray(int);} visited_indices;
    bool ret = false;
    darray_init(visited_indices);

    ast_matchexpr_foreach(n, &it, mcase) {
        int index = type_is_childof(
            mcase->matchcase.matched_type,
            ast_matchexpr_matched_type(n)
        );
        if (index == -1) {
            RF_ERROR("Failed to match a case's type to the matchexpr type during RIR formation");
            goto end;
        }
        // Check if that index was already used in this expression
        // TODO: Yes I know, linear array search is not the best thing to
        //  do here but if this is ever a bottleneck a set of ints can be used
        int *idx;
        darray_foreach(idx, visited_indices) {
            if (*idx == index) {
                RF_ERROR("A match expression's index was used by two different cases. Should never happen");
                goto end;
            }
        }
        darray_append(visited_indices, index);
        mcase->matchcase.match_idx = index;
    }
    //success
    ret = true;
end:
    darray_free(visited_indices);
    return ret;
}

void ast_matchexpr_add_case(struct ast_node *n, struct ast_node *mcase)
{
    ast_node_add_child(n, mcase);
    ++n->matchexpr.match_cases_num;
}

struct ast_node *ast_matchexpr_first_case(const struct ast_node *n,
                                          struct ast_matchexpr_it *it)
{
    AST_NODE_ASSERT_TYPE(n, AST_MATCH_EXPRESSION);
    // get second child if we have a matched identifier so that we
    // start from the first matchase
    it->idx = ast_matchexpr_has_header(n) ? 1 : 0;
    it->cases = &n->children;
    return darray_item(n->children, it->idx);
}

bool ast_match_expr_next_case_is_last(const struct ast_node *matchexpr,
                                      struct ast_matchexpr_it *it)
{
    AST_NODE_ASSERT_TYPE(matchexpr, AST_MATCH_EXPRESSION);
    return it->idx + 1 == darray_size(*it->cases);
}

struct ast_node *ast_matchexpr_next_case(const struct ast_node *n,
                                         struct ast_matchexpr_it *it)
{
    AST_NODE_ASSERT_TYPE(n, AST_MATCH_EXPRESSION);
    if (it->idx + 1 >= darray_size(*it->cases)) {
        return NULL;
    }
    return darray_item(*it->cases, ++it->idx);
}

struct ast_node *ast_matchexpr_get_case(const struct ast_node *n, unsigned int i)
{
    AST_NODE_ASSERT_TYPE(n, AST_MATCH_EXPRESSION);
    struct ast_matchexpr_it it;
    unsigned int count = 0;
    struct ast_node *mcase;
    ast_matchexpr_foreach(n, &it, mcase) {
        if (count++ == i) {
            return mcase;
        }
    }
    return NULL;
}
