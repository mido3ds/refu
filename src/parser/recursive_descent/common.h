#ifndef LFR_PARSER_RECURSIVE_DESCENT_COMMON_H
#define LFR_PARSER_RECURSIVE_DESCENT_COMMON_H

// TODO: Change both this and the lexer macro to something better
#define parser_synerr(parser_, start_, end_, ...) \
    do {                                          \
        i_info_ctx_add_msg((parser_)->info,       \
                           MESSAGE_SYNTAX_ERROR,  \
                           (start_),              \
                           (end_),                \
                           __VA_ARGS__);          \
    } while(0)

#define parser_has_synerr(parser_)                      \
    info_ctx_has((parser_)->info, MESSAGE_SYNTAX_ERROR)

#endif