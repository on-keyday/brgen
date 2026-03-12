/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include "define.hpp"
#include "generate.hpp"

namespace rebgn {
    void write_add_parameter(bm2::TmpCodeWriter& add_parameter,
                             bm2::TmpCodeWriter& add_call_parameter,
                             AbstractOp op, Flags& flags) {
        flags.set_func_name(bm2::FuncName::add_parameter,bm2::HookFile::param_op);
        if (op != AbstractOp::RETURN_TYPE) {
            add_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope = add_parameter.indent_scope();
            auto param_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
                if (stage == bm2::HookFileSub::main) {
                    may_write_from_hook(add_parameter, flags, bm2::HookFile::param_op, op, bm2::HookFileSub::pre_main);
                }
                if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_op, op, stage)) {
                    inner();
                }
            };
            param_hook([&] {}, bm2::HookFileSub::before);
            if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_each_code, false)) {
                add_parameter.writeln("if(params > 0) {");
                add_parameter.indent_writeln("w.write(\", \");");
                add_parameter.writeln("}");
            }
            if (op == AbstractOp::PROPERTY_INPUT_PARAMETER ||
                op == AbstractOp::DEFINE_PARAMETER ||
                op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                if (op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                    define_ident(add_parameter, flags, op, "ident", code_ref(flags, "ref"), "state variable");
                    define_type(add_parameter, flags, op, "type", "ctx.ref(ident_ref).type().value()", "state variable type");
                }
                else {
                    define_ident(add_parameter, flags, op, "ident", code_ref(flags, "ident"), "parameter");
                    define_type(add_parameter, flags, op, "type", code_ref(flags, "type"), "parameter type");
                }
                param_hook([&] {
                    if (auto& key = USE_FLAG(define_parameter); key.size()) {
                        std::map<std::string, std::string> param_map = {
                            {"IDENT", "\",ident,\""},
                            {"TYPE", "\",type,\""},
                        };
                        auto escaped = env_escape(flags, op, ENV_FLAG(define_parameter), param_map);
                        add_parameter.writeln("w.write(\"", escaped, "\");");
                    }
                    else if (USE_FLAG(prior_ident)) {
                        add_parameter.writeln("w.write(ident, \" ", USE_FLAG(param_type_separator), "\", type);");
                    }
                    else {
                        add_parameter.writeln("w.write(type, \" ", USE_FLAG(param_type_separator), "\", ident);");
                    }
                    add_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::ENCODER_PARAMETER) {
                param_hook([&] {
                    add_parameter.writeln("w.write(\"", USE_FLAG(encoder_param), "\");");
                    add_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::DECODER_PARAMETER) {
                param_hook([&] {
                    add_parameter.writeln("w.write(\"", USE_FLAG(decoder_param), "\");");
                    add_parameter.writeln("params++;");
                });
            }
            else {
                param_hook([&] {
                    add_parameter.writeln("w.write(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    add_parameter.writeln("params++;");
                });
            }
            param_hook([&] {}, bm2::HookFileSub::after);
            add_parameter.writeln("break;");
            scope.execute();
            add_parameter.writeln("}");

            flags.set_func_name(bm2::FuncName::add_call_parameter,bm2::HookFile::call_param_op);
            add_call_parameter.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope_call = add_call_parameter.indent_scope();
            auto call_param_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
                if (stage == bm2::HookFileSub::main) {
                    may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_op, op, bm2::HookFileSub::pre_main);
                }
                if (!may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_op, op, stage)) {
                    inner();
                }
            };
            call_param_hook([&] {}, bm2::HookFileSub::before);
            if (!may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_each_code, false)) {
                add_call_parameter.writeln("if(params > 0) {");
                add_call_parameter.indent_writeln("w.write(\", \");");
                add_call_parameter.writeln("}");
            }
            if (op == AbstractOp::PROPERTY_INPUT_PARAMETER) {
                define_ident(add_call_parameter, flags, op, "ident", code_ref(flags, "ident"), "parameter");
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(ident);");
                    add_call_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::STATE_VARIABLE_PARAMETER) {
                define_ident(add_call_parameter, flags, op, "ident", code_ref(flags, "ref"), "state variable");
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(ident);");
                    add_call_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::ENCODER_PARAMETER) {
                call_param_hook([&] {
                    add_call_parameter.writeln(
                        "w.write(ctx.w());");
                    add_call_parameter.writeln("params++;");
                });
            }
            else if (op == AbstractOp::DECODER_PARAMETER) {
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(ctx.r());");
                    add_call_parameter.writeln("params++;");
                });
            }
            else {
                call_param_hook([&] {
                    add_call_parameter.writeln("w.write(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
                    add_call_parameter.writeln("params++;");
                });
            }
            call_param_hook([&] {}, bm2::HookFileSub::after);
            add_call_parameter.writeln("break;");
            scope_call.execute();
            add_call_parameter.writeln("}");
        }
    }

    void write_parameter_func(bm2::TmpCodeWriter& w,
                              bm2::TmpCodeWriter& add_parameter,
                              bm2::TmpCodeWriter& add_call_parameter, Flags& flags) {
        w.writeln("void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        add_parameter.writeln("void add_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_add_parameter = add_parameter.indent_scope();
        add_parameter.writeln("size_t params = 0;");
        add_parameter.writeln("auto belong = ctx.bm.code[range.start].belong().value();");
        add_parameter.writeln("auto is_member = belong.value() != 0 && ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM;");
        if (!may_write_from_hook(add_parameter, flags, bm2::HookFile::param_start, false)) {
            if (USE_FLAG_BASE(rebgn::AbstractOp::DEFINE_PARAMETER, self_param).size()) {
                add_parameter.writeln("if(is_member) {");
                auto if_block_belong = add_parameter.indent_scope();
                add_parameter.writeln("w.write(\"", USE_FLAG_BASE(rebgn::AbstractOp::DEFINE_PARAMETER, self_param), "\");");
                add_parameter.writeln("params++;");
                if_block_belong.execute();
                add_parameter.writeln("}");
            }
        }
        add_parameter.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_add_parameter = add_parameter.indent_scope();
        add_parameter.writeln("auto& code = ctx.bm.code[i];");
        add_parameter.writeln("switch(code.op) {");
        auto scope_switch_add_parameter = add_parameter.indent_scope();

        w.writeln("void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        add_call_parameter.writeln("void add_call_parameter(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_add_call_parameter = add_call_parameter.indent_scope();
        add_call_parameter.writeln("size_t params = 0;");
        may_write_from_hook(add_call_parameter, flags, bm2::HookFile::call_param_start, false);
        add_call_parameter.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_add_call_parameter = add_call_parameter.indent_scope();
        add_call_parameter.writeln("auto& code = ctx.bm.code[i];");
        add_call_parameter.writeln("switch(code.op) {");
        auto scope_switch_add_call_parameter = add_call_parameter.indent_scope();

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }

            if (is_parameter_related(op)) {
                write_add_parameter(add_parameter, add_call_parameter, op, flags);
            }
        }

        add_parameter.writeln("default: {");
        add_parameter.indent_writeln("// skip other op");
        add_parameter.indent_writeln("break;");
        add_parameter.writeln("}");
        scope_switch_add_parameter.execute();
        add_parameter.writeln("}");
        scope_nest_add_parameter.execute();
        add_parameter.writeln("}");  // close for
        scope_add_parameter.execute();
        add_parameter.writeln("}");  // close function

        add_call_parameter.writeln("default: {");
        add_call_parameter.indent_writeln("// skip other op");
        add_call_parameter.indent_writeln("break;");
        add_call_parameter.writeln("}");
        scope_switch_add_call_parameter.execute();
        add_call_parameter.writeln("}");
        scope_nest_add_call_parameter.execute();
        add_call_parameter.writeln("}");  // close for
        scope_add_call_parameter.execute();
        add_call_parameter.writeln("}");  // close function
    }
}  // namespace rebgn