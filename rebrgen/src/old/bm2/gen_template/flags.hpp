/*license*/
#pragma once
#include <cmdline/template/help_option.h>
#include <json/convert_json.h>
#include <json/json_export.h>
#include <format>
#include <map>
#include "hook_list.hpp"

struct VariableDesc {
    std::string var_name;
    std::string type;
    std::string description;
    std::string initial_value;
};

struct EnvMappingDesc {
    std::string variable_name;
    std::map<std::string, std::string> mapping;
    std::string_view file_name;
    std::string_view func_name;
    size_t line;
};

struct FlagUsageMappingDesc {
    std::string flag_name;
    std::string flag_value;
    std::string_view file_name;
    std::string_view func_name;
    size_t line;
};

struct Content {
    std::vector<VariableDesc> variables;
    std::vector<EnvMappingDesc> env_mappings;
    std::vector<FlagUsageMappingDesc> flag_usage_mappings;
};

struct HookSection {
    std::vector<std::string> lines;
};

struct Flags : futils::cmdline::templ::HelpOption {
    bm2::GenerateMode mode = bm2::GenerateMode::generator;
    std::map<bm2::FuncName, std::map<std::string, Content>> content;
    bm2::FuncName func_name = bm2::FuncName::eval;
    bm2::HookFile func_hook = bm2::HookFile::eval_op;
    std::map<std::u8string, HookSection> hook_sections;

    bool requires_lang_option() const {
        return !print_hooks && !(mode == bm2::GenerateMode::docs_json ||
                                 mode == bm2::GenerateMode::docs_markdown);
    }

    void set_func_name(bm2::FuncName func_name, bm2::HookFile hook) {
        this->func_name = func_name;
        this->func_hook = hook;
    }

    std::string_view config_file = "config.json";
    std::string_view hook_file_dir = "hook";
    bool debug = false;
    bool print_hooks = false;

    std::string worker_name() {
        if (worker_request_name.empty()) {
            return lang_name;
        }
        return worker_request_name;
    }

    // json options
    std::string lang_name;
    std::string file_suffix = "";
    std::string worker_request_name = "";
    std::string worker_lsp_name = "";
    std::string comment_prefix = "/*";
    std::string comment_suffix = "*/";
    std::string int_type = "std::int${ALIGNED_BIT_SIZE}_t";
    std::string uint_type = "std::uint${ALIGNED_BIT_SIZE}_t";
    std::string float_type = "std::float${ALIGNED_BIT_SIZE}_t";
    std::string array_type = "std::array<$TYPE, $LEN>";
    std::string vector_type = "std::vector<$TYPE>";
    std::string optional_type = "std::optional<$TYPE>";
    std::string pointer_type = "$TYPE*";
    std::string recursive_struct_type = "$TYPE*";
    std::string byte_vector_type = "std::vector<std::uint8_t>";
    std::string byte_array_type = "std::array<std::uint8_t, $LEN>";
    std::string bool_type = "bool";
    std::string true_literal = "true";
    std::string false_literal = "false";
    std::string coder_return_type = "bool";
    std::string property_setter_return_type = "bool";
    std::string end_of_statement = ";";
    std::string block_begin = "{";
    std::string block_end = "}";
    bool otbs_on_block_end = false;  // like golang else style
    std::string block_end_type = "};";
    std::string struct_keyword = "struct";
    std::string enum_keyword = "enum";
    std::string union_keyword = "union";
    std::string define_var_keyword = "";
    std::string var_type_separator = " ";
    std::string define_var_assign = "=";
    bool omit_type_on_define_var = false;
    std::string field_type_separator = " ";
    std::string field_end = ";";
    std::string enum_member_end = ",";
    std::string func_keyword = "";
    bool trailing_return_type = false;
    std::string func_brace_ident_separator = "";
    std::string func_type_separator = " ";
    std::string func_void_return_type = "void";
    bool prior_ident = false;
    std::string define_variable = "";
    std::string define_field = "";
    std::string define_bit_field = "";
    std::string define_parameter = "";

    std::string if_keyword = "if";
    std::string elif_keyword = "else if";
    std::string else_keyword = "else";
    std::string infinity_loop = "for(;;)";
    std::string conditional_loop = "while";
    std::string define_const_keyword = "const";
    std::string match_keyword = "switch";
    std::string match_case_keyword = "case";
    std::string match_case_separator = ":";
    std::string match_default_keyword = "default";
    std::string self_ident = "(*this)";
    std::string param_type_separator = " ";
    bool condition_has_parentheses = true;
    std::string self_param = "";
    std::string encoder_param = "Encoder& w";
    std::string decoder_param = "Decoder& r";
    bool func_style_cast = false;
    std::string empty_pointer = "nullptr";
    std::string empty_optional = "std::nullopt";
    std::string size_method = "$VECTOR.size()";
    std::string append_method = "$VECTOR.push_back($ITEM)";
    std::string assert_method = "assert($CONDITION)";
    std::string length_check_method = "assert($VECTOR.size() == $SIZE)";
    std::string property_setter_ok = "return true";
    std::string property_setter_fail = "return false";
    std::string coder_success = "return true";

    std::string variant_mode = "union";  // union or algebraic
    std::string algebraic_variant_separator = "|";
    std::string algebraic_variant_type = "$TYPES";
    std::string check_union_condition = "!std::holds_alternative<$MEMBER_INDEX>($FIELD_IDENT)";
    std::string check_union_fail_return_value = "false";
    std::string switch_union = "$FIELD_IDENT = $MEMBER_IDENT()";
    std::string address_of_placeholder = "&$VALUE";
    std::string optional_of_placeholder = "$VALUE";
    std::string decode_bytes_op = "$VALUE = $DECODER.decode_bytes($LEN)";
    std::string encode_bytes_op = "$ENCODER.encode_bytes($VALUE)";
    std::string decode_bytes_until_eof_op = "$VALUE = $DECODER.decode_bytes_until_eof()";
    std::string peek_bytes_op = "$VALUE = $DECODER.peek_bytes($LEN)";
    std::string encode_offset = "$ENCODER.offset()";
    std::string decode_offset = "$DECODER.offset()";
    std::string encode_backward = "$ENCODER.backward($OFFSET)";
    std::string decode_backward = "$DECODER.backward($OFFSET)";
    std::string is_little_endian = "std::endian::native == std::endian::little";
    std::string default_enum_base = "";  // if empty, omit the base type
    std::string enum_base_separator = " : ";
    std::string reserve_size_dynamic = "$VECTOR.reserve($SIZE)";
    std::string reserve_size_static = "";

    std::string eval_result_text = "$RESULT = make_eval_result($TEXT);";
    std::string eval_result_passthrough = "$RESULT = $TEXT;";
    std::string access_style = "$BASE.$IDENT";
    std::string enum_access_style = "$BASE::$IDENT";
    // NEW_OBJECT means initialized by default
    std::string new_object_style = "$TYPE()";
    std::string new_object_int_style = "0";
    std::string new_object_float_style = "0.0";
    std::string new_object_array_style = "$TYPE()";
    std::string new_object_vector_style = "$TYPE()";
    std::string new_object_struct_style = "$TYPE()";

    std::string keyword_escape_style = "${VALUE}_";

    bool compact_bit_field = false;
    std::string format_nested_function = "";  // "", declare, define, none
    bool format_nested_struct = false;

#define USE_FLAG_BASE(op, name) use_flag(flags, op, #name, __FILE__, __func__, __LINE__, flags.name)
#define USE_FLAG(name) USE_FLAG_BASE(op, name)
#define ENV_FLAG(name) #name, __FILE__, __func__, __LINE__, flags.name

#define MAP_TO_MACRO(MACRO_NAME)                                               \
    MACRO_NAME(lang_name, "lang")                                              \
    MACRO_NAME(file_suffix, "suffix")                                          \
    MACRO_NAME(worker_request_name, "worker_request_name")                     \
    MACRO_NAME(worker_lsp_name, "worker_lsp_name")                             \
    MACRO_NAME(comment_prefix, "comment_prefix")                               \
    MACRO_NAME(comment_suffix, "comment_suffix")                               \
    MACRO_NAME(int_type, "int_type")                                           \
    MACRO_NAME(uint_type, "uint_type")                                         \
    MACRO_NAME(float_type, "float_type")                                       \
    MACRO_NAME(array_type, "array_type")                                       \
    MACRO_NAME(vector_type, "vector_type")                                     \
    MACRO_NAME(byte_vector_type, "byte_vector_type")                           \
    MACRO_NAME(byte_array_type, "byte_array_type")                             \
    MACRO_NAME(optional_type, "optional_type")                                 \
    MACRO_NAME(pointer_type, "pointer_type")                                   \
    MACRO_NAME(recursive_struct_type, "recursive_struct_type")                 \
    MACRO_NAME(bool_type, "bool_type")                                         \
    MACRO_NAME(true_literal, "true_literal")                                   \
    MACRO_NAME(false_literal, "false_literal")                                 \
    MACRO_NAME(coder_return_type, "coder_return_type")                         \
    MACRO_NAME(property_setter_return_type, "property_setter_return_type")     \
    MACRO_NAME(end_of_statement, "end_of_statement")                           \
    MACRO_NAME(block_begin, "block_begin")                                     \
    MACRO_NAME(block_end, "block_end")                                         \
    MACRO_NAME(otbs_on_block_end, "otbs_on_block_end")                         \
    MACRO_NAME(block_end_type, "block_end_type")                               \
    MACRO_NAME(prior_ident, "prior_ident")                                     \
    MACRO_NAME(define_variable, "define_variable")                             \
    MACRO_NAME(define_field, "define_field")                                   \
    MACRO_NAME(struct_keyword, "struct_keyword")                               \
    MACRO_NAME(enum_keyword, "enum_keyword")                                   \
    MACRO_NAME(define_var_keyword, "define_var_keyword")                       \
    MACRO_NAME(define_const_keyword, "define_const_keyword")                   \
    MACRO_NAME(var_type_separator, "var_type_separator")                       \
    MACRO_NAME(define_var_assign, "define_var_assign")                         \
    MACRO_NAME(omit_type_on_define_var, "omit_type_on_define_var")             \
    MACRO_NAME(field_type_separator, "field_type_separator")                   \
    MACRO_NAME(field_end, "field_end")                                         \
    MACRO_NAME(enum_member_end, "enum_member_end")                             \
    MACRO_NAME(func_keyword, "func_keyword")                                   \
    MACRO_NAME(trailing_return_type, "trailing_return_type")                   \
    MACRO_NAME(func_brace_ident_separator, "func_brace_ident_separator")       \
    MACRO_NAME(func_type_separator, "func_type_separator")                     \
    MACRO_NAME(func_void_return_type, "func_void_return_type")                 \
    MACRO_NAME(if_keyword, "if_keyword")                                       \
    MACRO_NAME(elif_keyword, "elif_keyword")                                   \
    MACRO_NAME(else_keyword, "else_keyword")                                   \
    MACRO_NAME(infinity_loop, "infinity_loop")                                 \
    MACRO_NAME(conditional_loop, "conditional_loop")                           \
    MACRO_NAME(match_keyword, "match_keyword")                                 \
    MACRO_NAME(match_case_keyword, "match_case_keyword")                       \
    MACRO_NAME(match_case_separator, "match_case_separator")                   \
    MACRO_NAME(match_default_keyword, "match_default_keyword")                 \
    MACRO_NAME(condition_has_parentheses, "condition_has_parentheses")         \
    MACRO_NAME(self_ident, "self_ident")                                       \
    MACRO_NAME(param_type_separator, "param_type_separator")                   \
    MACRO_NAME(self_param, "self_param")                                       \
    MACRO_NAME(encoder_param, "encoder_param")                                 \
    MACRO_NAME(decoder_param, "decoder_param")                                 \
    MACRO_NAME(func_style_cast, "func_style_cast")                             \
    MACRO_NAME(empty_pointer, "empty_pointer")                                 \
    MACRO_NAME(empty_optional, "empty_optional")                               \
    MACRO_NAME(size_method, "size_method")                                     \
    MACRO_NAME(append_method, "append_method")                                 \
    MACRO_NAME(length_check_method, "length_check_method")                     \
    MACRO_NAME(property_setter_ok, "property_setter_ok")                       \
    MACRO_NAME(property_setter_fail, "property_setter_fail")                   \
    MACRO_NAME(coder_success, "coder_success")                                 \
    MACRO_NAME(variant_mode, "variant_mode")                                   \
    MACRO_NAME(algebraic_variant_separator, "algebraic_variant_separator")     \
    MACRO_NAME(algebraic_variant_type, "algebraic_variant_type")               \
    MACRO_NAME(check_union_condition, "check_union_condition")                 \
    MACRO_NAME(check_union_fail_return_value, "check_union_fail_return_value") \
    MACRO_NAME(switch_union, "switch_union")                                   \
    MACRO_NAME(address_of_placeholder, "address_of_placeholder")               \
    MACRO_NAME(optional_of_placeholder, "optional_of_placeholder")             \
    MACRO_NAME(decode_bytes_op, "decode_bytes_op")                             \
    MACRO_NAME(encode_bytes_op, "encode_bytes_op")                             \
    MACRO_NAME(decode_bytes_until_eof_op, "decode_bytes_until_eof_op")         \
    MACRO_NAME(peek_bytes_op, "peek_bytes_op")                                 \
    MACRO_NAME(encode_offset, "encode_offset")                                 \
    MACRO_NAME(decode_offset, "decode_offset")                                 \
    MACRO_NAME(encode_backward, "encode_backward")                             \
    MACRO_NAME(decode_backward, "decode_backward")                             \
    MACRO_NAME(is_little_endian, "is_little_endian_expr")                      \
    MACRO_NAME(default_enum_base, "default_enum_base")                         \
    MACRO_NAME(enum_base_separator, "enum_base_separator")                     \
    MACRO_NAME(eval_result_text, "eval_result_text")                           \
    MACRO_NAME(eval_result_passthrough, "eval_result_passthrough")             \
    MACRO_NAME(compact_bit_field, "compact_bit_field")                         \
    MACRO_NAME(format_nested_function, "format_nested_function")               \
    MACRO_NAME(reserve_size_dynamic, "reserve_size_dynamic")                   \
    MACRO_NAME(reserve_size_static, "reserve_size_static")                     \
    MACRO_NAME(access_style, "access_style")                                   \
    MACRO_NAME(enum_access_style, "enum_access_style")                         \
    MACRO_NAME(format_nested_struct, "format_nested_struct")                   \
    MACRO_NAME(keyword_escape_style, "keyword_escape_style")

    bool from_json(const futils::json::JSON& js) {
        JSON_PARAM_BEGIN(*this, js)
        MAP_TO_MACRO(FROM_JSON_OPT)
        JSON_PARAM_END()
    }

    bool to_json(auto&& js) const {
        JSON_PARAM_BEGIN(*this, js)
        MAP_TO_MACRO(TO_JSON_PARAM)
        JSON_PARAM_END()
    }

    void bind(futils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarString(&lang_name, "lang", "language name", "LANG");
        ctx.VarBool(&debug, "debug", "debug mode (print hook call on stderr)");
        ctx.VarBool(&print_hooks, "print-hooks", "print hooks");
        ctx.VarString<true>(&config_file, "config-file", "config file", "FILE");
        ctx.VarString<true>(&hook_file_dir, "hook-dir", "hook file directory", "DIR");
        ctx.VarMap<std::string, bm2::GenerateMode, std::map>(
            &mode, "mode", "generate mode (generator,config,header,main,cmake,js-worker,js-ui,js-ui-embed,docs-json,docs-markdown,cmptest-json,cmptest-build)", "MODE",
            std::map<std::string, bm2::GenerateMode>{
                {"generator", bm2::GenerateMode::generator},
                {"header", bm2::GenerateMode::header},
                {"main", bm2::GenerateMode::main},
                {"cmake", bm2::GenerateMode::cmake},
                {"js-worker", bm2::GenerateMode::js_worker},
                {"js-ui", bm2::GenerateMode::js_ui},
                {"js-ui-embed", bm2::GenerateMode::js_ui_embed},
                {"docs-json", bm2::GenerateMode::docs_json},
                {"docs-markdown", bm2::GenerateMode::docs_markdown},
                {"config", bm2::GenerateMode::config_},
                {"cmptest-json", bm2::GenerateMode::cmptest_json},
                {"cmptest-build", bm2::GenerateMode::cmptest_build},
            });
    }

    bool is_valid_placeholder(std::string_view placeholder) {
        auto first_found = placeholder.find("{}");
        if (first_found == std::string_view::npos) {
            return false;
        }
        auto second_found = placeholder.find("{}", first_found + 1);
        return second_found == std::string_view::npos;
    }

    /*
    std::string wrap_int(size_t bit_size) {
        if (is_valid_placeholder(int_type)) {
            return std::vformat(int_type, std::make_format_args(bit_size));
        }
        else if (!int_type.contains("{") && !int_type.contains("}")) {
            return int_type;
        }
        return std::format("* invalid placeholder: {} *", int_type);
    }

    std::string wrap_uint(size_t bit_size) {
        if (is_valid_placeholder(uint_type)) {
            return std::vformat(uint_type, std::make_format_args(bit_size));
        }
        else if (!uint_type.contains("{") && !uint_type.contains("}")) {
            return uint_type;
        }
        return std::format("* invalid placeholder: {} *", uint_type);
    }

    std::string wrap_float(size_t bit_size) {
        if (is_valid_placeholder(float_type_placeholder)) {
            return std::vformat(float_type_placeholder, std::make_format_args(bit_size));
        }
        else if (!float_type_placeholder.contains("{") && !float_type_placeholder.contains("}")) {
            return float_type_placeholder;
        }
        return std::format("* invalid placeholder: {} *", float_type_placeholder);
    }
    */

    std::string wrap_comment(const std::string& comment) {
        std::string result;
        result.reserve(comment.size() + comment_prefix.size() + comment_suffix.size());
        result.append(comment_prefix);
        result.append(comment);
        result.append(comment_suffix);
        return result;
    }
};
