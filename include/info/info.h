#ifndef LFR_INFO_H
#define LFR_INFO_H

#include <stdio.h>

#include <rfbase/datastructs/darray.h>
#include <rfbase/datastructs/intrusive_list.h>
#include <rfbase/string/corex.h>

enum info_msg_type {
    MESSAGE_ANY = 0xFF,
    MESSAGE_SEMANTIC_WARNING = 0x2,
    MESSAGE_SYNTAX_WARNING = 0x4,
    MESSAGE_SEMANTIC_ERROR = 0x8,
    MESSAGE_SYNTAX_ERROR = 0x10,
};
#define INFO_MESSAGE_TYPES_NUM 5

struct inplocation;
struct inplocation_mark;
struct info_msg;

struct info_ctx {
    RFilist_head msg_list;
    size_t msg_num;
    int verbose_level;
    struct RFstringx buff;
    bool syntax_error; /* maybe to avoid searching the whole list? */
    struct {darray(struct info_msg*);} last_msgs_arr;
    // a pointer to the file all the info messages will refer to (not owned)
    struct inpfile *file;
};


struct info_ctx *info_ctx_create(struct inpfile *file);
void info_ctx_destroy(struct info_ctx *ctx);

void info_print_cond(int vlevel, const char *fmt, ...);

bool i_info_ctx_add_msg(struct info_ctx *ctx,
                        enum info_msg_type type,
                        const struct inplocation_mark *start,
                        const struct inplocation_mark *end,
                        const char *fmt,
                        ...);

/**
 * Remove the last @a num messages from the info context
 */
void info_ctx_rem_messages(struct info_ctx *ctx, size_t num);

/**
 * Push the current info messages status so that it can later be either popped or rolled back
 */
void info_ctx_push(struct info_ctx *ctx);
/**
 * Pop the current info message status so that a previous push's effects are undone.
 * Will not delete any messages.
 */
void info_ctx_pop(struct info_ctx *ctx);
/**
 * Rollback the info message status to the last push. Will delete all messages after the last push.
 */
void info_ctx_rollback(struct info_ctx *ctx);

/**
 * Return if the info context has errors of @a type message type
 */
bool info_ctx_has(struct info_ctx *ctx, enum info_msg_type type);
void info_ctx_flush(struct info_ctx *ctx, FILE *f, int type);
void info_ctx_print(struct info_ctx *ctx, unsigned int index);

i_INLINE_DECL void info_ctx_inject_input_file(struct info_ctx *ctx, struct inpfile *f)
{
    ctx->file = f;
}

/**
 * Gets all messages of a certain type, properly formatted and returns them
 * in the given RFstringx
 * @param ctx        The info context to work with
 * @param type       The type of messages to retrieve
 * @param str        The string in which to return
 * @return           True/false for success/failure
*/
bool info_ctx_get_messages_fmt(struct info_ctx *ctx,
                               enum info_msg_type type,
                               struct RFstringx *str);

/**
 * An iterator allowing iteration over the messages of an info_ctx
 */
struct info_ctx_msg_iterator {
    enum info_msg_type msg_types;
    struct RFilist_node *start;
    struct RFilist_node *curr;
};
void info_ctx_get_iter(struct info_ctx *ctx,
                       enum info_msg_type types,
                       struct info_ctx_msg_iterator *iter);
struct info_msg *info_ctx_msg_iterator_next(struct info_ctx_msg_iterator *it);

/* simple printing related function wrappers */

#define ERROR(...)                              \
    do {                                        \
        printf("refu: [error] "__VA_ARGS__);    \
        printf("\n");                           \
    } while(0)

#define WARN(...)                               \
    do {                                        \
        printf("refu: [warning] "__VA_ARGS__);  \
        printf("\n");                           \
    } while(0)

#endif
