/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include "generate.hpp"
#include "define.hpp"

namespace rebgn {
    void write_func_decl(bm2::TmpCodeWriter& inner_function, AbstractOp op, Flags& flags) {
        auto& keyword = USE_FLAG(func_keyword);
        if (keyword.size()) {
            inner_function.writeln("w.write(\"", keyword, " \");");
        }
        if (!USE_FLAG(trailing_return_type)) {
            inner_function.writeln("if(ret_type) {");
            inner_function.indent_writeln("w.write(*ret_type);");
            inner_function.writeln("}");
            inner_function.writeln("else {");
            inner_function.indent_writeln("w.write(\"", USE_FLAG(func_void_return_type), "\");");
            inner_function.writeln("}");
            inner_function.writeln("w.write(\" \");");
        }
        inner_function.writeln("w.write(ident, \"", USE_FLAG(func_brace_ident_separator), "(\");");
        inner_function.writeln("add_parameter(ctx, w, inner_range);");
        inner_function.writeln("w.write(\") \");");
        if (USE_FLAG(trailing_return_type)) {
            inner_function.writeln("if(ret_type) {");
            inner_function.indent_writeln("w.write(\"", USE_FLAG(func_type_separator), "\", *ret_type);");
            inner_function.writeln("}");
            inner_function.writeln("else {");
            inner_function.indent_writeln("w.write(\"", USE_FLAG(func_void_return_type), "\");");
            inner_function.writeln("}");
        }
    }

    void write_return_type(bm2::TmpCodeWriter& inner_function, AbstractOp op, Flags& flags) {
        inner_function.writeln("auto found_type_pos = find_op(ctx,inner_range,rebgn::AbstractOp::RETURN_TYPE);");
        do_typed_variable_definition(inner_function, flags, op, "ret_type", "std::nullopt", "std::optional<std::string>", "function return type");
        inner_function.writeln("if(found_type_pos) {");
        {
            auto inner_scope = inner_function.indent_scope();
            inner_function.writeln("auto type_ref = ctx.bm.code[*found_type_pos].type().value();");
            inner_function.writeln("ret_type = type_to_string(ctx,type_ref);");
        }
        inner_function.writeln("}");
    }

    void write_inner_function(bm2::TmpCodeWriter& inner_function, AbstractOp op, Flags& flags) {
        flags.set_func_name(bm2::FuncName::inner_function, bm2::HookFile::inner_function_op);
        inner_function.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
        auto scope = inner_function.indent_scope();
        auto func_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_op, op, stage)) {
                inner();
            }
        };
        func_hook([&] {}, bm2::HookFileSub::before);
        if (op == AbstractOp::APPEND) {
            define_eval(inner_function, flags, op, "vector_eval", code_ref(flags, "left_ref"), "vector (not temporary)");
            define_eval(inner_function, flags, op, "new_element_eval", code_ref(flags, "right_ref"), "new element");
            func_hook([&] {
                std::map<std::string, std::string> map{
                    {"VECTOR", "\",vector_eval.result,\""},
                    {"ITEM", "\",new_element_eval.result,\""},
                };
                auto escaped = env_escape(flags, op, ENV_FLAG(append_method), map);
                inner_function.writeln("w.writeln(\"", escaped, USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::ASSIGN) {
            define_eval(inner_function, flags, op, "left_eval", code_ref(flags, "left_ref"), "assignment target");
            define_eval(inner_function, flags, op, "right_eval", code_ref(flags, "right_ref"), "assignment source");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"\", left_eval.result, \" = \", right_eval.result, \"", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::BACKWARD_INPUT || op == AbstractOp::BACKWARD_OUTPUT) {
            define_eval(inner_function, flags, op, "evaluated", code_ref(flags, "ref"), "backward offset to move (in byte)");
            func_hook([&] {
                if (op == AbstractOp::BACKWARD_INPUT) {
                    std::map<std::string, std::string> map{
                        {"DECODER", "\",ctx.r(),\""},
                        {"OFFSET", "\",evaluated.result,\""},
                    };
                    auto escaped = env_escape(flags, op, ENV_FLAG(decode_backward), map);
                    inner_function.writeln("w.writeln(\"", escaped, USE_FLAG(end_of_statement), "\");");
                }
                else {
                    std::map<std::string, std::string> map{
                        {"ENCODER", "\",ctx.w(),\""},
                        {"OFFSET", "\",evaluated.result,\""},
                    };
                    auto escaped = env_escape(flags, op, ENV_FLAG(encode_backward), map);
                    inner_function.writeln("w.writeln(\"", escaped, USE_FLAG(end_of_statement), "\");");
                }
            });
        }
        else if (op == AbstractOp::LENGTH_CHECK) {
            define_eval(inner_function, flags, op, "vector_eval", code_ref(flags, "left_ref"), "vector to check");
            define_eval(inner_function, flags, op, "size_eval", code_ref(flags, "right_ref"), "size to check");
            func_hook([&] {
                std::map<std::string, std::string> map2{
                    {"VECTOR", "\",vector_eval.result,\""},
                    {"SIZE", "\",size_eval.result,\""},
                };
                auto escaped = env_escape(flags, op, ENV_FLAG(length_check_method), map2);
                inner_function.writeln("w.writeln(\"", escaped, USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::DEFINE_VARIABLE || op == AbstractOp::DECLARE_VARIABLE) {
            if (op == AbstractOp::DECLARE_VARIABLE) {
                define_ident(inner_function, flags, op, "ident", code_ref(flags, "ref"), "variable");
                define_eval(inner_function, flags, op, "init", code_ref(flags, "ref", ctx_ref(flags, code_ref(flags, "ref"))), "variable initialization");
                define_type(inner_function, flags, op, "type", code_ref(flags, "type", ctx_ref(flags, code_ref(flags, "ref"))), "variable");
            }
            else {
                define_ident(inner_function, flags, op, "ident", code_ref(flags, "ident"), "variable");
                define_eval(inner_function, flags, op, "init", code_ref(flags, "ref"), "variable initialization");
                define_type(inner_function, flags, op, "type", code_ref(flags, "type"), "variable");
            }
            func_hook([&] {
                if (auto& key = USE_FLAG(define_variable); key.size()) {
                    std::map<std::string, std::string> map{
                        {"IDENT", "\",ident,\""},
                        {"INIT", "\",init.result,\""},
                        {"TYPE", "\",type,\""},
                    };
                    auto escaped = env_escape(flags, op, ENV_FLAG(define_variable), map);
                    inner_function.writeln("w.writeln(\"", escaped, USE_FLAG(end_of_statement), "\");");
                }
                else if (USE_FLAG(omit_type_on_define_var)) {
                    inner_function.writeln("w.writeln(std::format(\"", USE_FLAG(define_var_keyword), "{} ", USE_FLAG(define_var_assign), " {}", USE_FLAG(end_of_statement), "\", ident, init.result));");
                }
                else {
                    if (USE_FLAG(prior_ident)) {
                        inner_function.writeln("w.writeln(std::format(\"", USE_FLAG(define_var_keyword), "{} ", USE_FLAG(var_type_separator), "{} ", USE_FLAG(define_var_assign), " {}", USE_FLAG(end_of_statement), "\", ident, type, init.result));");
                    }
                    else {
                        inner_function.writeln("w.writeln(std::format(\"", USE_FLAG(define_var_keyword), "{} ", USE_FLAG(var_type_separator), "{} ", USE_FLAG(define_var_assign), " {}", USE_FLAG(end_of_statement), "\",type, ident, init.result));");
                    }
                }
            });
        }
        else if (op == AbstractOp::RESERVE_SIZE) {
            define_eval(inner_function, flags, op, "vector_eval", code_ref(flags, "left_ref"), "vector");
            define_eval(inner_function, flags, op, "size_eval", code_ref(flags, "right_ref"), "size");
            do_variable_definition(inner_function, flags, op, "reserve_type", code_ref(flags, "reserve_type"), "rebgn::ReserveType", "reserve vector type");
            func_hook([&] {
                inner_function.writeln("if(reserve_type == rebgn::ReserveType::STATIC) {");
                auto scope = inner_function.indent_scope();
                func_hook([&] {
                    std::map<std::string, std::string> map{
                        {"VECTOR", "\",vector_eval.result,\""},
                        {"SIZE", "\",size_eval.result,\""},
                    };
                    auto escaped = env_escape(flags, op, ENV_FLAG(reserve_size_static), map);
                    inner_function.writeln("w.writeln(\"", escaped, "\");");
                },
                          bm2::HookFileSub::static_);
                scope.execute();
                inner_function.writeln("}");
                inner_function.writeln("else if(reserve_type == rebgn::ReserveType::DYNAMIC) {");
                auto scope2 = inner_function.indent_scope();
                func_hook([&] {
                    std::map<std::string, std::string> map{
                        {"VECTOR", "\",vector_eval.result,\""},
                        {"SIZE", "\",size_eval.result,\""},
                    };
                    auto escaped = env_escape(flags, op, ENV_FLAG(reserve_size_dynamic), map);
                    inner_function.writeln("w.writeln(\"", escaped, "\");");
                },
                          bm2::HookFileSub::dynamic);
                scope2.execute();
                inner_function.writeln("}");
            });
        }
        else if (op == AbstractOp::DEFINE_CONSTANT) {
            define_ident(inner_function, flags, op, "ident", code_ref(flags, "ident"), "constant name");
            define_eval(inner_function, flags, op, "init", code_ref(flags, "ref"), "constant value");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", USE_FLAG(define_const_keyword), "\", ident, \" = \", init.result,\"", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::BEGIN_ENCODE_SUB_RANGE || op == AbstractOp::BEGIN_DECODE_SUB_RANGE) {
            define_eval(inner_function, flags, op, "evaluated", code_ref(flags, "ref"), "sub range length or vector expression");
            do_variable_definition(inner_function, flags, op, "sub_range_type", code_ref(flags, "sub_range_type"), "SubRangeType", "sub range type (byte_len or replacement)");
            func_hook([&] {
                inner_function.writeln("if(sub_range_type == rebgn::SubRangeType::byte_len) {");
                auto scope = inner_function.indent_scope();
                func_hook([&] {
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                },
                          bm2::HookFileSub::byte_length);
                scope.execute();
                inner_function.writeln("}");
                inner_function.writeln("else if(sub_range_type == rebgn::SubRangeType::replacement) {");
                auto scope2 = inner_function.indent_scope();
                func_hook([&] {
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                },
                          bm2::HookFileSub::replacement);
                scope2.execute();
                inner_function.writeln("}");
                inner_function.writeln("else {");
                auto scope3 = inner_function.indent_scope();
                inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                scope3.execute();
                inner_function.writeln("}");
            });
        }
        else if (op == AbstractOp::BEGIN_ENCODE_PACKED_OPERATION || op == AbstractOp::BEGIN_DECODE_PACKED_OPERATION ||
                 op == AbstractOp::END_ENCODE_PACKED_OPERATION || op == AbstractOp::END_DECODE_PACKED_OPERATION ||
                 op == AbstractOp::DYNAMIC_ENDIAN) {
            define_ref(inner_function, flags, op, "fallback", code_ref(flags, "fallback"), "fallback operation");
            func_hook([&] {
                inner_function.writeln("if(fallback.value() != 0) {");
                auto scope = inner_function.indent_scope();
                define_range(inner_function, flags, op, "inner_range", "fallback", "fallback operation");
                func_hook([&] {
                    inner_function.writeln("inner_function(ctx, w, inner_range);");
                },
                          bm2::HookFileSub::fallback);
                scope.execute();
                inner_function.writeln("}");
                inner_function.writeln("else {");
                auto scope2 = inner_function.indent_scope();
                func_hook([&] {
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                },
                          bm2::HookFileSub::no_fallback);
                scope2.execute();
                inner_function.writeln("}");
            });
        }
        else if (op == AbstractOp::ASSERT) {
            define_eval(inner_function, flags, op, "evaluated", code_ref(flags, "ref"), "assertion condition");
            func_hook([&] {
                std::map<std::string, std::string> map{
                    {"CONDITION", "\",evaluated.result,\""},
                };
                auto escaped = env_escape(flags, op, ENV_FLAG(assert_method), map);
                inner_function.writeln("w.writeln(\"", escaped, USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::EXPLICIT_ERROR) {
            do_variable_definition(inner_function, flags, op, "param", code_ref(flags, "param"), "Param", "error message parameters");
            define_eval(inner_function, flags, op, "evaluated", "param.refs[0]", "error message", true);
            func_hook([&] {
                inner_function.writeln("w.writeln(\"throw std::runtime_error(\", evaluated.result, \")", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::IF || op == AbstractOp::ELIF ||
                 op == AbstractOp::ELSE || op == AbstractOp::LOOP_INFINITE ||
                 op == AbstractOp::LOOP_CONDITION || op == AbstractOp::DEFINE_FUNCTION ||
                 op == AbstractOp::MATCH || op == AbstractOp::EXHAUSTIVE_MATCH ||
                 op == AbstractOp::CASE || op == AbstractOp::DEFAULT_CASE) {
            if (op == AbstractOp::IF || op == AbstractOp::ELIF || op == AbstractOp::LOOP_CONDITION ||
                op == AbstractOp::MATCH || op == AbstractOp::EXHAUSTIVE_MATCH ||
                op == AbstractOp::CASE) {
                define_eval(inner_function, flags, op, "evaluated", code_ref(flags, "ref"), "condition");
            }
            if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                inner_function.writeln("defer.pop_back();");
            }

            if (op == AbstractOp::DEFINE_FUNCTION) {
                define_ident(inner_function, flags, op, "ident", code_ref(flags, "ident"), "function");
                do_variable_definition(inner_function, flags, op, "func_type", code_ref(flags, "func_type"), "rebgn::FunctionType", "function type");
                define_bool(inner_function, flags, op, "is_empty_block", "i + 1 < ctx.bm.code.size() && ctx.bm.code[i + 1].op == rebgn::AbstractOp::END_FUNCTION", "empty block");
                // inner_function.writeln("auto range = ctx.range(code.ident().value());");
                do_variable_definition(inner_function, flags, op, "inner_range", "range", "Range", "function range");
                write_return_type(inner_function, op, flags);
                do_typed_variable_definition(inner_function, flags, op, "belong_name", "std::nullopt", "std::optional<std::string>", "function belong name");
                inner_function.writeln("if(auto belong = code.belong();belong&&belong->value()!=0) {");
                {
                    auto inner_scope = inner_function.indent_scope();
                    inner_function.writeln("belong_name = ctx.ident(belong.value());");
                }
                inner_function.writeln("}");
                func_hook([&] {
                    write_func_decl(inner_function, op, flags);
                    inner_function.writeln("w.writeln(\"", USE_FLAG(block_begin), "\");");
                });
            }
            else {
                if (op == AbstractOp::IF || op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                    define_bool(inner_function, flags, op, "is_empty_block", "find_next_else_or_end_if(ctx, i, true) == i + 1 || ctx.bm.code[i + 1].op == rebgn::AbstractOp::BEGIN_COND_BLOCK", "empty block");
                }
                else if (op == AbstractOp::LOOP_CONDITION || op == AbstractOp::LOOP_INFINITE) {
                    define_bool(inner_function, flags, op, "is_empty_block", "find_next_end_loop(ctx, i) == i + 1", "empty block");
                }
                func_hook([&] {
                    if (op == AbstractOp::ELIF || op == AbstractOp::ELSE) {
                        if (USE_FLAG(otbs_on_block_end)) {
                            inner_function.writeln("w.write(\"", USE_FLAG(block_end), "\");");
                        }
                        else {
                            inner_function.writeln("w.writeln(\"", USE_FLAG(block_end), "\");");
                        }
                    }
                    inner_function.write("w.writeln(\"");
                    std::string condition = "\",evaluated.result,\"";
                    if (USE_FLAG(condition_has_parentheses)) {
                        condition = "(" + condition + ")";
                    }
                    switch (op) {
                        case AbstractOp::IF:
                            inner_function.write(USE_FLAG(if_keyword), " ", condition, " ", USE_FLAG(block_begin));
                            break;
                        case AbstractOp::ELIF:
                            inner_function.write(USE_FLAG(elif_keyword), " ", condition, " ", USE_FLAG(block_begin));
                            break;
                        case AbstractOp::ELSE:
                            inner_function.write(USE_FLAG(else_keyword), " ", USE_FLAG(block_begin));
                            break;
                        case AbstractOp::LOOP_INFINITE:
                            inner_function.write(USE_FLAG(infinity_loop), " ", USE_FLAG(block_begin));
                            break;
                        case AbstractOp::LOOP_CONDITION:
                            inner_function.write(USE_FLAG(conditional_loop), " ", condition, " ", USE_FLAG(block_begin));
                            break;
                        case AbstractOp::MATCH:
                        case AbstractOp::EXHAUSTIVE_MATCH:
                            inner_function.write(USE_FLAG(match_keyword), " ", condition, " ", USE_FLAG(block_begin));
                            break;
                        case AbstractOp::CASE:
                            inner_function.write(USE_FLAG(match_case_keyword), " ", condition, " ", USE_FLAG(match_case_separator), USE_FLAG(block_begin));
                            break;
                        case AbstractOp::DEFAULT_CASE:
                            inner_function.write(USE_FLAG(match_default_keyword), " ", USE_FLAG(block_begin));
                            break;
                        default:;
                    }
                    inner_function.writeln("\");");
                });
            }
            inner_function.writeln("defer.push_back(w.indent_scope_ex());");
        }
        else if (op == AbstractOp::END_IF || op == AbstractOp::END_LOOP ||
                 op == AbstractOp::END_FUNCTION || op == AbstractOp::END_MATCH ||
                 op == AbstractOp::END_CASE) {
            inner_function.writeln("defer.pop_back();");
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", USE_FLAG(block_end), "\");");
            });
        }
        else if (op == AbstractOp::CONTINUE) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"continue", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::BREAK) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"break", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::RET) {
            define_ref(inner_function, flags, op, "ref", code_ref(flags, "ref"), "return value");
            inner_function.writeln("if(ref.value() != 0) {");
            auto scope = inner_function.indent_scope();
            define_eval(inner_function, flags, op, "evaluated", "ref", "return value", true);
            func_hook([&] {
                inner_function.writeln("w.writeln(\"return \", evaluated.result, \"", USE_FLAG(end_of_statement), "\");");
            },
                      bm2::HookFileSub::value);
            scope.execute();
            inner_function.writeln("}");
            inner_function.writeln("else {");
            auto else_scope = inner_function.indent_scope();
            func_hook([&] {
                inner_function.writeln("w.writeln(\"return", USE_FLAG(end_of_statement), "\");");
            },
                      bm2::HookFileSub::empty);
            else_scope.execute();
            inner_function.writeln("}");
        }
        else if (op == AbstractOp::RET_SUCCESS) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", USE_FLAG(coder_success), USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::RET_PROPERTY_SETTER_OK) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", USE_FLAG(property_setter_ok), USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::RET_PROPERTY_SETTER_FAIL) {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", USE_FLAG(property_setter_fail), USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::INC) {
            define_eval(inner_function, flags, op, "evaluated", code_ref(flags, "ref"), "increment target");
            func_hook([&] {
                inner_function.writeln("w.writeln(evaluated.result, \"+= 1", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::CHECK_UNION || op == AbstractOp::SWITCH_UNION) {
            define_ref(inner_function, flags, op, "union_member_ref", code_ref(flags, "ref"), "current union member");
            define_ref(inner_function, flags, op, "union_ref", "ctx.ref(union_member_ref).belong().value()", "union");
            define_ref(inner_function, flags, op, "union_field_ref", "ctx.ref(union_ref).belong().value()", "union field");
            define_uint(inner_function, flags, op, "union_member_index", "ctx.ref(union_member_ref).int_value()->value()", "current union member index");
            define_ident(inner_function, flags, op, "union_member_ident", "union_member_ref", "union member", true);
            define_ident(inner_function, flags, op, "union_ident", "union_ref", "union", true);
            define_eval(inner_function, flags, op, "union_field_ident", "union_field_ref", "union field", true);
            if (op == AbstractOp::CHECK_UNION) {
                do_variable_definition(inner_function, flags, op, "check_type", "code.check_at().value()", "rebgn::UnionCheckAt", "union check location");
            }
            func_hook([&] {
                std::map<std::string, std::string> map{
                    {"MEMBER_IDENT", "\",union_member_ident,\""},
                    {"MEMBER_FULL_IDENT", "\",type_accessor(ctx.ref(union_member_ref),ctx),\""},
                    {"UNION_IDENT", "\",union_ident,\""},
                    {"UNION_FULL_IDENT", "\",type_accessor(ctx.ref(union_ref),ctx),\""},
                    {"FIELD_IDENT", "\",union_field_ident.result,\""},
                    {"MEMBER_INDEX", "\",futils::number::to_string<std::string>(union_member_index),\""},
                };
                auto escaped = env_escape(flags, op, ENV_FLAG(check_union_condition), map);
                inner_function.writeln("w.writeln(\"", USE_FLAG(if_keyword), USE_FLAG(condition_has_parentheses) ? "(" : " ",
                                       escaped, USE_FLAG(condition_has_parentheses) ? ") " : " ", USE_FLAG(block_begin), "\");");
                inner_function.writeln("auto scope = w.indent_scope_ex();");
                if (op == AbstractOp::CHECK_UNION) {
                    inner_function.writeln("if(check_type == rebgn::UnionCheckAt::ENCODER) {");
                    auto encoder_scope = inner_function.indent_scope();
                    auto ret = env_escape(flags, op, ENV_FLAG(check_union_fail_return_value), map);
                    inner_function.writeln("w.writeln(\"return ", ret, USE_FLAG(end_of_statement), "\");");
                    encoder_scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else if(check_type == rebgn::UnionCheckAt::PROPERTY_GETTER_PTR) {");
                    auto property_getter_ptr_scope = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"return ", USE_FLAG(empty_pointer), USE_FLAG(end_of_statement), "\");");
                    property_getter_ptr_scope.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else if(check_type == rebgn::UnionCheckAt::PROPERTY_GETTER_OPTIONAL) {");
                    auto property_getter_optional_scope = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"return ", USE_FLAG(empty_optional), USE_FLAG(end_of_statement), "\");");
                    property_getter_optional_scope.execute();
                    inner_function.writeln("}");
                }
                else {
                    auto switch_union = env_escape(flags, op, ENV_FLAG(switch_union), map);
                    inner_function.writeln("w.writeln(\"", switch_union, USE_FLAG(end_of_statement), "\");");
                }
                inner_function.writeln("scope.execute();");
                inner_function.writeln("w.writeln(\"", USE_FLAG(block_end), "\");");
            });
        }
        else if (op == AbstractOp::CALL_ENCODE || op == AbstractOp::CALL_DECODE) {
            define_ref(inner_function, flags, op, "func_ref", code_ref(flags, "left_ref"), "function");
            define_ref(inner_function, flags, op, "func_belong", "ctx.ref(func_ref).belong().value()", "function belong");
            do_variable_definition(inner_function, flags, op, "func_belong_name", "type_accessor(ctx.ref(func_belong), ctx)", "string", "function belong name");
            define_ident(inner_function, flags, op, "func_name", "func_ref", "function", true);
            define_eval(inner_function, flags, op, "obj_eval", code_ref(flags, "right_ref"), "`this` object");
            define_range(inner_function, flags, op, "inner_range", "func_ref", "function call range");
            func_hook([&] {
                inner_function.writeln("w.write(obj_eval.result, \".\", func_name, \"(\");");
                inner_function.writeln("add_call_parameter(ctx, w, inner_range);");
                inner_function.writeln("w.writeln(\")", USE_FLAG(end_of_statement), "\");");
            });
        }
        else if (op == AbstractOp::ENCODE_INT || op == AbstractOp::DECODE_INT ||
                 op == AbstractOp::ENCODE_INT_VECTOR || op == AbstractOp::DECODE_INT_VECTOR ||
                 op == AbstractOp::ENCODE_INT_VECTOR_FIXED || op == AbstractOp::DECODE_INT_VECTOR_FIXED ||
                 op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF || op == AbstractOp::PEEK_INT_VECTOR) {
            define_ref(inner_function, flags, op, "fallback", code_ref(flags, "fallback"), "fallback operation");
            func_hook([&] {
                inner_function.writeln("if(fallback.value() != 0) {");
                auto indent = inner_function.indent_scope();
                define_range(inner_function, flags, op, "inner_range", "fallback", "fallback operation");
                func_hook([&] {
                    inner_function.writeln("inner_function(ctx, w, inner_range);");
                },
                          bm2::HookFileSub::fallback);
                indent.execute();
                inner_function.writeln("}");
                inner_function.writeln("else {");
                auto indent2 = inner_function.indent_scope();
                define_uint(inner_function, flags, op, "bit_size", "code.bit_size()->value()", "bit size of element");
                do_variable_definition(inner_function, flags, op, "endian", "code.endian().value()", "rebgn::Endian", "endian of element");
                if (op == AbstractOp::ENCODE_INT || op == AbstractOp::DECODE_INT ||
                    op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF) {
                    define_eval(inner_function, flags, op, "evaluated", code_ref(flags, "ref"), "element");
                }
                else {
                    define_eval(inner_function, flags, op, "vector_value", code_ref(flags, "left_ref"), "vector");
                    define_eval(inner_function, flags, op, "size_value", code_ref(flags, "right_ref"), "size");
                }
                func_hook([&] {
                    inner_function.writeln("if(bit_size == 8) {");
                    auto indent3 = inner_function.indent_scope();
                    func_hook([&] {
                        if (op == AbstractOp::ENCODE_INT_VECTOR || op == AbstractOp::ENCODE_INT_VECTOR_FIXED) {
                            std::map<std::string, std::string> map{
                                {"ENCODER", "\",ctx.w(),\""},
                                {"LEN", "\",size_value.result,\""},
                                {"VALUE", "\",vector_value.result,\""},
                            };
                            auto escaped = env_escape(flags, op, ENV_FLAG(encode_bytes_op), map);
                            inner_function.writeln("w.writeln(\"", escaped, "\");");
                        }
                        else if (op == AbstractOp::DECODE_INT_VECTOR || op == AbstractOp::DECODE_INT_VECTOR_FIXED) {
                            std::map<std::string, std::string> map{
                                {"DECODER", "\",ctx.r(),\""},
                                {"LEN", "\",size_value.result,\""},
                                {"VALUE", "\",vector_value.result,\""},
                            };
                            auto escaped = env_escape(flags, op, ENV_FLAG(decode_bytes_op), map);
                            inner_function.writeln("w.writeln(\"", escaped, "\");");
                        }
                        else if (op == AbstractOp::DECODE_INT_VECTOR_UNTIL_EOF) {
                            std::map<std::string, std::string> map{
                                {"DECODER", "\",ctx.r(),\""},
                                {"VALUE", "\",evaluated.result,\""},
                            };
                            auto escaped = env_escape(flags, op, ENV_FLAG(decode_bytes_until_eof_op), map);
                            inner_function.writeln("w.writeln(\"", escaped, "\");");
                        }
                        else if (op == AbstractOp::PEEK_INT_VECTOR) {
                            std::map<std::string, std::string> map{
                                {"DECODER", "\",ctx.r(),\""},
                                {"LEN", "\",size_value.result,\""},
                                {"VALUE", "\",vector_value.result,\""},
                            };
                            auto escaped = env_escape(flags, op, ENV_FLAG(peek_bytes_op), map);
                            inner_function.writeln("w.writeln(\"", escaped, "\");");
                        }
                        else {
                            inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                        }
                    },
                              bm2::HookFileSub::bytes);
                    indent3.execute();
                    inner_function.writeln("}");
                    inner_function.writeln("else {");
                    auto indent4 = inner_function.indent_scope();
                    inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), "\");");
                    indent4.execute();
                    inner_function.writeln("}");
                },
                          bm2::HookFileSub::no_fallback);
                indent2.execute();
                inner_function.writeln("}");
            });
        }
        else {
            func_hook([&] {
                inner_function.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
            });
        }
        func_hook([&] {}, bm2::HookFileSub::after);
        inner_function.writeln("break;");
        scope.execute();
        inner_function.writeln("}");
    }

    void write_inner_function_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_function, Flags& flags) {
        w.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        inner_function.writeln("void inner_function(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_function = inner_function.indent_scope();
        inner_function.writeln("std::vector<futils::helper::DynDefer> defer;");
        may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_start, false);
        inner_function.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_function = inner_function.indent_scope();
        inner_function.writeln("auto& code = ctx.bm.code[i];");
        may_write_from_hook(inner_function, flags, bm2::HookFile::inner_function_each_code, false);
        inner_function.writeln("switch(code.op) {");

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }

            if ((!is_expr(op) && !is_struct_define_related(op) && !is_parameter_related(op)) || is_both_expr_and_def(op)) {
                write_inner_function(inner_function, op, flags);
            }
        }

        inner_function.writeln("default: {");
        auto if_function = inner_function.indent_scope();
        inner_function.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_struct_define_related(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {");
        inner_function.indent_writeln("w.writeln(", unimplemented_comment(flags, "code.op"), ");");
        inner_function.writeln("}");
        inner_function.writeln("break;");
        if_function.execute();
        inner_function.writeln("}");
        inner_function.writeln("}");  // close switch
        scope_nest_function.execute();
        inner_function.writeln("}");  // close for
        scope_function.execute();
        inner_function.writeln("}");  // close function
    }

}  // namespace rebgn
