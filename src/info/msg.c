#include <info/msg.h>

#include <Utils/memory.h>
#include <Utils/sanity.h>
#include <inpfile.h>

#define INFO_WARNING_STR "warning"
#define INFO_ERROR_STR "error"

#define LOCMARK_FMT "%*c\n"
#define LOCMARK_ARG(column_position_) column_position_ + 1, '^'

#define LOCMARK2_FMT "%*c%*c\n"
#define LOCMARK2_ARG(colpos1_, colpos2_) colpos1_ + 1, '^', \
        colpos2_ - colpos1_, '^'

inline static void print_location_marker(FILE *f, int col_pos)
{
    printf(LOCMARK_FMT, LOCMARK_ARG(col_pos));
}

i_INLINE_INS bool info_msg_has_end_mark(struct info_msg *msg);

struct info_msg *info_msg_create(enum info_msg_type type,
                                 const struct inplocation_mark *start,
                                 const struct inplocation_mark *end,
                                 const char *fmt,
                                 va_list args)
{
    struct info_msg *ret;
    RF_MALLOC(ret, sizeof(*ret), return NULL);

    if (!rf_string_initvl(&ret->s, fmt, args)) {
        va_end(args);
        return NULL;
    }

    ret->type = type;

    if (start) {
        ret->start_mark = *start;
    } else {
        LOCMARK_RESET(&ret->start_mark);
    }

    if (end) {
        ret->end_mark = *end;
    } else {
        LOCMARK_RESET(&ret->end_mark);
    }

    return ret;
}

void info_msg_destroy(struct info_msg *m)
{
    rf_string_deinit(&m->s);
    free(m);
}

void info_msg_print(struct info_msg *m, FILE *f, struct inpfile *input_file)
{
// TODO: combine with get_formatted
#if 0
    struct RFstring line_str;
    switch(m->type) {
    case MESSAGE_SEMANTIC_WARNING:
        fprintf(
            f,
            INPLOCATION_FMT" "INFO_WARNING_STR": "RF_STR_PF_FMT"\n",
            INPLOCATION_ARG(input_file, &m->loc),
            RF_STR_PF_ARG(&m->s));

        break;
    case MESSAGE_SYNTAX_WARNING:
        fprintf(
            f,
            INPLOCATION_FMT" "INFO_WARNING_STR": "RF_STR_PF_FMT"\n",
            INPLOCATION_ARG(input_file, &m->loc),
            RF_STR_PF_ARG(&m->s));

        break;
    case MESSAGE_SEMANTIC_ERROR:
        fprintf(
            f,
            INPLOCATION_FMT" "INFO_ERROR_STR": "RF_STR_PF_FMT"\n",
            INPLOCATION_ARG(input_file, &m->loc),
            RF_STR_PF_ARG(&m->s));
        break;
    case MESSAGE_SYNTAX_ERROR:
        fprintf(
            f,
            INPLOCATION_FMT" "INFO_ERROR_STR": "RF_STR_PF_FMT"\n",
            INPLOCATION_ARG(input_file, &m->loc),
            RF_STR_PF_ARG(&m->s));
        if (!inpfile_line(input_file, m->loc.start.line, &line_str)) {
            ERROR("Could not locate line %u at file "RF_STR_PF_FMT,
                  m->loc.start.line,
                  RF_STR_PF_ARG(inpfile_name(input_file)));
        } else {
            fprintf(f, RF_STR_PF_FMT, RF_STR_PF_ARG(&line_str));
            print_location_marker(f, m->loc.start.col);
        }
        break;
    default: /* should never get here */
        assert(0);
        break;
    }
#endif
}

bool info_msg_get_formatted(struct info_msg *m, struct RFstringx *s,
                            struct inpfile *input_file)
{
    struct RFstring line_str;
    switch(m->type) {
    case MESSAGE_SEMANTIC_WARNING:
        rf_stringx_assignv(
            s,
            INPLOCMARKS_FMT" "INFO_WARNING_STR": "RF_STR_PF_FMT"\n",
            INPLOCMARKS_ARG(input_file, &m->start_mark, &m->end_mark),
            RF_STR_PF_ARG(&m->s));

        break;
    case MESSAGE_SYNTAX_WARNING:
        rf_stringx_assignv(
            s,
            INPLOCMARKS_FMT" "INFO_WARNING_STR": "RF_STR_PF_FMT"\n",
            INPLOCMARKS_ARG(input_file, &m->start_mark, &m->end_mark),
            RF_STR_PF_ARG(&m->s));

        break;
    case MESSAGE_SEMANTIC_ERROR:
        rf_stringx_assignv(
            s,
            INPLOCMARKS_FMT" "INFO_ERROR_STR": "RF_STR_PF_FMT"\n",
            INPLOCMARKS_ARG(input_file, &m->start_mark, &m->end_mark),
            RF_STR_PF_ARG(&m->s));
        break;
    case MESSAGE_SYNTAX_ERROR:
        rf_stringx_assignv(
            s,
            INPLOCMARKS_FMT" "INFO_ERROR_STR": "RF_STR_PF_FMT"\n",
            INPLOCMARKS_ARG(input_file, &m->start_mark, &m->end_mark),
            RF_STR_PF_ARG(&m->s));
        if (!inpfile_line(input_file, m->start_mark.line, &line_str)) {
            ERROR("Could not locate line %u at file "RF_STR_PF_FMT,
                  m->start_mark.line,
                  RF_STR_PF_ARG(inpfile_name(input_file)));
            return false;
        } else {
            rf_stringx_move_end(s);
            rf_stringx_assignv(s, RF_STR_PF_FMT"\n", RF_STR_PF_ARG(&line_str));
            rf_stringx_move_end(s);

            // set the markers
            if (info_msg_has_end_mark(m)) {
                rf_stringx_assignv(s,
                                   LOCMARK2_FMT,
                                   LOCMARK2_ARG(m->start_mark.col,
                                                m->end_mark.col));
            } else {
                rf_stringx_assignv(s,
                                   LOCMARK_FMT,
                                   LOCMARK_ARG(m->start_mark.col));
            }
        }
        break;
    default: /* should never get here */
        RF_ASSERT_OR_CRITICAL(false,
                              "Illegal compiler message type encountered");
        break;
    }

    return true;
}
