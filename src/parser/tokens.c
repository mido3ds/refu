#include <parser/tokens.h>

#include <String/rf_str_core.h>



const struct RFstring parser_tok_colon = RF_STRING_STATIC_INIT(":");
const struct RFstring parser_tok_ocbrace = RF_STRING_STATIC_INIT("{");
const struct RFstring parser_tok_ccbrace = RF_STRING_STATIC_INIT("}");
const struct RFstring parser_tok_oparen = RF_STRING_STATIC_INIT("(");
const struct RFstring parser_tok_cparen = RF_STRING_STATIC_INIT(")");
const struct RFstring parser_tok_fn = RF_STRING_STATIC_INIT("fn");
const struct RFstring parser_tok_arrow = RF_STRING_STATIC_INIT("->");
const struct RFstring parser_tok_comma = RF_STRING_STATIC_INIT(",");

const struct RFstring parser_tok_dsum = RF_STRING_STATIC_INIT("|");
const struct RFstring parser_tok_dprod = RF_STRING_STATIC_INIT(",");
const struct RFstring parser_tok_dimpl = RF_STRING_STATIC_INIT("->");
const struct RFstring parser_tok_gt = RF_STRING_STATIC_INIT(">");
const struct RFstring parser_tok_lt = RF_STRING_STATIC_INIT("<");

const struct RFstring parser_kw_data = RF_STRING_STATIC_INIT("data");
const struct RFstring parser_kw_type = RF_STRING_STATIC_INIT("type");