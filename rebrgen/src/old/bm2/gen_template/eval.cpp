/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include <env/env.h>
#include "define.hpp"
#include "generate.hpp"

namespace rebgn {

    std::string env_escape(Flags& flags, std::string_view action_at, std::string_view param_name, std::string_view file_name, std::string_view func_name, size_t line, std::string_view str, std::map<std::string, std::string>& map) {
        map.erase("DOLLAR");
        if (flags.mode == bm2::GenerateMode::docs_json || flags.mode == bm2::GenerateMode::docs_markdown) {
            flags.content[flags.func_name][std::string(action_at)].env_mappings.push_back({std::string(param_name), map, file_name, func_name, line});
        }
        map["DOLLAR"] = "$";
        return futils::env::expand<std::string>(str, futils::env::expand_map<std::string>(map), true);
    }

    void do_make_eval_result(bm2::TmpCodeWriter& w, AbstractOp op, Flags& flags, const std::string& text, EvalResultMode mode) {
        std::map<std::string, std::string> map{
            {"RESULT", "result"},
            {"TEXT", text},
        };
        if (mode == EvalResultMode::PASSTHROUGH) {
            auto escaped = env_escape(flags, op, ENV_FLAG(eval_result_passthrough), map);
            w.writeln(escaped);
        }
        else {
            auto escaped = env_escape(flags, op, ENV_FLAG(eval_result_text), map);
            w.writeln(escaped);
        }
    }

    void write_eval(bm2::TmpCodeWriter& eval, AbstractOp op, Flags& flags) {
        flags.set_func_name(bm2::FuncName::eval,bm2::HookFile::eval_op);
        eval.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
        auto scope = eval.indent_scope();
        auto eval_hook = [&](auto&& default_action, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(eval, flags, bm2::HookFile::eval_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(eval, flags, bm2::HookFile::eval_op, op, stage)) {
                default_action();
            }
        };
        eval_hook([&] {}, bm2::HookFileSub::before);
        if (op == AbstractOp::BINARY) {
            do_variable_definition(eval, flags, op, "op", code_ref(flags, "bop"), "rebgn::BinaryOp", "binary operator");
            define_eval(eval, flags, op, "left_eval", code_ref(flags, "left_ref"), "left operand");
            define_eval(eval, flags, op, "right_eval", code_ref(flags, "right_ref"), "right operand");
            do_variable_definition(eval, flags, op, "opstr", "to_string(op)", "string", "binary operator string");
            eval_hook([&] {
                eval_hook([&] {}, bm2::HookFileSub::op);
                for (size_t b = 0; to_string(BinaryOp(b))[0] != '\0'; b++) {
                    bm2::TmpCodeWriter inner_eval;
                    if (may_write_from_hook(inner_eval, flags, bm2::HookFile::eval_op, op, bm2::HookFileSub::op, BinaryOp(b))) {
                        eval.writeln("if(op == rebgn::BinaryOp::" + std::string(to_string(BinaryOp(b))) + ") {");
                        auto scope = eval.indent_scope();
                        eval.writeln(inner_eval.out());
                        scope.execute();
                        eval.writeln("}");
                    }
                }
                do_make_eval_result(eval, op, flags, "std::format(\"({} {} {})\", left_eval.result, opstr, right_eval.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::UNARY) {
            do_variable_definition(eval, flags, op, "op", code_ref(flags, "uop"), "rebgn::UnaryOp", "unary operator");
            define_eval(eval, flags, op, "target", code_ref(flags, "ref"), "target");
            do_variable_definition(eval, flags, op, "opstr", "to_string(op)", "string", "unary operator string");
            eval_hook([&] {
                eval_hook([&] {}, bm2::HookFileSub::op);
                for (size_t b = 0; to_string(UnaryOp(b))[0] != '\0'; b++) {
                    bm2::TmpCodeWriter inner_eval;
                    if (may_write_from_hook(inner_eval, flags, bm2::HookFile::eval_op, op, bm2::HookFileSub::op, UnaryOp(b))) {
                        eval.writeln("if(op == rebgn::UnaryOp::" + std::string(to_string(UnaryOp(b))) + ") {");
                        auto scope = eval.indent_scope();
                        eval.writeln(inner_eval.out());
                        scope.execute();
                        eval.writeln("}");
                    }
                }
                do_make_eval_result(eval, op, flags, "std::format(\"({}{})\", opstr, target.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IS_LITTLE_ENDIAN) {
            define_ref(eval, flags, op, "fallback", code_ref(flags, "fallback"), "fallback expression");
            eval_hook([&] {
                eval.writeln("if(fallback.value() != 0) {");
                auto scope = eval.indent_scope();
                eval_hook([&] {
                    do_make_eval_result(eval, op, flags, "eval(ctx.ref(fallback), ctx)", EvalResultMode::PASSTHROUGH);
                },
                          bm2::HookFileSub::fallback);
                scope.execute();
                eval.writeln("}");
                eval.writeln("else {");
                auto scope2 = eval.indent_scope();
                eval_hook([&] {
                    do_make_eval_result(eval, op, flags, "\"" + USE_FLAG(is_little_endian) + "\"", EvalResultMode::TEXT);
                },
                          bm2::HookFileSub::no_fallback);
                scope2.execute();
                eval.writeln("}");
            });
        }
        else if (op == AbstractOp::ADDRESS_OF) {
            define_eval(eval, flags, op, "target", code_ref(flags, "ref"), "target object");
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"VALUE", "\",target.result,\""},
                };
                auto escaped = env_escape_and_concat(flags, op, ENV_FLAG(address_of_placeholder), map);
                do_make_eval_result(eval, op, flags, escaped, EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::OPTIONAL_OF) {
            define_eval(eval, flags, op, "target", code_ref(flags, "ref"), "target object");
            define_type(eval, flags, op, "type", code_ref(flags, "type"), "type of optional (not include optional)");
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"VALUE", "\",target.result,\""},
                    {"TYPE", "\",type,\""},
                };
                auto escaped = env_escape_and_concat(flags, op, ENV_FLAG(optional_of_placeholder), map);
                do_make_eval_result(eval, op, flags, escaped, EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::INPUT_BYTE_OFFSET) {
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"DECODER", "\",ctx.r(),\""},
                };
                auto escaped = env_escape(flags, op, ENV_FLAG(decode_offset), map);
                do_make_eval_result(eval, op, flags, "futils::strutil::concat<std::string>(\"" + escaped + "\")", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::OUTPUT_BYTE_OFFSET) {
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"ENCODER", "\",ctx.w(),\""},
                };
                auto escaped = env_escape(flags, op, ENV_FLAG(encode_offset), map);
                do_make_eval_result(eval, op, flags, "futils::strutil::concat<std::string>(\"" + escaped + "\")", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_INT) {
            define_uint(eval, flags, op, "value", "code.int_value()->value()", "immediate value");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}\", value)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_STRING) {
            do_variable_definition(eval, flags, op, "str", "ctx.string_table[code.ident().value().value()]", "string", "immediate string");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"\\\"{}\\\"\", futils::escape::escape_str<std::string>(str,futils::escape::EscapeFlag::hex,futils::escape::no_escape_set(),futils::escape::escape_all()))", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_TRUE) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + USE_FLAG(true_literal) + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_FALSE) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + USE_FLAG(false_literal) + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_INT64) {
            define_uint(eval, flags, op, "value", "*code.int_value64()", "immediate value");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}\", value)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_TYPE) {
            define_type(eval, flags, op, "type", code_ref(flags, "type"), "immediate type");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "type", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::IMMEDIATE_CHAR) {
            define_uint(eval, flags, op, "char_code", "code.int_value()->value()", "immediate char code");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}\", char_code)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::EMPTY_PTR) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + USE_FLAG(empty_pointer) + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::EMPTY_OPTIONAL) {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + USE_FLAG(empty_optional) + "\"", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::PHI || op == AbstractOp::DECLARE_VARIABLE ||
                 op == AbstractOp::DEFINE_VARIABLE_REF ||
                 op == AbstractOp::BEGIN_COND_BLOCK) {
            define_ref(eval, flags, op, "ref", code_ref(flags, "ref"), "expression");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(ref), ctx)", EvalResultMode::PASSTHROUGH);
            });
        }
        else if (op == AbstractOp::ASSIGN) {
            define_ref(eval, flags, op, "left_ref", code_ref(flags, "left_ref"), "assign target");
            define_ref(eval, flags, op, "right_ref", code_ref(flags, "right_ref"), "assign source");
            define_ref(eval, flags, op, "ref", code_ref(flags, "ref"), "previous assignment or phi or definition");
            eval_hook([&] {
                eval.writeln("if(ref.value() != 0) {");
                auto scope = eval.indent_scope();
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(ref), ctx)", EvalResultMode::PASSTHROUGH);
                scope.execute();
                eval.writeln("}");
                eval.writeln("else {");
                auto scope2 = eval.indent_scope();
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(left_ref), ctx)", EvalResultMode::PASSTHROUGH);
                scope2.execute();
                eval.writeln("}");
            });
        }
        else if (op == AbstractOp::ACCESS) {
            define_eval(eval, flags, op, "left_eval", code_ref(flags, "left_ref"), "left operand");
            define_ident(eval, flags, op, "right_ident", code_ref(flags, "right_ref"), "right operand");
            define_bool(eval, flags, op, "is_enum_member", "ctx.ref(left_eval_ref).op == rebgn::AbstractOp::IMMEDIATE_TYPE", "is enum member");
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"BASE", "\",left_eval.result,\""},
                    {"IDENT", "\",right_ident,\""},
                };
                auto access = env_escape_and_concat(flags, op, ENV_FLAG(access_style), map);
                auto enum_access = env_escape_and_concat(flags, op, ENV_FLAG(enum_access_style), map);
                eval.writeln("if(is_enum_member) {");
                auto scope = eval.indent_scope();
                eval_hook([&] {
                    do_make_eval_result(eval, op, flags, enum_access, EvalResultMode::TEXT);
                },
                          bm2::HookFileSub::enum_member);
                scope.execute();
                eval.writeln("}");
                eval.writeln("else {");
                auto scope2 = eval.indent_scope();
                eval_hook([&] {
                    do_make_eval_result(eval, op, flags, access, EvalResultMode::TEXT);
                },
                          bm2::HookFileSub::normal);
                scope2.execute();
                eval.writeln("}");
            });
        }
        else if (op == AbstractOp::INDEX) {
            define_eval(eval, flags, op, "left_eval", code_ref(flags, "left_ref"), "indexed object");
            define_eval(eval, flags, op, "right_eval", code_ref(flags, "right_ref"), "index");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}[{}]\", left_eval.result, right_eval.result)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::ARRAY_SIZE) {
            define_eval(eval, flags, op, "vector_eval", code_ref(flags, "ref"), "array");
            eval_hook([&] {
                std::map<std::string, std::string> map{
                    {"VECTOR", "\",vector_eval.result,\""},
                };
                auto size_method = env_escape_and_concat(flags, op, ENV_FLAG(size_method), map);
                do_make_eval_result(eval, op, flags, size_method, EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::DEFINE_FIELD || op == AbstractOp::DEFINE_PROPERTY || op == AbstractOp::DEFINE_BIT_FIELD) {
            do_make_eval_result(eval, op, flags, "field_accessor(code,ctx)", EvalResultMode::PASSTHROUGH);
        }
        else if (op == AbstractOp::DEFINE_VARIABLE ||
                 op == AbstractOp::DEFINE_PARAMETER ||
                 op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
            define_ident(eval, flags, op, "ident", code_ref(flags, "ident"), "variable value");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "ident", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::NEW_OBJECT) {
            define_type(eval, flags, op, "type", code_ref(flags, "type"), "object type");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "std::format(\"{}()\", type)", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::DEFINE_CONSTANT) {
            define_ident(eval, flags, op, "ident", code_ref(flags, "ident"), "constant name");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "ident", EvalResultMode::TEXT);
            });
        }
        else if (op == AbstractOp::CAST) {
            define_type(eval, flags, op, "type", code_ref(flags, "type"), "cast target type");
            define_ref(eval, flags, op, "from_type_ref", code_ref(flags, "from_type"), "cast source type");
            define_eval(eval, flags, op, "evaluated", code_ref(flags, "ref"), "cast source value");
            // eval.writeln("result.insert(result.end(), evaluated.begin(), evaluated.end() - 1);");
            eval_hook([&] {
                if (USE_FLAG(func_style_cast)) {
                    do_make_eval_result(eval, op, flags, "std::format(\"{}({})\", type, evaluated.result)", EvalResultMode::TEXT);
                }
                else {
                    do_make_eval_result(eval, op, flags, "std::format(\"({}){}\", type, evaluated.result)", EvalResultMode::TEXT);
                }
            });
        }
        else if (op == AbstractOp::FIELD_AVAILABLE) {
            define_ref(eval, flags, op, "left_ref", code_ref(flags, "left_ref"), "field (maybe null)");
            eval.writeln("if(left_ref.value() == 0) {");
            auto scope_1 = eval.indent_scope();
            define_ref(eval, flags, op, "right_ref", code_ref(flags, "right_ref"), "condition");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(right_ref), ctx)", EvalResultMode::PASSTHROUGH);
            },
                      bm2::HookFileSub::self);
            // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
            scope_1.execute();
            eval.writeln("}");
            eval.writeln("else {");
            auto scope_2 = eval.indent_scope();
            define_eval(eval, flags, op, "left_eval", "left_ref", "field", true);
            // eval.writeln("result.insert(result.end(), left_eval.begin(), left_eval.end() - 1);");
            eval.writeln("ctx.this_as.push_back(left_eval.result);");
            define_ref(eval, flags, op, "right_ref", code_ref(flags, "right_ref"), "condition");
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "eval(ctx.ref(right_ref), ctx)", EvalResultMode::PASSTHROUGH);
            },
                      bm2::HookFileSub::field);
            // eval.writeln("result.insert(result.end(), right_eval.begin(), right_eval.end());");
            eval.writeln("ctx.this_as.pop_back();");
            scope_2.execute();
            eval.writeln("}");
        }
        else {
            eval_hook([&] {
                do_make_eval_result(eval, op, flags, "\"" + flags.wrap_comment("Unimplemented " + std::string(to_string(op))) + "\"", EvalResultMode::COMMENT);
            });
        }
        eval_hook([&] {}, bm2::HookFileSub::after);
        eval.writeln("break;");
        scope.execute();
        eval.writeln("}");
    }

    void write_eval_result(bm2::TmpCodeWriter& eval_result, Flags& flags) {
        eval_result.writeln("struct EvalResult {");
        {
            auto scope = eval_result.indent_scope();
            eval_result.writeln("std::string result;");
            may_write_from_hook(eval_result, flags, bm2::HookFile::eval_result, false);
        }
        eval_result.writeln("};");

        eval_result.writeln("EvalResult make_eval_result(std::string result) {");
        auto scope_make_eval_result = eval_result.indent_scope();
        eval_result.writeln("return EvalResult{std::move(result)};");
        scope_make_eval_result.execute();
        eval_result.writeln("}");
    }

    void write_eval_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& eval, Flags& flags) {
        w.writeln("EvalResult eval(const rebgn::Code& code, Context& ctx);");
        eval.writeln("EvalResult eval(const rebgn::Code& code, Context& ctx) {");
        auto scope_eval = eval.indent_scope();
        eval.writeln("EvalResult result;");
        eval.writeln("switch(code.op) {");

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }

            if (is_expr(op)) {
                write_eval(eval, op, flags);
            }
        }

        eval.writeln("default: {");
        eval.indent_writeln("result = make_eval_result(", unimplemented_comment(flags, "code.op"), ");");
        eval.indent_writeln("break;");
        eval.writeln("}");
        eval.write("}");  // close switch
        eval.writeln("return result;");
        scope_eval.execute();
        eval.writeln("}");  // close function
    }
}  // namespace rebgn
