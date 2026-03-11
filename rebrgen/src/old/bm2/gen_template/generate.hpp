/*license*/
#pragma once
#include <string>
#include <string_view>
#include <map>
#include "flags.hpp"
#include <format>

namespace rebgn {
    std::string env_escape(Flags& flags, std::string_view action_at, std::string_view param_name, std::string_view file_name, std::string_view func_name, size_t line, std::string_view str, std::map<std::string, std::string>& map);

    std::string env_escape(Flags& flags, auto op, std::string_view param_name, std::string_view file_name, std::string_view func_name, size_t line, std::string_view str, std::map<std::string, std::string>& map) {
        return env_escape(flags, std::string_view(to_string(op)), param_name, file_name, func_name, line, str, map);
    }

    std::string env_escape_and_concat(Flags& flags, auto op, std::string_view param_name, std::string_view file_name, std::string_view func_name, size_t line, std::string_view str, std::map<std::string, std::string>& map) {
        auto escaped = env_escape(flags, op, param_name, file_name, func_name, line, str, map);
        return "futils::strutil::concat<std::string>(\"" + escaped + "\")";
    }

    decltype(auto) use_flag(Flags& flags, auto op, std::string_view flag_name, std::string_view file_name, std::string_view func_name, size_t line, auto& flag_ref) {
        if (flags.mode == bm2::GenerateMode::docs_json || flags.mode == bm2::GenerateMode::docs_markdown) {
            flags.content[flags.func_name][std::string(to_string(op))].flag_usage_mappings.push_back({std::string(flag_name), std::format("{}", flag_ref), file_name, func_name, line});
        }
        return flag_ref;
    }

    std::string unimplemented_comment(Flags& flags, const std::string& op);
    enum class EvalResultMode {
        TEXT,
        PASSTHROUGH,
        COMMENT,  // treat as TEXT but special handling for comment
    };
    void do_make_eval_result(bm2::TmpCodeWriter& w, AbstractOp op, Flags& flags, const std::string& text, EvalResultMode mode);
    void write_eval_result(bm2::TmpCodeWriter& eval_result, Flags& flags);
    void write_eval_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& eval, Flags& flags);
    void write_inner_function_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_function, Flags& flags);
    void write_field_accessor_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& field_accessor,
                                   bm2::TmpCodeWriter& type_accessor, Flags& flags);
    void write_inner_block_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_block, Flags& flags);
    void write_parameter_func(bm2::TmpCodeWriter& w,
                              bm2::TmpCodeWriter& add_parameter,
                              bm2::TmpCodeWriter& add_call_parameter, Flags& flags);
    void write_type_to_string_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& type_to_string, Flags& flags);
    void write_func_decl(bm2::TmpCodeWriter& inner_function, AbstractOp op, Flags& flags);
    void write_return_type(bm2::TmpCodeWriter& inner_function, AbstractOp op, Flags& flags);
}  // namespace rebgn
