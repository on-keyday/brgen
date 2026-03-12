/*license*/
#pragma once
#include "flags.hpp"
#include "../context.hpp"
#include "hook_load.hpp"

namespace rebgn {
    void may_add_doc_content(Flags& flags, auto op, std::string_view var_name, std::string_view type, std::string_view description, std::string_view init_expr) {
        if (flags.mode == bm2::GenerateMode::docs_json || flags.mode == bm2::GenerateMode::docs_markdown) {
            flags.content[flags.func_name][to_string(op)].variables.push_back(VariableDesc{std::string(var_name), std::string(type), std::string(description), std::string(init_expr)});
        }
    }

    void do_variable_definition_internal(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view init_expr, std::string_view type, std::string_view desc_type, std::string_view description) {                
        may_write_from_hook(w,flags,flags.func_hook,op,var_name,bm2::HookFileSub::before);
        if(!may_write_from_hook(w,flags,flags.func_hook,op,var_name,bm2::HookFileSub::main)) {
            w.writeln(type, " ", var_name, " = ", init_expr, "; //", description);
        }
        may_write_from_hook(w,flags,flags.func_hook,op,var_name,bm2::HookFileSub::after);
        may_add_doc_content(flags, op, var_name, desc_type, description, init_expr);
    }

    void do_typed_variable_definition(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view init_expr, std::string_view type, std::string_view description) {
        do_variable_definition_internal(w, flags, op, var_name, init_expr, type, type, description);
    }

    void do_variable_definition(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view init_expr, std::string_view type, std::string_view description) {
        do_variable_definition_internal(w, flags, op, var_name, init_expr, "auto", type, description);
    }

    inline std::string code_ref(Flags& flags, std::string_view ref_name, std::string_view base = "code") {
        return std::format("{}.{}().value()", base, ref_name);
    }

    inline std::string ctx_ref(Flags& flags, std::string_view ref_name) {
        return std::format("ctx.ref({})", ref_name);
    }

    void define_ref(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description) {
        do_variable_definition(w, flags, op, var_name, ref, "Varint", std::format("reference of {}", description));
    }

    void define_type_ref(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description) {
        do_variable_definition(w, flags, op, var_name, ref, "StorageRef", std::format("type reference of {}", description));
    }

    void define_range(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description) {
        auto range_var = std::format("ctx.range({})", ref);
        do_variable_definition(w, flags, op, var_name, range_var, "string", std::format("range of {}", description));
    }

    void define_uint(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description) {
        do_variable_definition(w, flags, op, var_name, ref, "uint64_t", description);
    }

    void define_bool(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description) {
        do_variable_definition(w, flags, op, var_name, ref, "bool", description);
    }

    void define_ident(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description, bool direct_ref = false) {
        if (direct_ref) {
            auto ref_var = std::format("ctx.ident({})", ref);
            do_variable_definition(w, flags, op, var_name, ref_var, "string", std::format("identifier of {}", description));
        }
        else {
            auto ref_name = std::format("{}_ref", var_name);
            define_ref(w, flags, op, ref_name, ref, description);
            auto ref_var = std::format("ctx.ident({})", ref_name);
            do_variable_definition(w, flags, op, var_name, ref_var, "string", std::format("identifier of {}", description));
        }
    }

    void define_eval(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description, bool direct_ref = false) {
        if (direct_ref) {
            auto ref_var = std::format("eval(ctx.ref({}), ctx)", ref);
            do_variable_definition(w, flags, op, var_name, ref_var, "EvalResult", description);
        }
        else {
            auto ref_name = std::format("{}_ref", var_name);
            define_ref(w, flags, op, ref_name, ref, description);
            auto ref_var = std::format("eval(ctx.ref({}), ctx)", ref_name);
            do_variable_definition(w, flags, op, var_name, ref_var, "EvalResult", description);
        }
    }

    void define_type(bm2::TmpCodeWriter& w, Flags& flags, auto op, std::string_view var_name, std::string_view ref, std::string_view description, bool direct_ref = false) {
        if (direct_ref) {
            auto ref_var = std::format("type_to_string(ctx,{})", ref);
            do_variable_definition(w, flags, op, var_name, ref_var, "string", description);
        }
        else {
            auto ref_name = std::format("{}_ref", var_name);
            define_ref(w, flags, op, ref_name, ref, description);
            auto ref_var = std::format("type_to_string(ctx,{})", ref_name);
            do_variable_definition(w, flags, op, var_name, ref_var, "string", description);
        }
    }
}  // namespace rebgn