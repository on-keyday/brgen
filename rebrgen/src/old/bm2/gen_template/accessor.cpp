/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include "define.hpp"
#include "generate.hpp"

namespace rebgn {
    void write_field_accessor(bm2::TmpCodeWriter& field_accessor, bm2::TmpCodeWriter& type_accessor, AbstractOp op, Flags& flags) {
        auto field_accessor_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(field_accessor, flags, bm2::HookFile::field_accessor_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(field_accessor, flags, bm2::HookFile::field_accessor_op, op, stage)) {
                inner();
            }
        };

        auto add_start = [&](auto&& inner) {
            flags.set_func_name(bm2::FuncName::field_accessor,bm2::HookFile::field_accessor_op);
            field_accessor.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope = field_accessor.indent_scope();
            field_accessor_hook([&] {}, bm2::HookFileSub::before);
            inner();
            field_accessor_hook([&] {}, bm2::HookFileSub::after);
            field_accessor.writeln("break;");
            scope.execute();
            field_accessor.writeln("}");
        };

        auto type_accessor_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(type_accessor, flags, bm2::HookFile::type_accessor_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(type_accessor, flags, bm2::HookFile::type_accessor_op, op, stage)) {
                inner();
            }
        };
        auto add_type_start = [&](auto&& inner) {
            flags.set_func_name(bm2::FuncName::type_accessor,bm2::HookFile::type_accessor_op);
            type_accessor.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
            auto scope = type_accessor.indent_scope();
            type_accessor_hook([&] {}, bm2::HookFileSub::before);
            inner();
            type_accessor_hook([&] {}, bm2::HookFileSub::after);
            type_accessor.writeln("break;");
            scope.execute();
            type_accessor.writeln("}");
        };
        if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE) {
            add_start([&] {
                field_accessor_hook([&] {
                    do_make_eval_result(field_accessor, op, flags, "ctx.this_()", EvalResultMode::TEXT);
                });
            });
            add_type_start([&] {
                std::string desc = to_string(op);
                desc.erase(0, 7);  // remove "DEFINE_"
                define_ident(type_accessor, flags, op, "ident", code_ref(flags, "ident"), desc);
                type_accessor_hook([&] {
                    type_accessor.writeln("result = ident;");
                });
            });
        }
        else if (op == AbstractOp::DEFINE_UNION || op == AbstractOp::DEFINE_UNION_MEMBER ||
                 op == AbstractOp::DEFINE_FIELD || op == AbstractOp::DEFINE_BIT_FIELD ||
                 op == AbstractOp::DEFINE_PROPERTY) {
            std::string desc = to_string(op);
            desc.erase(0, 7);  // remove "DEFINE_"
            add_start([&] {
                define_ident(field_accessor, flags, op, "ident", code_ref(flags, "ident"), desc);
                define_ref(field_accessor, flags, op, "belong", code_ref(flags, "belong"), "belong");
                define_bool(field_accessor, flags, op, "is_member", "belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM", "is member of a struct");
                if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                    define_ref(field_accessor, flags, op, "union_member_ref", "code.ident().value()", "union member");
                    define_ref(field_accessor, flags, op, "union_ref", "belong", "union");
                    define_ref(field_accessor, flags, op, "union_field_ref", "ctx.ref(union_ref).belong().value()", "union field");
                    define_ref(field_accessor, flags, op, "union_field_belong", "ctx.ref(union_field_ref).belong().value()", "union field belong");
                    do_variable_definition(field_accessor, flags, op, "union_field_eval", "field_accessor(ctx.ref(union_field_ref),ctx)", "string", "field accessor");
                }
                else if (op == AbstractOp::DEFINE_BIT_FIELD) {
                    do_variable_definition(field_accessor, flags, op, "belong_eval", "field_accessor(ctx.ref(belong),ctx)", "string", "field accessor");
                }
                field_accessor_hook([&] {
                    if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                        do_make_eval_result(field_accessor, op, flags, "union_field_eval", EvalResultMode::PASSTHROUGH);
                    }
                    else if (op == AbstractOp::DEFINE_BIT_FIELD && !USE_FLAG(compact_bit_field)) {
                        do_make_eval_result(field_accessor, op, flags, "belong_eval", EvalResultMode::PASSTHROUGH);
                    }
                    else {
                        field_accessor.writeln("if(is_member) {");
                        auto scope = field_accessor.indent_scope();
                        do_variable_definition(field_accessor, flags, op, "belong_eval", "field_accessor(ctx.ref(belong), ctx)", "string", "belong eval");
                        field_accessor_hook([&] {
                            do_make_eval_result(field_accessor, op, flags, "std::format(\"{}.{}\", belong_eval.result, ident)", EvalResultMode::TEXT);
                        },
                                            bm2::HookFileSub::field);
                        scope.execute();
                        field_accessor.writeln("}");
                        field_accessor.writeln("else {");
                        auto scope2 = field_accessor.indent_scope();
                        field_accessor_hook([&] {
                            do_make_eval_result(field_accessor, op, flags, "ident", EvalResultMode::TEXT);
                        },
                                            bm2::HookFileSub::self);
                        scope2.execute();
                        field_accessor.writeln("}");
                    }
                });
            });
            add_type_start([&] {
                define_ident(type_accessor, flags, op, "ident", code_ref(flags, "ident"), desc);
                define_ref(type_accessor, flags, op, "belong", code_ref(flags, "belong"), "belong");
                define_bool(type_accessor, flags, op, "is_member", "belong.value() != 0&& ctx.ref(belong).op != rebgn::AbstractOp::DEFINE_PROGRAM", "is member of a struct");
                if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                    define_ref(type_accessor, flags, op, "union_member_ref", "code.ident().value()", "union member");
                    define_ref(type_accessor, flags, op, "union_ref", "belong", "union");
                    define_ref(type_accessor, flags, op, "union_field_ref", "ctx.ref(union_ref).belong().value()", "union field");
                    define_ref(type_accessor, flags, op, "union_field_belong", "ctx.ref(union_field_ref).belong().value()", "union field belong");
                }
                type_accessor_hook([&] {
                    if (op == AbstractOp::DEFINE_UNION_MEMBER) {
                        do_variable_definition(type_accessor, flags, op, "belong_eval", "type_accessor(ctx.ref(union_field_belong),ctx)", "string", "field accessor");
                        type_accessor_hook([&] {
                            type_accessor.writeln("result = std::format(\"{}.{}\", belong_eval, ident);");
                        },
                                           bm2::HookFileSub::field);
                    }
                    else if (op == AbstractOp::DEFINE_BIT_FIELD) {
                        type_accessor_hook([&] {
                            type_accessor.writeln("result = type_accessor(ctx.ref(belong),ctx);");
                        },
                                           bm2::HookFileSub::field);
                    }
                    else {
                        type_accessor.writeln("if(is_member) {");
                        auto scope = type_accessor.indent_scope();
                        do_variable_definition(type_accessor, flags, op, "belong_eval", "type_accessor(ctx.ref(belong),ctx)", "string", "field accessor");
                        type_accessor_hook([&] {
                            type_accessor.writeln("result = std::format(\"{}.{}\", belong_eval, ident);");
                        },
                                           bm2::HookFileSub::field);
                        scope.execute();
                        type_accessor.writeln("}");
                        type_accessor.writeln("else {");
                        auto scope2 = type_accessor.indent_scope();
                        type_accessor_hook([&] {
                            type_accessor.writeln("result = ident;");
                        },
                                           bm2::HookFileSub::self);
                        scope2.execute();
                        type_accessor.writeln("}");
                    }
                });
            });
        }
    }

    void write_field_accessor_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& field_accessor,
                                   bm2::TmpCodeWriter& type_accessor, Flags& flags) {
        w.writeln("EvalResult field_accessor(const rebgn::Code& code, Context& ctx);");
        field_accessor.writeln("EvalResult field_accessor(const rebgn::Code& code, Context& ctx) {");
        auto scope_field_accessor = field_accessor.indent_scope();
        field_accessor.writeln("EvalResult result;");
        field_accessor.writeln("switch(code.op) {");

        w.writeln("std::string type_accessor(const rebgn::Code& code, Context& ctx);");
        type_accessor.writeln("std::string type_accessor(const rebgn::Code& code, Context& ctx) {");
        auto scope_type_accessor = type_accessor.indent_scope();
        type_accessor.writeln("std::string result;");
        type_accessor.writeln("switch(code.op) {");

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }
            if (is_struct_define_related(op)) {
                write_field_accessor(field_accessor, type_accessor, op, flags);
            }
        }

        field_accessor.writeln("default: {");
        auto if_block_field_accessor = field_accessor.indent_scope();
        do_make_eval_result(field_accessor, AbstractOp::DEFINE_PROGRAM, flags, unimplemented_comment(flags, "code.op"), EvalResultMode::COMMENT);
        field_accessor.writeln("break;");
        if_block_field_accessor.execute();
        field_accessor.writeln("}");  // close default
        field_accessor.writeln("}");  // close switch
        field_accessor.writeln("return result;");
        scope_field_accessor.execute();
        field_accessor.writeln("}");  // close function

        type_accessor.writeln("default: {");
        auto if_block_type_accessor = type_accessor.indent_scope();
        type_accessor.writeln("return ", unimplemented_comment(flags, "code.op"), ";");
        if_block_type_accessor.execute();
        type_accessor.writeln("}");  // close default
        type_accessor.writeln("}");  // close switch
        type_accessor.writeln("return result;");
        scope_type_accessor.execute();
        type_accessor.writeln("}");  // close function
    }
}  // namespace rebgn