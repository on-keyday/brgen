/*license*/
#include "../context.hpp"
#include "flags.hpp"
#include "hook_load.hpp"
#include "define.hpp"
#include "generate.hpp"

namespace rebgn {
    void write_inner_block(bm2::TmpCodeWriter& inner_block,
                           AbstractOp op, Flags& flags) {
        flags.set_func_name(bm2::FuncName::inner_block,bm2::HookFile::inner_block_op);
        inner_block.writeln(std::format("case rebgn::AbstractOp::{}: {{", to_string(op)));
        auto scope = inner_block.indent_scope();
        auto block_hook = [&](auto&& inner, bm2::HookFileSub stage = bm2::HookFileSub::main) {
            if (stage == bm2::HookFileSub::main) {
                may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_op, op, bm2::HookFileSub::pre_main);
            }
            if (!may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_op, op, stage)) {
                inner();
            }
        };
        block_hook([&] {}, bm2::HookFileSub::before);
        std::string desc = to_string(op);
        if (desc.starts_with("DECLARE_")) {
            desc.erase(0, 8);  // remove "DECLARE_"
        }
        if (desc.starts_with("DEFINE_")) {
            desc.erase(0, 7);  // remove "DEFINE_"
        }
        if (op == AbstractOp::DECLARE_FORMAT || op == AbstractOp::DECLARE_ENUM ||
            op == AbstractOp::DECLARE_STATE || op == AbstractOp::DECLARE_PROPERTY ||
            op == AbstractOp::DECLARE_FUNCTION) {
            define_ref(inner_block, flags, op, "ref", code_ref(flags, "ref"), desc);
            define_range(inner_block, flags, op, "inner_range", code_ref(flags, "ref"), desc);
            define_ident(inner_block, flags, op, "ident", "ref", desc, true);
            if (op == AbstractOp::DECLARE_FUNCTION) {
                do_variable_definition(inner_block, flags, op, "func_type", "ctx.ref(ref).func_type().value()", "rebgn::FunctionType", "function type");
                write_return_type(inner_block, op, flags);
                do_typed_variable_definition(inner_block, flags, op, "belong_name", "std::nullopt", "std::optional<std::string>", "function belong name");
                inner_block.writeln("if(auto belong = ctx.ref(ref).belong();belong&&belong->value()!=0) {");
                {
                    auto inner_scope = inner_block.indent_scope();
                    inner_block.writeln("belong_name = ctx.ident(belong.value());");
                }
                inner_block.writeln("}");
                block_hook([&] {
                    auto& key = USE_FLAG(format_nested_function);
                    if (key == "define" || key == "declare") {
                        futils::helper::DynDefer indent2;
                        if (!USE_FLAG(compact_bit_field)) {
                            inner_block.writeln("if(func_type != rebgn::FunctionType::BIT_GETTER &&");
                            inner_block.writeln("   func_type != rebgn::FunctionType::BIT_SETTER) {");
                            indent2 = inner_block.indent_scope_ex();
                        }
                        if (key == "define") {
                            inner_block.writeln("inner_function(ctx, w, inner_range);");
                        }
                        else {
                            write_func_decl(inner_block, op, flags);
                            inner_block.writeln("w.writeln(\"", USE_FLAG(end_of_statement), "\");");
                        }
                        if (!USE_FLAG(compact_bit_field)) {
                            indent2.execute();
                            inner_block.writeln("}");
                        }
                    }
                });  // do nothing
            }
            else {
                block_hook([&] {
                    inner_block.writeln("inner_block(ctx, w, inner_range);");
                });
            }
        }
        else if (op == AbstractOp::DEFINE_PROPERTY ||
                 op == AbstractOp::END_PROPERTY) {
            block_hook([&] {});  // do nothing
        }
        else if (op == AbstractOp::DECLARE_BIT_FIELD) {
            define_ref(inner_block, flags, op, "ref", code_ref(flags, "ref"), "bit field");
            define_ident(inner_block, flags, op, "ident", "ref", "bit field", true);
            define_range(inner_block, flags, op, "inner_range", "ref", "bit field");
            define_ref(inner_block, flags, op, "type_ref", "ctx.ref(ref).type().value()", "bit field type");
            define_type(inner_block, flags, op, "type", "type_ref", "bit field type", true);
            block_hook([&] {
                inner_block.writeln("inner_block(ctx,w,inner_range);");
            });
        }
        else if (op == AbstractOp::DECLARE_UNION || op == AbstractOp::DECLARE_UNION_MEMBER) {
            define_ref(inner_block, flags, op, "ref", code_ref(flags, "ref"), desc);
            define_range(inner_block, flags, op, "inner_range", "ref", desc);
            block_hook([&] {
                if (USE_FLAG(format_nested_struct)) {
                    inner_block.writeln("inner_block(ctx, w, inner_range);");
                }
                else {
                    inner_block.writeln("TmpCodeWriter inner_w;");
                    inner_block.writeln("inner_block(ctx, inner_w, inner_range);");
                    inner_block.writeln("ctx.cw.write_unformatted(inner_w.out());");
                }
            });
        }
        else if (op == AbstractOp::DEFINE_FORMAT || op == AbstractOp::DEFINE_STATE || op == AbstractOp::DEFINE_UNION_MEMBER) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "format");
            define_bool(inner_block, flags, op, "is_empty_block", "range.start ==range.end - 2", "is empty block");
            if (op == AbstractOp::DEFINE_FORMAT) {
                inner_block.writeln("ctx.output.struct_names.push_back(ident);");
            }
            block_hook([&] {
                inner_block.writeln("w.writeln(\"", USE_FLAG(struct_keyword), " \", ident, \" ", USE_FLAG(block_begin), "\");");
                inner_block.writeln("defer.push_back(w.indent_scope_ex());");
            });
        }
        else if (op == AbstractOp::DEFINE_UNION) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "union");
            define_bool(inner_block, flags, op, "is_empty_block", "range.start ==range.end - 2", "is empty block");
            block_hook([&] {
                if (USE_FLAG(variant_mode) == "union") {
                    inner_block.writeln("w.writeln(\"", USE_FLAG(union_keyword), " \",ident, \" ", USE_FLAG(block_begin), "\");");
                    inner_block.writeln("defer.push_back(w.indent_scope_ex());");
                }
            });
        }
        else if (op == AbstractOp::END_UNION) {
            block_hook([&] {
                if (USE_FLAG(variant_mode) == "union") {
                    inner_block.writeln("defer.pop_back();");
                    inner_block.writeln("w.writeln(\"", USE_FLAG(block_end_type), "\");");
                }
            });
        }
        else if (op == AbstractOp::DEFINE_ENUM) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "enum");
            define_type_ref(inner_block, flags, op, "base_type_ref", code_ref(flags, "type"), "enum base type");
            do_typed_variable_definition(inner_block, flags, op, "base_type", "std::nullopt", "std::optional<std::string>", "enum base type");
            inner_block.writeln("if(base_type_ref.ref.value() != 0) {");
            auto scope = inner_block.indent_scope();
            inner_block.writeln("base_type = type_to_string(ctx,base_type_ref);");
            scope.execute();
            inner_block.writeln("}");
            if (USE_FLAG(default_enum_base).size()) {
                inner_block.writeln("else {");
                inner_block.indent_writeln("base_type = \"", USE_FLAG(default_enum_base), "\";");
                inner_block.writeln("}");
            }
            block_hook([&] {
                inner_block.writeln("w.write(\"", USE_FLAG(enum_keyword), " \", ident);");
                inner_block.writeln("if(base_type) {");
                inner_block.indent_writeln("w.write(\" ", USE_FLAG(enum_base_separator), " \", *base_type);");
                inner_block.writeln("}");
                inner_block.writeln("w.writeln(\" ", USE_FLAG(block_begin), "\");");
                inner_block.writeln("defer.push_back(w.indent_scope_ex());");
            });
        }
        else if (op == AbstractOp::DEFINE_ENUM_MEMBER) {
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "enum member");
            define_eval(inner_block, flags, op, "evaluated", code_ref(flags, "left_ref"), "enum member value");
            define_ident(inner_block, flags, op, "enum_ident", code_ref(flags, "belong"), "enum");
            block_hook([&] {
                inner_block.writeln("w.writeln(ident, \" = \", evaluated.result, \"", USE_FLAG(enum_member_end), "\");");
            });
        }
        else if (op == AbstractOp::DEFINE_BIT_FIELD) {
            define_type(inner_block, flags, op, "type", code_ref(flags, "type"), "bit field type", true);
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "bit field");
            define_ref(inner_block, flags, op, "belong", code_ref(flags, "belong"), "belonging struct or bit field");
            block_hook([&] {
                if (USE_FLAG(compact_bit_field)) {
                    if (auto& key = USE_FLAG(define_bit_field); key.size()) {
                        std::map<std::string, std::string> map{
                            {"IDENT", "\",ident,\""},
                            {"FULL_IDENT", "\",type_accessor(ctx.ref(belong),ctx),\""},
                            {"TYPE", "\",type,\""},
                        };
                        auto escaped = env_escape(flags, op, ENV_FLAG(define_bit_field), map);
                        inner_block.writeln("w.writeln(\"", escaped, USE_FLAG(field_end), "\");");
                    }
                    else if (USE_FLAG(prior_ident)) {
                        inner_block.writeln("w.writeln(ident, \" ", USE_FLAG(field_type_separator), "\", type, \"", USE_FLAG(field_end), "\");");
                    }
                    else {
                        inner_block.writeln("w.writeln(type, \" ", USE_FLAG(field_type_separator), "\", ident, \"", USE_FLAG(field_end), "\");");
                    }
                }
            });
        }
        else if (op == AbstractOp::END_BIT_FIELD) {
            block_hook([&] {});  // do nothing
        }
        else if (op == AbstractOp::DEFINE_FIELD) {
            inner_block.writeln("if (ctx.ref(code.belong().value()).op == rebgn::AbstractOp::DEFINE_PROGRAM) {");
            auto scope = inner_block.indent_scope();
            inner_block.writeln("break;");
            scope.execute();
            inner_block.writeln("}");
            define_type(inner_block, flags, op, "type", code_ref(flags, "type"), "field type", true);
            define_ident(inner_block, flags, op, "ident", code_ref(flags, "ident"), "field");
            define_ref(inner_block, flags, op, "belong", code_ref(flags, "belong"), "belonging struct or bit field");
            define_bool(inner_block, flags, op, "is_bit_field", "belong.value()!=0&&ctx.ref(belong).op==rebgn::AbstractOp::DEFINE_BIT_FIELD", "is part of bit field");
            block_hook([&] {
                futils::helper::DynDefer defer;
                if (USE_FLAG(compact_bit_field)) {
                    inner_block.writeln("if (!is_bit_field) {");
                    defer = inner_block.indent_scope_ex();
                }
                if (auto& key = USE_FLAG(define_field); key.size()) {
                    std::map<std::string, std::string> map{
                        {"IDENT", "\",ident,\""},
                        {"FULL_IDENT", "\",type_accessor(ctx.ref(belong),ctx),\""},
                        {"TYPE", "\",type,\""},
                    };
                    auto escaped = env_escape(flags, op, ENV_FLAG(define_field), map);
                    inner_block.writeln("w.writeln(\"", escaped, USE_FLAG(field_end), "\");");
                }
                else if (USE_FLAG(prior_ident)) {
                    inner_block.writeln("w.writeln(ident, \" ", USE_FLAG(field_type_separator), "\", type, \"", USE_FLAG(field_end), "\");");
                }
                else {
                    inner_block.writeln("w.writeln(type, \" ", USE_FLAG(field_type_separator), "\", ident, \"", USE_FLAG(field_end), "\");");
                }

                if (USE_FLAG(compact_bit_field)) {
                    defer.execute();
                    inner_block.writeln("}");
                    inner_block.writeln("else {");
                    auto scope = inner_block.indent_scope();
                    inner_block.writeln("w.writeln(\"", flags.wrap_comment("\",ident,\""), "\");");
                    scope.execute();
                    inner_block.writeln("}");
                }
            });
        }
        else if (op == AbstractOp::END_FORMAT || op == AbstractOp::END_ENUM || op == AbstractOp::END_STATE ||
                 op == AbstractOp::END_UNION_MEMBER) {
            block_hook([&] {
                inner_block.writeln("defer.pop_back();");
                inner_block.writeln("w.writeln(\"", USE_FLAG(block_end_type), "\");");
            });
        }
        else if (op == AbstractOp::DEFINE_PROPERTY_GETTER || op == AbstractOp::DEFINE_PROPERTY_SETTER) {
            define_ref(inner_block, flags, op, "func", code_ref(flags, "right_ref"), "function");
            define_range(inner_block, flags, op, "inner_range", "func", "function");
            define_ident(inner_block, flags, op, "ident", "ctx.ref(func).ident().value()", "function");
            write_return_type(inner_block, op, flags);
            do_typed_variable_definition(inner_block, flags, op, "belong_name", "std::nullopt", "std::optional<std::string>", "function belong name");
            inner_block.writeln("if(auto belong = ctx.ref(func).belong();belong&&belong->value()!=0) {");
            auto scope = inner_block.indent_scope();
            inner_block.writeln("belong_name = ctx.ident(belong.value());");
            scope.execute();
            inner_block.writeln("}");
            block_hook([&] {
                auto& key = USE_FLAG(format_nested_function);
                if (key == "define") {
                    inner_block.writeln("inner_function(ctx, w, inner_range);");
                }
                else if (key == "declare") {
                    write_func_decl(inner_block, op, flags);
                    inner_block.writeln("w.writeln(\"", USE_FLAG(end_of_statement), "\");");
                }
            });
        }
        else {
            block_hook([&] {
                inner_block.writeln("w.writeln(\"", flags.wrap_comment("Unimplemented " + std::string(to_string(op))), " \");");
            });
        }
        block_hook([&] {}, bm2::HookFileSub::after);
        inner_block.writeln("break;");
        scope.execute();
        inner_block.writeln("}");
    }

    void write_inner_block_func(bm2::TmpCodeWriter& w, bm2::TmpCodeWriter& inner_block, Flags& flags) {
        w.writeln("void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range);");
        inner_block.writeln("void inner_block(Context& ctx, TmpCodeWriter& w, rebgn::Range range) {");
        auto scope_block = inner_block.indent_scope();
        inner_block.writeln("std::vector<futils::helper::DynDefer> defer;");
        may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_start, false);
        inner_block.writeln("for(size_t i = range.start; i < range.end; i++) {");
        auto scope_nest_block = inner_block.indent_scope();
        inner_block.writeln("auto& code = ctx.bm.code[i];");
        may_write_from_hook(inner_block, flags, bm2::HookFile::inner_block_each_code, false);
        inner_block.writeln("switch(code.op) {");

        for (size_t i = 0; to_string(AbstractOp(i))[0] != 0; i++) {
            auto op = AbstractOp(i);
            if (rebgn::is_marker(op)) {
                continue;
            }
            if (is_struct_define_related(op)) {
                write_inner_block(inner_block, op, flags);
            }
        }

        inner_block.writeln("default: {");
        auto if_block = inner_block.indent_scope();
        inner_block.writeln("if (!rebgn::is_marker(code.op)&&!rebgn::is_expr(code.op)&&!rebgn::is_parameter_related(code.op)) {");
        inner_block.indent_writeln("w.writeln(", unimplemented_comment(flags, "code.op"), ");");
        inner_block.writeln("}");
        inner_block.writeln("break;");
        if_block.execute();
        inner_block.writeln("}");  // close default
        inner_block.writeln("}");  // close switch
        scope_nest_block.execute();
        inner_block.writeln("}");  // close for
        scope_block.execute();
        inner_block.writeln("}");  // close function
    }

}  // namespace rebgn
