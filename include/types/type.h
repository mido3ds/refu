#ifndef LFR_TYPES_TYPE_H
#define LFR_TYPES_TYPE_H

#include <Utils/sanity.h>
#include <Definitions/inline.h>
#include <String/rf_str_decl.h>

#include <utils/traversal.h>
#include <types/type_decls.h>

struct analyzer;
struct symbol_table;
struct RFbuffer;

extern const struct RFstring g_wildcard_s;

/* -- type allocation functions -- */

struct type *type_alloc(struct analyzer *a);
void type_free(struct type *t, struct analyzer *a);

/* -- various type creation and initialization functions -- */

struct type *type_create_from_node(struct ast_node *n, struct analyzer *a,
                                   struct symbol_table *st,
                                   struct ast_node *genrdecl,
                                   bool lookup);

struct type *type_create_from_typedecl(struct ast_node *n,
                                       struct analyzer *a,
                                       struct symbol_table *st);

struct type *type_create_from_fndecl(struct ast_node *n,
                                     struct analyzer *a,
                                     struct symbol_table *st);

struct type *type_function_create(struct analyzer *a,
                                  struct type *arg_type,
                                  struct type *ret_type);

struct type *type_create_from_typedesc(struct ast_node *typedesc,
                                       struct analyzer *a,
                                       struct symbol_table *st,
                                       struct ast_node *genrdecl,
                                       bool lookup);

struct type *type_operator_create(struct analyzer *a,
                                  struct type *left_type,
                                  struct type *right_type,
                                  enum typeop_type type);

struct type *type_operator_create_from_node(struct ast_node *n,
                                            struct analyzer *a,
                                            struct symbol_table *st,
                                            struct ast_node *genrdecl,
                                            bool lookup);

struct type *type_leaf_create(struct analyzer *a,
                              const struct RFstring *id,
                              struct type *leaf_type);


/**
 * Attempts to retrieve the type for ast node @c n and if it does not exist
 * it creates it and if @c add_type is true is adds it to the types list.
 *
 * @note: If @c n is an ast description of a single type_leaf say a:f64 this is
 *        the kind of type this should return
 *
 * @param n            The node whose type to retrieve/create
 * @param a            The analyzer instance for which to do it
 * @param st           The symbol table to check for the type
 * @param genrdecl     An optional generic declaration node that describes @c n.
 *                     Can be NULL.
 * @param make_leaf    If true and @c n is a type description of a single
 *                     type_leaf say a:f64 then this will return a type leaf.
 *                     If false then this will just return the right part of the
 *                     description
 * @param add_type     If @c true and the created type is not already in the list
 *                     then add it
 * @return             Return either the type of @c n or NULL if there was an error
 */
struct type *type_lookup_or_create(struct ast_node *n,
                                   struct analyzer *a,
                                   struct symbol_table *st,
                                   struct ast_node *genrdecl,
                                   bool make_leaf,
                                   bool add_type_to_list);

/**
 * Applies a type operator to 2 types and returns the result
 * @param type          The type operator to apply to @c left and @c right
 * @param left          The type to become left part of the operand
 * @param right         The type to become right part of the operand
 * @param a             The analyzer instance for which we are typechecking
 * @return              The new type or NULL if there was an error
 */
struct type *type_create_from_operation(enum typeop_type type,
                                        struct type *left,
                                        struct type *right,
                                        struct analyzer *a);

struct type *type_lookup_identifier_string(const struct RFstring *str,
                                           struct symbol_table *st);
struct type *type_lookup_xidentifier(struct ast_node *n,
                                     struct analyzer *a,
                                     struct symbol_table *st,
                                     struct ast_node *genrdecl);

/**
 * Gets a string representation of the type
 *
 * Before this function you need to execute use @ref RFS_buffer_push() in order
 * to remember the temporary string buffer position and after it you need to
 * pop it with @ref RFS_buffer_pop().
 *
 * @param t                 The type whose string representation to get
 * @param print_leaf_id     If @c true will also print the identifier of a type
 *                          leaf such as: "age:u32, name:string" instead of
 *                          just "u32, string". For defined types it will also
 *                          print the contents of the types.
 * @return                  Returns a pointer to the the string representation.
 *                          If there is an error returns NULL.
 */
const struct RFstring *type_str(const struct type *t, bool print_leaf_id);

/**
 * Gets a string representation of a type we know is a user defined type
 *
 * In contrast to @ref type_str() this provides details as to the description
 * of the type too.
 */
const struct RFstring *type_defined_to_str(const struct type *t);

/**
 * @returns the wildcard type '_'
 */
const struct type *type_get_wildcard();

/* -- type traversal functions -- */

typedef bool (*type_iterate_cb) (struct type *t, void *user_arg);
typedef bool (*leaf_type_cb) (struct type_leaf *t, void *user_arg);
typedef enum traversal_cb_res (*leaf_type_nostop_cb) (const struct type_leaf *t, void *user_arg);

/**
 * Iterate and call callback for each subtype leaf
 * @param t          The type whose subtypes to iterate
 * @param cb         The callback to call on each type leaf
 * @param user_arg   The input to the callback
 *
 * @return true for success and false if the callback fails anywhere
 */
bool type_for_each_leaf(struct type *t, leaf_type_cb cb, void *user_arg);

/**
 * Behaves just like @c type_for_each_leaf() but has different treatment of
 * callback return.
 * @param t          The type whose subtypes to iterate
 * @param cb         The callback to call on each type leaf
 * @param user_arg   The input to the callback
 *
 * @return true for success and false if the callback fails anywhere
 */
enum traversal_cb_res type_for_each_leaf_nostop(const struct type *t, leaf_type_nostop_cb cb, void *user_arg);

/**
 * Post order iteration of a given type
 * @param t            The type to traverse
 * @param cb           The callback to execute in a post order fashion for each node
 *                     of the type description
 * @param user_arg     An extra argument to provide to the callbacks
 *
 * @return             true if all callbacks returned succesfully and false otherwise
 */
bool type_traverse_postorder(struct type *t, type_iterate_cb cb, void *user_arg);

/**
 * Tteration of a given type, with callbacks both for when going down and up.
 * Traversal should be in order.
 * @param t            The type to traverse
 * @param pre_cb       The callback to execute in a pre order fashion for each node
 *                     of the type description
 * @param post_cb      The callback to execute in a post order fashion for each node
 *                     of the type description
 * @param user_arg     An extra argument to provide to the callbacks
 *
 * @return             true if all callbacks returned succesfully and false otherwise
 */
bool type_traverse(struct type *t, type_iterate_cb pre_cb,
                   type_iterate_cb post_cb, void *user_arg);

#endif
