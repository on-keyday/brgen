/*license*/
#include "stub/class_based.hpp"
#include "ebmgen/common.hpp"
#include "error/error.h"
#include "helper/defer_ex.h"
#include "stub/structs.hpp"
#include <cstddef>
#include <format>
#include <string>
#include "stub/hooks.hpp"

namespace ebmcodegen {
    constexpr auto macro_CODEGEN_COMMON_INCLUDE_GUARD = "EBM_CODEGEN_COMMON_INCLUDE_GUARD";
    constexpr auto macro_CODEGEN_NAMESPACE = "CODEGEN_NAMESPACE";
    constexpr auto macro_CODEGEN_VISITOR = "CODEGEN_VISITOR";
    constexpr auto macro_CODEGEN_CONTEXT_PARAMETERS = "CODEGEN_CONTEXT_PARAMETERS";
    constexpr auto macro_CODEGEN_CONTEXT = "CODEGEN_CONTEXT";
    constexpr auto macro_CODEGEN_EXPECTED_PRIORITY = "CODEGEN_EXPECTED_PRIORITY";
    constexpr auto macro_CODEGEN_MAY_HIJACK = "CODEGEN_MAY_HIJACK";
    constexpr auto legacy_compat_ptr_name = "__legacy_compat_ptr";
    constexpr auto template_param_result(bool is_decl) {
        if (is_decl) {
            return "typename Result = Result";
        }
        return "typename Result";
    }

    struct ContextClassField {
        std::string_view name;
        std::string type;
        const StructField* base = nullptr;
    };

    enum ContextClassKind {
        ContextClassKind_Normal = 0,
        ContextClassKind_Before = 1,
        ContextClassKind_After = 2,
        ContextClassKind_VariantMask = 3,

        ContextClassKind_List = 4,
        ContextClassKind_Generic = 8,   // like Statement, Expression, Type
        ContextClassKind_Special = 16,  // like entry
        ContextClassKind_Observe = 32,  // like pre_visitor, post_visitor
    };

    std::string upper(std::string_view s) {
        std::string result;
        for (auto c : s) {
            result += std::toupper(c);
        }
        return result;
    }

    std::string lower(std::string_view s) {
        std::string result;
        for (auto c : s) {
            result += std::tolower(c);
        }
        return result;
    }

    struct TypeParam {
        std::string_view name;
        std::string constraint;
    };

    struct ContextClass {
        std::string_view base;     // like Statement, Expression, Type
        std::string_view variant;  // like BLOCK, ASSIGNMENT, etc
        ContextClassKind kind = ContextClassKind_Normal;
        std::vector<TypeParam> type_params;
        std::vector<ContextClassField> fields;

        std::string body_name() const {
            return std::string(base) + "Body";
        }

        std::string ref_name() const {
            return std::string(base) + "Ref";
        }

        bool has(ContextClassKind k) const {
            return (kind & k) != 0;
        }

        ContextClassKind is_before_after() const {
            return ContextClassKind(kind & ContextClassKind_VariantMask);
        }

        /*
         std::string type_parameters_body(bool include_constraints) const {
            std::string result;
            for (size_t i = 0; i < type_params.size(); i++) {
                if (i != 0) {
                    result += ",";
                }
                if (type_params[i].constraint.size() && include_constraints) {
                    result += std::format("{} {}", type_params[i].constraint, type_params[i].name);
                }
                else {
                    result += std::format("typename {}", type_params[i].name);
                }
            }
            return result;
        }

        std::string type_parameters(bool include_constraints) const {
            if (type_params.size() == 0) {
                return "";
            }
            std::string result = "template <";
            result += type_parameters_body(include_constraints);
            result += ">";
            return result;
        }
        std::string type_param_arguments() const {
            std::string result;
            for (size_t i = 0; i < type_params.size(); i++) {
                if (i != 0) {
                    result += ",";
                }
                result += std::string(type_params[i].name);
            }
            return result;
        }


        std::string class_name_with_type_params() const {
            std::string name = class_name();
            if (type_params.size()) {
                name += "<";
                name += type_param_arguments();
                name += ">";
            }
            return name;
        }
        */

        std::string class_name() const {
            std::string name;
            if (kind & ContextClassKind_List) {
                if (base == "Statement") {
                    name = "Block";
                }
                else {
                    name = std::string(base) + "s";
                }
            }
            else {
                name = std::string(base);
            }
            if (variant.size()) {
                name += "_";
                name += variant;
            }
            if (kind & ContextClassKind_Before) {
                name += suffixes[suffix_before];
            }
            else if (kind & ContextClassKind_After) {
                name += suffixes[suffix_after];
            }
            return name;
        }

        std::string upper_class_name() const {
            return upper(class_name());
        }
    };

    struct ContextClasses {
        ContextClass classes[3];  // main, before, after
        ContextClass& main() {
            return classes[0];
        }

        ContextClass& before() {
            return classes[1];
        }

        ContextClass& after() {
            return classes[2];
        }

        const ContextClass& main() const {
            return classes[0];
        }

        const ContextClass& before() const {
            return classes[1];
        }

        const ContextClass& after() const {
            return classes[2];
        }
    };

    std::string visitor_tag_name(const ContextClass& cls) {
        return "VisitorTag_" + cls.class_name();
    }

    std::string visitor_requirements_name(const ContextClass& cls) {
        return "has_visitor_" + cls.class_name();
    }

    std::string context_requirements_name(const ContextClass& cls) {
        return "has_context_" + cls.class_name();
    }

    struct HookType {
        std::string_view name;
        bool is_config_include_target = false;

        std::string visitor_instance_name(const ContextClass& cls, std::string_view ns_name = "") const {
            auto may_with_ns = [&](std::string_view n) {
                if (ns_name.size()) {
                    return std::string(ns_name) + "::" + std::string(n);
                }
                return std::string(n);
            };
            auto visitor_tag = visitor_tag_name(cls);
            auto ns_visitor_tag = may_with_ns(visitor_tag);
            auto hook_type_template = may_with_ns(std::format("{}<{}>", name, ns_visitor_tag));
            return may_with_ns(std::format("Visitor<{}>", hook_type_template));
        }

        std::string visitor_instance_holder_name(const ContextClass& cls) const {
            return std::format("visitor_{}_{}", cls.class_name(), name);
        }
    };

    std::vector<ContextClasses> generate_context_classes(const std::map<std::string_view, Struct>& struct_map) {
        std::vector<ContextClasses> context_classes;
        std::string result_type = "expected<Result>";
        auto add_common_visitor = [&](ContextClass& context_class) {
            // context_class.type_params.push_back({"Visitor", std::format("{}::BaseVisitorLike", ns_name)});
            context_class.fields.push_back(ContextClassField{.name = "visitor", .type = "BaseVisitor&"});
        };
        auto add_context_variant_for_before_after = [&](ContextClasses& result) {
            auto& base = result.main();
            auto for_before = base;
            for_before.kind = ContextClassKind(base.kind | ContextClassKind_Before | ContextClassKind_Observe);
            for_before.fields.push_back(ContextClassField{.name = "main_logic", .type = "ebmcodegen::util::MainLogicWrapper<Result>"});
            for_before.type_params.push_back({"Result", ""});
            result.before() = std::move(for_before);
            auto for_after = base;
            for_after.kind = ContextClassKind(base.kind | ContextClassKind_After | ContextClassKind_Observe);
            for_after.fields.push_back(ContextClassField{.name = "main_logic", .type = "ebmcodegen::util::MainLogicWrapper<Result>"});
            for_after.fields.push_back(ContextClassField{.name = "result", .type = result_type + "&"});
            for_after.type_params.push_back({"Result", ""});
            result.after() = std::move(for_after);
        };

        ContextClasses entry;
        entry.main().base = "entry";
        entry.main().kind = ContextClassKind_Special;
        add_common_visitor(entry.main());
        add_context_variant_for_before_after(entry);
        context_classes.push_back(std::move(entry));
        ContextClasses pre_visitor;
        pre_visitor.main().base = "pre_visitor";
        pre_visitor.main().kind = ContextClassKind(ContextClassKind_Special | ContextClassKind_Observe);
        add_common_visitor(pre_visitor.main());
        pre_visitor.main().fields.push_back(ContextClassField{.name = "ebm", .type = "ebm::ExtendedBinaryModule&"});
        add_context_variant_for_before_after(pre_visitor);
        context_classes.push_back(std::move(pre_visitor));
        ContextClasses post_entry;
        post_entry.main().base = "post_entry";  // name is for backward compatibility
        post_entry.main().kind = ContextClassKind(ContextClassKind_Special | ContextClassKind_Observe);
        add_common_visitor(post_entry.main());
        post_entry.main().fields.push_back(ContextClassField{.name = "entry_result", .type = result_type + "&"});
        add_context_variant_for_before_after(post_entry);
        // add type param Result
        post_entry.main().type_params.push_back({"Result", ""});
        context_classes.push_back(std::move(post_entry));

        auto convert = [&](auto t, std::string_view kind, auto subset) {
            using T = std::decay_t<decltype(t)>;
            for (size_t i = 0; to_string(T(i))[0]; i++) {
                ContextClasses per_variant;
                per_variant.main().base = kind;
                per_variant.main().variant = to_string(T(i));
                add_common_visitor(per_variant.main());
                per_variant.main().fields.push_back(ContextClassField{.name = "item_id", .type = "ebm::" + std::string(kind) + "Ref"});
                auto& body = struct_map.at(std::string(kind) + "Body");
                for (auto& field : body.fields) {
                    if (!subset[T(i)].first.contains(field.name)) {
                        continue;
                    }
                    std::string typ;
                    if (field.type.contains("std::") || field.type == "bool") {
                        typ = field.type;
                    }
                    else {
                        typ = std::format("ebm::{}", field.type);
                    }
                    if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                        typ = std::format("std::vector<{}>", typ);
                    }
                    typ = "const " + typ + "&";
                    per_variant.main().fields.push_back(ContextClassField{.name = field.name, .type = typ, .base = &field});
                }
                add_context_variant_for_before_after(per_variant);
                context_classes.push_back(std::move(per_variant));
            }
            ContextClasses per_kind;
            per_kind.main().base = kind;
            per_kind.main().kind = ContextClassKind_Generic;
            add_common_visitor(per_kind.main());
            per_kind.main().fields.push_back(ContextClassField{.name = "in", .type = "const ebm::" + std::string(kind) + "&"});
            per_kind.main().fields.push_back(ContextClassField{.name = "alias_ref", .type = "ebm::" + std::string(kind) + "Ref"});
            add_context_variant_for_before_after(per_kind);
            context_classes.push_back(std::move(per_kind));
            ContextClasses per_list;
            per_list.main().base = kind;
            per_list.main().kind = ContextClassKind_List;
            add_common_visitor(per_list.main());
            per_list.main().fields.push_back(ContextClassField{.name = "in", .type = "const ebm::" + per_list.main().class_name() + "&"});
            add_context_variant_for_before_after(per_list);
            context_classes.push_back(std::move(per_list));
        };
        convert(ebm::StatementKind{}, prefixes[prefix_statement], body_subset_StatementBody());
        convert(ebm::ExpressionKind{}, prefixes[prefix_expression], body_subset_ExpressionBody());
        convert(ebm::TypeKind{}, prefixes[prefix_type], body_subset_TypeBody());
        return context_classes;
    }

    struct UserConfigIncludes {
        std::string_view visitor = prefixes[prefix_visitor];
        std::string_view post_includes = prefixes[prefix_post_includes];
        std::string_view includes = prefixes[prefix_includes];
        std::string_view flags = prefixes[prefix_flags];
        std::string flags_bind = std::format("{}{}", prefixes[prefix_flags], suffixes[suffix_bind]);
        std::string flags_struct = std::format("{}{}", prefixes[prefix_flags], suffixes[suffix_struct]);
        std::string_view output = prefixes[prefix_output];
        std::string_view result = prefixes[prefix_result];

        void foreach (auto&& cb) {
            cb(visitor);
            cb(post_includes);
            cb(includes);
            cb(flags);
            cb(flags_bind);
            cb(flags_struct);
            cb(output);
            cb(result);
        }
    };

    struct IncludeInfo {
        const IncludeLocations& includes;
        const std::vector<HookType>& hooks;
        UserConfigIncludes user_include;
    };

    void user_config_include_per_base(CodeWriter& w, std::string_view base_name, const IncludeInfo& includes_info) {
        std::vector<std::string> candidates;
        const auto& includes = includes_info.includes;
        const auto& hooks = includes_info.hooks;
        for (size_t i = 0; i < includes.include_locations.size(); i++) {
            if (!hooks[i].is_config_include_target) {
                continue;
            }
            auto& loc = includes.include_locations[i];
            auto file_name = std::format("{}{}{}.hpp", loc.location, base_name, loc.suffix);
            auto include_loc = std::format("\"{}\"", file_name);
            candidates.push_back(include_loc);
        }
        if (candidates.size() == 0) {
            return;
        }
        bool first = true;
        for (auto& include_loc : candidates) {
            if (first) {
                w.writeln("#if __has_include(", include_loc, ")");
                first = false;
            }
            else {
                w.writeln("#elif __has_include(", include_loc, ")");
            }
            w.writeln("#include ", include_loc);
        }
        w.writeln("#endif");
    }

    void user_config_include(CodeWriter& w, std::string_view base_name, const IncludeInfo& includes_info) {
        user_config_include_per_base(w, std::format("{}{}", base_name, suffixes[suffix_before]), includes_info);
        user_config_include_per_base(w, base_name, includes_info);
        user_config_include_per_base(w, std::format("{}{}", base_name, suffixes[suffix_after]), includes_info);
    }

    std::vector<std::string_view> make_args_with_injected(const ContextClass& cls, std::vector<std::string_view> injected_args) {
        std::vector<std::string_view> args;
        size_t injected_index = 0;
        for (auto& field : cls.fields) {
            if (!field.base) {
                assert(injected_index < injected_args.size());
                args.push_back(injected_args[injected_index++]);
            }
            else {
                args.push_back(field.name);
            }
        }
        assert(injected_index == injected_args.size());
        return args;
    }

    std::string context_name(const ContextClass& cls) {
        return "Context_" + cls.class_name();
    }

    std::string context_instance_name(const ContextClass& cls) {
        // return std::format("{}<{}>", context_name(cls), cls.type_param_arguments());
        auto name = context_name(cls);
        if (cls.type_params.size()) {
            name += "<";
            for (size_t i = 0; i < cls.type_params.size(); i++) {
                if (i != 0) {
                    name += ", ";
                }
                name += cls.type_params[i].name;
            }
            name += ">";
        }
        return name;
    }

    void construct_instance(CodeWriter& w, const ContextClass& cls, std::string_view instance_name /*, std::vector<std::string_view> type_params*/, std::vector<std::string_view> args) {
        // assert(type_params.size() == cls.type_params.size());
        assert(args.size() == cls.fields.size());
        w.write(context_instance_name(cls));
        w.writeln(" ", instance_name, "{");
        {
            auto scope = w.indent_scope();
            for (size_t i = 0; i < cls.fields.size(); i++) {
                w.writeln(".", cls.fields[i].name, " = ", args[i], ",");
            }
        }
        w.writeln("};");
    }

    void deconstruct_base_struct(CodeWriter& w, const ContextClass& cls, std::string_view instance_name) {
        for (auto& field : cls.fields) {
            if (!field.base) {
                continue;
            }
            if (field.base->attr & ebmcodegen::TypeAttribute::PTR) {
                w.writeln("if (!in.body.", field.name, "()) {");
                w.indent_writeln("return unexpect_error(\"Unexpected null pointer for ", cls.body_name(), "::", field.name, "\");");
                w.writeln("}");
                w.writeln("auto& ", field.name, " = *in.body.", field.name, "();");
            }
            else {
                w.writeln("auto& ", field.name, " = in.body.", field.name, ";");
            }
        }
    }

    std::string deconstruct_macro_name(std::string_view ns_name, const ContextClass& cls) {
        return upper(ns_name) + "_DECONSTRUCT_" + cls.upper_class_name();
    }

    void deconstruct_context_fields_macro(CodeWriter& w, std::string_view ns_name, const ContextClass& cls) {
        w.writeln("// Deconstruct context fields");
        w.writeln("#define ", deconstruct_macro_name(ns_name, cls), "(instance_name) \\");
        for (auto& field : cls.fields) {
            w.write("auto& ", field.name, " = instance_name.", field.name, ";");
        }
        w.writeln();
    }

    void generate_hijack_logic_macro(CodeWriter& w) {
        auto may_inject = "value";
        w.writeln("#define ", macro_CODEGEN_MAY_HIJACK, "(value) \\");
        w.writeln("if (ebmcodegen::util::needs_return(value)) { \\");
        {
            auto if_scope = w.indent_scope();
            w.writeln("return ebmcodegen::util::wrap_return(std::move(value)); \\");
        }
        w.writeln("}");
    }

    void handle_hijack_logic(CodeWriter& w, std::string_view may_inject) {
        w.writeln(macro_CODEGEN_MAY_HIJACK, "(", may_inject, ");");
        /*
        w.writeln("if (!", may_inject, ") {");
        {
            auto if_scope = w.indent_scope();
            w.writeln("if(!ebmcodegen::util::is_pass_error(", may_inject, ".error())) {");
            {
                auto if_scope2 = w.indent_scope();
                w.writeln("return ebmgen::unexpect_error(std::move(", may_inject, ".error())); // for trace");
            }
            w.writeln("}");
        }
        w.writeln("}");
        w.writeln("else { // if hijacked");
        {
            auto else_scope = w.indent_scope();
            w.writeln("return ", may_inject, ";");
        }
        w.writeln("}");
        */
    }

    void generate_visitor_requirements(CodeWriter& w, const std::string_view result_type) {
        // w.writeln("template<typename VisitorImpl,", cls.type_parameters_body(false), ">");
        w.writeln("template<", template_param_result(false), ",typename VisitorImpl,typename Context>");
        w.writeln("concept HasVisitor = !std::is_base_of_v<ContextBase<std::decay_t<VisitorImpl>>,std::decay_t<VisitorImpl>> && requires(VisitorImpl v,Context c) {");
        {
            auto requires_scope = w.indent_scope();
            w.writeln(" { v.visit(c) } -> std::convertible_to<", result_type, ">;");
        }
        w.writeln("};");
    }

    std::string dispatch_fn_name(const ContextClass& cls) {
        return "dispatch_" + cls.class_name();
    }

    void generate_dispatch_fn_signature(CodeWriter& w, const ContextClass& cls, const std::string_view result_type, bool is_decl) {
        auto fn_name = dispatch_fn_name(cls);
        if (cls.has(ContextClassKind_Special)) {
            w.writeln("template<", template_param_result(is_decl), ",typename Context>");
            w.write(result_type, " ", fn_name, "(Context&& ctx");
            if (cls.base == "pre_visitor") {  // special case: pre_visitor has ebm ref
                w.write(",ebm::ExtendedBinaryModule& ebm");
            }
            if (cls.base == "post_entry") {  // special case: post_entry has result ref
                w.write(",", result_type, "& entry_result");
            }
            w.write(")");
            if (is_decl) {
                w.writeln(";");
            }
        }
        else if (cls.has(ContextClassKind_List)) {
            w.writeln("template<", template_param_result(is_decl), ",typename Context>");
            w.write(result_type, " ", fn_name, "(Context&& ctx,const ebm::", cls.class_name(), "& in)");
            if (is_decl) {
                w.writeln(";");
            }
        }
        else {
            w.writeln("template<", template_param_result(is_decl), ",typename Context>");
            w.write(result_type, " ", fn_name, "(Context&& ctx,const ebm::", cls.base, "& in,ebm::", cls.ref_name(), " alias_ref");
            if (is_decl) {
                w.writeln(" = {});");
            }
            else {
                w.write(")");
            }
        }
    }

    constexpr auto get_visitor_arg_fn = "get_visitor_arg_from_context";
    constexpr auto get_visitor_fn = "get_visitor_from_context";

    void generate_dispatcher_function(CodeWriter& hdr, CodeWriter& src, const ContextClasses& clss, const std::string_view result_type) {
        auto& cls = clss.main();
        generate_dispatch_fn_signature(hdr, cls, result_type, true);
        const auto visitor_arg = std::format("{}(ctx)", get_visitor_arg_fn);
        const auto get_visitor = [&](auto tctx) { return std::format("{}<Result>(ctx,{})", get_visitor_fn, tctx); };
        auto id_arg = "is_nil(alias_ref) ? in.id : alias_ref";
        auto make_args = [&](const ContextClass& cls, std::vector<std::string_view> additional_args) {
            std::vector<std::string_view> args;
            if (cls.has(ContextClassKind_Special)) {
                args = {visitor_arg};
                if (cls.base == "post_entry") {
                    args.push_back("entry_result");
                }
                if (cls.base == "pre_visitor") {
                    args.push_back("ebm");
                }
            }
            else if (cls.has(ContextClassKind_Generic)) {
                args = {visitor_arg, "in", "alias_ref"};
            }
            else if (cls.has(ContextClassKind_List)) {
                args = {visitor_arg, "in"};
            }
            else {
                args = {visitor_arg, id_arg};
            }
            args.insert(args.end(), additional_args.begin(), additional_args.end());
            return make_args_with_injected(cls, args);
        };
        {
            generate_dispatch_fn_signature(src, cls, result_type, false);
            src.writeln("{");
            {
                auto scope = src.indent_scope();
                deconstruct_base_struct(src, cls, "in");
                src.writeln("auto main_logic = [&]() -> ", result_type, "{");
                {
                    auto main_scope = src.indent_scope();
                    auto args = make_args(cls, {});
                    construct_instance(src, clss.main(), "new_ctx", args);
                    src.writeln("return ", get_visitor("new_ctx"), ".visit(new_ctx);");
                }
                src.writeln("};");
                auto before_args = make_args(clss.before(), {"main_logic"});
                construct_instance(src, clss.before(), "before_ctx", before_args);
                src.writeln(result_type, " before_result = ", get_visitor("before_ctx"), ".visit(before_ctx);");
                handle_hijack_logic(src, "before_result");
                src.writeln(result_type, " main_result = main_logic();");
                auto after_args = make_args(clss.after(), {"main_logic", "main_result"});
                construct_instance(src, clss.after(), "after_ctx", after_args);
                src.writeln(result_type, " after_result = ", get_visitor("after_ctx"), ".visit(after_ctx);");
                handle_hijack_logic(src, "after_result");
                src.writeln("return main_result;");
            }
            src.writeln("}");
        }
    }

    void generate_context_class(CodeWriter& w, const ContextClass& cls) {
        // w.writeln(cls.type_parameters(true));
        if (cls.type_params.size()) {
            w.write("template <");
            for (size_t i = 0; i < cls.type_params.size(); i++) {
                if (i != 0) {
                    w.write(", ");
                }
                if (cls.type_params[i].constraint.size()) {
                    w.write(cls.type_params[i].constraint, " ");
                }
                else {
                    w.write("typename ");
                }
                w.write(cls.type_params[i].name);
            }
            w.writeln(">");
        }
        w.writeln("struct ", context_name(cls), " : ebmcodegen::util::ContextBase<", context_instance_name(cls), "> {");
        {
            auto scope = w.indent_scope();
            w.writeln("constexpr static std::string_view context_name = \"", cls.class_name(), "\";");
            for (auto& field : cls.fields) {
                w.writeln(field.type, " ", field.name, ";");
            }
        }
        w.writeln("};");
    }

    void generate_visitor_tag(CodeWriter& w, const ContextClass& cls) {
        w.writeln("struct ", visitor_tag_name(cls), " {};");
    }

    void generate_hook_tag(CodeWriter& w, const HookType& hook) {
        w.writeln("template <typename Tag>");
        w.writeln("struct ", hook.name, " {}; // Hook tag");
    }

    void generate_visitor_customization_point(CodeWriter& w) {
        w.writeln("template<typename Tag>");
        w.writeln("struct Visitor; // Customization point struct");
    }

    std::string priority_check_macro_name(const ContextClass& cls) {
        return std::format("{}_{}", macro_CODEGEN_EXPECTED_PRIORITY, cls.upper_class_name());
    }

    std::string generate_generator_priority_check(CodeWriter& w, const ContextClass& cls, size_t selected_index) {
        auto selected = std::format("{}", selected_index);
        auto expected_index = priority_check_macro_name(cls);
        return std::format("{} == {}", expected_index, selected);
    }

    void generate_visit_if_exists(CodeWriter& w, std::string_view self, const HookType& hook, const ContextClass& cls, size_t index) {
        auto instance_name = hook.visitor_instance_holder_name(cls);
        if (!self.empty()) {
            instance_name = std::format("{}.{}", self, instance_name);
        }
        auto expect = generate_generator_priority_check(w, cls, index);
        w.writeln("if constexpr (", expect, ") {");
        {
            auto if_scope = w.indent_scope();
            // w.writeln("static_assert(", visitor_requirements, "<decltype(", instance_name, "),", cls.type_param_arguments(), ">);");
            // w.writeln("static_assert(", visitor_requirements, "<decltype(", instance_name, ")>);");
            w.writeln("return ", instance_name, ";");
        }
        w.writeln("}");
    }

    std::string get_hook_fn_name(const ContextClass& cls) {
        return "get_visitor_" + cls.class_name();
    }

    void generate_always_false_template(CodeWriter& w) {
        w.writeln("template<typename T>");
        w.writeln("constexpr bool dependent_false = false;");
    }

    void generate_get_hook(CodeWriter& w, const std::vector<HookType>& hooks, const ContextClass& cls) {
        auto fn_name = get_hook_fn_name(cls);
        // w.writeln(cls.type_parameters(true));
        auto arg_type = context_instance_name(cls);
        w.writeln("auto& ", fn_name, "(const ", arg_type, "&) {");
        {
            auto scope = w.indent_scope();
            bool first = true;
            size_t index = 0;
            for (auto& hook : hooks) {
                if (!first) {
                    w.write("else ");
                }
                first = false;
                generate_visit_if_exists(w, "", hook, cls, index);
                ++index;
            }
            w.writeln("else {");
            {
                auto else_scope = w.indent_scope();
                auto priority_macro = priority_check_macro_name(cls);
                w.writeln("static_assert(", priority_macro, ">= 0 && ", priority_macro, " < ", std::to_string(index), ", \"No suitable visitor hook found for ", cls.class_name(), "\");");
            }
            w.writeln("}");
        }
        w.writeln("}");
    }

    void generate_visitor_implementation(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        // w.writeln(cls.type_parameters(true));
        w.writeln(result_type, " visit(", context_instance_name(cls), "& ctx) {");
        {
            auto scope = w.indent_scope();
            w.writeln("auto& visitor = impl.", get_hook_fn_name(cls), "(ctx);");
            w.writeln("return visitor.visit(ctx);");
        }
        w.writeln("}");
    }

    void generate_unimplemented_stub_decl(CodeWriter& src, std::string_view result_type, bool is_codegen) {
        src.writeln(result_type, " visit_unimplemented(MergedVisitor& visitor,std::string_view kind,std::uint64_t item_id);");
    }

    void generate_unimplemented_stub_def(CodeWriter& src, std::string_view result_type, bool is_codegen) {
        src.writeln(result_type, " visit_unimplemented(MergedVisitor& visitor,std::string_view kind,std::uint64_t item_id) {");
        {
            auto scope = src.indent_scope();
            src.writeln("if (visitor.flags.debug_unimplemented) {");
            auto if_scope = src.indent_scope();

            if (is_codegen) {
                src.writeln("return std::format(\"{{{{Unimplemented {} {}}}}}\", kind, item_id);");
            }
            else {
                src.writeln("return unexpect_error(\"Unimplemented {} {}\", kind, item_id);");
            }
            src.writeln("}");
            src.writeln("return ", result_type, "{}; // Unimplemented");
        }
        src.writeln("}");
    }

    void generate_forward_declaration_source(CodeWriter& w, std::string_view ns_name, const std::string_view result_type, bool is_codegen) {
        w.writeln("namespace ", ns_name, " {");
        {
            auto scope = w.indent_scope();

            generate_unimplemented_stub_decl(w, result_type, is_codegen);
        }
        w.writeln("}");
    }

    void generate_forward_declaration_header(CodeWriter& w) {
        w.writeln("struct MergedVisitor;");
    }

    struct UtilityClassField {
        std::string_view name;
        std::string_view type;
        std::string_view constructor_type;
        std::string_view constructor_additional_arg;
        bool derived = false;

        bool back_ptr_when_derived = false;
    };

    struct UtilityClass {
        std::string_view name;
        std::vector<UtilityClassField> fields;

        std::string constructor_signature_on_derived(std::string_view base_name) const {
            CodeWriter w;
            w.write(name, "(");
            w.write(constructor_args(true));
            w.write(") :");
            {
                w.write(base_name, "(");
                w.write(pass_as_base_args());
                w.write(")");
            }
            w.write(",");
            w.write(derived_constructor_initializers());
            w.writeln("{}");
            return w.out();
        }

        std::string constructor_args(bool on_derived = false) const {
            CodeWriter w;
            bool first = true;
            for (auto& field : fields) {
                if (field.constructor_type.empty()) {
                    continue;
                }
                if (on_derived && field.back_ptr_when_derived) {
                    continue;
                }
                if (!first) {
                    w.write(",");
                }
                first = false;
                w.write(field.constructor_type, " ", field.name);
            }
            return w.out();
        }

        std::string constructor_initializers() const {
            CodeWriter w;
            bool first = true;
            for (auto& field : fields) {
                if (field.constructor_type.empty()) {
                    continue;
                }
                if (!first) {
                    w.write(",");
                }
                first = false;
                w.write(field.name, "(", field.name);
                if (!field.constructor_additional_arg.empty()) {
                    w.write(", ", field.constructor_additional_arg);
                }
                w.write(")");
            }
            return w.out();
        }

        std::string constructor_signature() const {
            CodeWriter w;
            w.write(name, "(");
            w.write(constructor_args());
            w.write(") :");
            w.write(constructor_initializers());
            w.write("{}");
            return w.out();
        }

        std::string derived_constructor_initializers() const {
            CodeWriter w;
            bool first = true;
            for (auto& field : fields) {
                if (!field.derived) {
                    continue;
                }
                if (field.constructor_type.empty()) {
                    continue;
                }
                if (!first) {
                    w.write(", ");
                }
                first = false;
                w.write(field.name, "(", field.name);
                if (!field.constructor_additional_arg.empty()) {
                    w.write(", ", field.constructor_additional_arg);
                }
                w.write(")");
            }
            return w.out();
        }

        std::string pass_as_base_args() const {
            CodeWriter w;
            bool first = true;
            for (auto& field : fields) {
                if (field.derived) {
                    continue;
                }
                if (field.constructor_type.empty()) {
                    continue;
                }
                if (!first) {
                    w.write(", ");
                }
                first = false;
                if (field.back_ptr_when_derived) {
                    w.write("this");
                    continue;
                }
                w.write(field.name);
            }
            return w.out();
        }

        std::string pass_as_args() const {
            CodeWriter w;
            bool first = true;
            for (auto& field : fields) {
                if (field.constructor_type.empty()) {
                    continue;
                }
                if (!first) {
                    w.write(", ");
                }
                first = false;
                w.write(field.name);
            }
            return w.out();
        }
    };

    using UtilityClasses = std::map<std::string, UtilityClass>;

    UtilityClasses generate_utility_classes(bool ebmgen_mode) {
        UtilityClasses utility_classes;
        UtilityClass base_visitor;
        base_visitor.name = "BaseVisitor";
        if (!ebmgen_mode) {
            base_visitor.fields.push_back(UtilityClassField{.name = "program_name", .type = "static constexpr const char*"});
            base_visitor.fields.push_back(UtilityClassField{.name = legacy_compat_ptr_name, .type = "MergedVisitor* const", .constructor_type = "MergedVisitor*", .back_ptr_when_derived = true});
            base_visitor.fields.push_back(UtilityClassField{.name = "flags", .type = "Flags&", .constructor_type = "Flags&"});
            base_visitor.fields.push_back(UtilityClassField{.name = "output", .type = "Output&", .constructor_type = "Output&"});
            base_visitor.fields.push_back(UtilityClassField{.name = "wm", .type = "ebmcodegen::WriterManager<CodeWriter>", .constructor_type = "futils::binary::writer&"});
        }
        base_visitor.fields.push_back(UtilityClassField{.name = "module_", .type = "ebmgen::MappingTable", .constructor_type = "ebmgen::EBMProxy", .constructor_additional_arg = "ebmgen::lazy_init"});
        utility_classes["BaseVisitor"] = std::move(base_visitor);
        return utility_classes;
    }

    void generate_Contexts_forward_declaration(CodeWriter& w, const std::vector<ContextClasses>& context_classes) {
        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                if (cls.type_params.size()) {
                    continue;
                }
                w.writeln("struct ", context_name(cls), ";");
            }
        }
    }

    void generate_BaseVisitor(CodeWriter& w, const IncludeInfo& includes_info, const UtilityClass& base_visitor, bool ebmgen_mode) {
        w.writeln("struct BaseVisitor {");
        {
            auto scope = w.indent_scope();
            w.writeln(base_visitor.constructor_signature());
            for (auto& field : base_visitor.fields) {
                w.write(field.type, " ", field.name);
                if (field.name == "program_name") {
                    w.write(" = \"", includes_info.includes.program_name, "\"");
                }
                w.writeln(";");
            }
            if (!ebmgen_mode) {
                user_config_include(w, includes_info.user_include.visitor, includes_info);
            }
        }
        w.writeln("};");

        w.writeln("template<typename V>");
        w.writeln("concept BaseVisitorLike = std::derived_from<V,BaseVisitor>;");
    }

    void generate_merged_visitor(CodeWriter& w, const std::vector<HookType>& hooks, const std::vector<ContextClasses>& context_classes, const std::string_view result_type, UtilityClasses& utils) {
        // CodeWriter impl;
        CodeWriter derive;
        CodeWriter merged;
        derive.writeln("struct VisitorsImpl {");
        bool first = true;
        merged.writeln("struct MergedVisitor : BaseVisitor {");
        {
            auto scope = derive.indent_scope();
            auto scope_2 = merged.indent_scope();
            auto new_class = utils["BaseVisitor"];
            new_class.name = "MergedVisitor";
            new_class.fields.push_back(UtilityClassField{.name = "impl", .type = "VisitorsImpl&", .constructor_type = "VisitorsImpl&", .derived = true});
            merged.writeln(new_class.constructor_signature_on_derived("BaseVisitor"));
            merged.writeln("VisitorsImpl& impl;");
            for (auto& cls_group : context_classes) {
                for (auto& cls : cls_group.classes) {
                    for (auto& hook : hooks) {
                        auto instance_name = hook.visitor_instance_holder_name(cls);
                        auto class_instance = hook.visitor_instance_name(cls);
                        /*
                        if (!first) {
                            derive.writeln(",");
                        }
                        */
                        first = false;
                        // derive.write("public ", class_instance);
                        // impl.writeln(class_instance, "& ", instance_name, "() {");
                        // impl.indent_writeln("return static_cast<", class_instance, "&>(*this);");
                        // impl.writeln("}");
                        derive.writeln(class_instance, " ", instance_name, ";");
                    }
                    generate_get_hook(derive, hooks, cls);
                    generate_visitor_implementation(merged, cls, result_type);
                }
            }
        }
        merged.writeln("};");
        /*
        derive.writeln("{");
        {
            auto scope = derive.indent_scope();
            derive.write_unformatted(impl.out());
        }
        */
        derive.writeln("};");
        w.write_unformatted(derive.out());
        w.write_unformatted(merged.out());
    }

    void generate_Flags(CodeWriter& w, const IncludeInfo& includes_info) {
        auto& flags = includes_info.includes;
        auto with_flag_bind = [&](bool on_define) {
            constexpr auto ensure_c_ident = "static_assert(ebmcodegen::util::internal::is_c_ident(#name),\"name must be a valid C identifier\");";
            w.writeln("#define DEFINE_FLAG(type,name,default_,flag_name,flag_func,...) \\");
            if (on_define) {
                w.indent_writeln(ensure_c_ident, "type name = default_");
            }
            else {
                w.indent_writeln("ctx.flag_func(&name,flag_name,__VA_ARGS__)");
            }
            w.write("#define WEB_FILTERED(...) ");
            if (!on_define) {
                w.write("web_filtered.insert_range(std::set{__VA_ARGS__})");
            }
            w.writeln();
            w.write("#define WEB_OPTION_HANDLE_TYPE(flag_name,type_name) ");
            if (!on_define) {
                w.write("static_assert(ebmcodegen::internal::is_web_type_allowed(type_name),\"Type \" #type_name \" is not allowed to be mapped to web\");");
                w.write("web_type_map[flag_name] = type_name");
            }
            w.writeln();
            auto map_name = [&](auto name, auto dst, auto src) {
                w.write("#define ", name, "(", src, ") ");
                if (!on_define) {
                    w.write(dst, " = ", src);
                }
                w.writeln();
            };
            map_name("WEB_UI_NAME", "ui_lang_name", "name");
            map_name("WEB_LSP_NAME", "lsp_name", "name");
            map_name("WEB_WORKER_NAME", "webworker_name", "name");
            w.write("#define FILE_EXTENSIONS(...) ");
            if (!on_define) {
                w.write("file_extensions = std::vector<std::string_view>{__VA_ARGS__}");
            }
            w.writeln();

            w.writeln("#define DEFINE_BOOL_FLAG(name,default_,flag_name,desc) DEFINE_FLAG(bool,name,default_,flag_name,VarBool,desc)");
            w.writeln("#define DEFINE_STRING_FLAG(name,default_,flag_name,desc,arg_desc) DEFINE_FLAG(std::string_view,name,default_,flag_name,VarString<true>,desc,arg_desc)");
            w.writeln("#define DEFINE_INT_FLAG(name,type,default_,flag_name,desc,arg_desc) DEFINE_FLAG(type,name,default_,flag_name,VarInt,desc,arg_desc)");
            w.write("#define BEGIN_MAP_FLAG(name,MappedType,default_,flag_name,desc)");
            if (on_define) {
                w.write(ensure_c_ident, "MappedType name = default_;");
            }
            else {
                w.write("{ std::map<std::string,MappedType> map__; auto& target__ = name; auto flag_name__ = flag_name; auto desc__ = desc; std::string arg_desc__ = \"{\"; ");
            }
            w.writeln();
            w.write("#define MAP_FLAG_ITEM(key,value) ");
            if (!on_define) {
                w.write("map__[key] = value;");
                w.write("if (!arg_desc__.empty() && arg_desc__.back() != '{') { arg_desc__ += \",\"; }");
                w.write("arg_desc__ += key;");
            }
            w.writeln();
            w.write("#define END_MAP_FLAG() ");
            if (!on_define) {
                w.write("ctx.VarMap(&target__,flag_name__,desc__,arg_desc__ + \"}\",std::move(map__)); }");
            }
            w.writeln();
            w.write("#define CONFIG_MAP(config_name,name)");
            if (!on_define) {
                w.write(ensure_c_ident, "config_map[config_name] = &name;");
            }
            w.writeln();

            user_config_include(w, includes_info.user_include.flags, includes_info);
            w.writeln("#undef DEFINE_FLAG");
            w.writeln("#undef WEB_FILTERED");
            w.writeln("#undef DEFINE_BOOL_FLAG");
            w.writeln("#undef DEFINE_STRING_FLAG");
            w.writeln("#undef DEFINE_INT_FLAG");
            w.writeln("#undef BEGIN_MAP_FLAG");
            w.writeln("#undef MAP_FLAG_ITEM");
            w.writeln("#undef END_MAP_FLAG");
            w.writeln("#undef WEB_UI_NAME");
            w.writeln("#undef WEB_LSP_NAME");
            w.writeln("#undef WEB_WORKER_NAME");
            w.writeln("#undef FILE_EXTENSIONS");
            w.writeln("#undef CONFIG_MAP");
            w.writeln("#undef WEB_OPTION_HANDLE_TYPE");
        };

        w.writeln("struct Flags : ebmcodegen::Flags {");
        {
            auto flag_scope = w.indent_scope();
            with_flag_bind(true);
            user_config_include(w, includes_info.user_include.flags_struct, includes_info);
            w.writeln("void bind(futils::cmdline::option::Context& ctx) {");
            auto nested_scope = w.indent_scope();
            w.writeln("lang_name = \"", flags.lang, "\";");
            w.writeln("ui_lang_name = lang_name;");
            w.writeln("lsp_name = lang_name;");
            w.writeln("webworker_name = \"", flags.program_name, "\";");
            w.writeln("file_extensions = {\".", flags.lang, "\"};");
            w.writeln("ebmcodegen::Flags::bind(ctx); // bind basis");
            with_flag_bind(false);
            user_config_include(w, includes_info.user_include.flags_bind, includes_info);
            nested_scope.execute();
            w.writeln("}");
        }
        w.writeln("};");
    }

    void generate_Output(CodeWriter& w, const IncludeInfo& includes_info) {
        w.writeln("struct Output : ebmcodegen::Output {");
        user_config_include(w, includes_info.user_include.output, includes_info);
        w.writeln("};");
    }

    void generate_Result(CodeWriter& w, bool is_codegen, const IncludeInfo& includes_info) {
        w.writeln();
        w.writeln("struct Result {");
        auto result_scope = w.indent_scope();
        if (is_codegen) {
            w.writeln("private: CodeWriter value;");
            w.writeln("public: Result(std::string v) { value.write(v); }");
            w.writeln("Result(const char* v) { value.write(v); }");
            w.writeln("Result(CodeWriter&& v) : value(std::move(v)) {}");
            w.writeln("Result() = default;");
            w.writeln("constexpr std::string to_string() const {");
            w.indent_writeln("return value.to_string();");
            w.writeln("}");
            w.writeln("constexpr const CodeWriter& to_writer() const {");
            w.indent_writeln("return value;");
            w.writeln("}");
            w.writeln("constexpr CodeWriter& to_writer() {");
            w.indent_writeln("return value;");
            w.writeln("}");
        }
        if (!includes_info.includes.ebmgen_mode) {
            user_config_include(w, includes_info.user_include.result, includes_info);
        }
        result_scope.execute();
        w.writeln("};");
    }

    void generate_namespace_injection(CodeWriter& w) {
        w.writeln("using namespace ebmgen;");
        w.writeln("using namespace ebmcodegen::util;");
        // w.writeln("using CodeWriter = futils::code::LocWriter<std::string,std::vector,ebm::AnyRef>;");
    }

    void generate_namespace(CodeWriter& w, std::string_view ns_name, bool open) {
        if (open) {
            w.writeln("namespace ", ns_name, " {");
        }
        else {
            w.writeln("}  // namespace ", ns_name);
        }
    }

    std::string list_dispatcher_customization_point_name(const ContextClass& cls) {
        return "ListDispatcher_" + cls.class_name();
    }

    void generate_list_dispatch_customization_point(CodeWriter& w, const ContextClass& cls, const std::string_view result_type, bool is_codegen) {
        w.writeln("template<typename Result>");
        w.writeln("struct ", list_dispatcher_customization_point_name(cls), " {");
        {
            auto scope = w.indent_scope();
            w.writeln("template<typename Context>");
            w.writeln("expected<void> on_dispatch(Context&& ctx,const ebm::", cls.class_name(), "& in,", result_type, "&& result) {");
            {
                auto inner_scope = w.indent_scope();
                w.writeln("if (!result) {");
                {
                    auto if_scope = w.indent_scope();
                    w.writeln("return unexpect_error(std::move(result.error()));");
                }
                w.writeln("}");
                w.writeln("return {}; // Default no-op implementation");
            }
            w.writeln("}");

            w.writeln("template<typename Context>");
            w.writeln(result_type, " finalize(Context&& ctx, const ebm::", cls.class_name(), "& in) {");
            {
                auto inner_scope = w.indent_scope();
                w.writeln("return {}; // Default no-op implementation");
            }
            w.writeln("}");
        }
        w.writeln("};");
        w.writeln();
        if (is_codegen) {
            // specialization for Result
            w.writeln("template<>");
            w.writeln("struct ", list_dispatcher_customization_point_name(cls), "<Result> {");
            {
                auto scope = w.indent_scope();

                w.writeln("CodeWriter result;");

                w.writeln("template<typename Context>");
                w.writeln("expected<void> on_dispatch(Context&& ctx,const ebm::", cls.class_name(), "& in,", result_type, "&& result) {");
                {
                    auto inner_scope = w.indent_scope();
                    w.writeln("if (!result) {");
                    {
                        auto if_scope = w.indent_scope();
                        w.writeln("return unexpect_error(std::move(result.error()));");
                    }
                    w.writeln("}");
                    w.writeln("this->result.write(std::move(result->to_writer()));");
                    w.writeln("return {};");
                }
                w.writeln("}");

                w.writeln("template<typename Context>");
                w.writeln(result_type, " finalize(Context&& ctx, const ebm::", cls.class_name(), "& in) {");
                {
                    auto inner_scope = w.indent_scope();

                    w.writeln("return std::move(result);");
                }
                w.writeln("}");
            }
            w.writeln("};");
        }
    }

    void generate_list_dispatch_default(CodeWriter& hdr, const ContextClass& cls, bool is_codegen, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_List)) {
            return;
        }
        generate_list_dispatch_customization_point(hdr, cls, result_type, is_codegen);
        auto class_name = cls.class_name();
        hdr.writeln("template<", template_param_result(true), ", typename Context>");
        hdr.writeln(result_type, " dispatch_", class_name, "_default(Context&& ctx,const ebm::", class_name, "& in) {");
        {
            auto list_scope = hdr.indent_scope();
            hdr.writeln("", list_dispatcher_customization_point_name(cls), "<Result> dispatcher;");
            hdr.writeln("for(auto& elem:in.container) {");
            {
                auto loop_scope = hdr.indent_scope();
                hdr.writeln("auto result = visit_", cls.base, "<Result>(ctx,elem);");
                hdr.writeln("MAYBE_VOID(dispatch,dispatcher.on_dispatch(std::forward<Context>(ctx),in,std::move(result)));");
            }
            hdr.writeln("}");
            hdr.writeln("return dispatcher.finalize(std::forward<Context>(ctx),in);");
        }
        hdr.writeln("}");
    }

    void generate_generic_dispatch_default(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_Generic)) {
            return;
        }
        auto class_name = cls.class_name();
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " dispatch_", class_name, "_default(Context&& ctx,const ebm::", class_name, "& in,ebm::", cls.ref_name(), " alias_ref = {}) {");
        {
            auto scope = w.indent_scope();
            w.writeln("switch(in.body.kind) {");
            {
                auto switch_scope = w.indent_scope();
                auto do_for_each = [&](auto t) {
                    using T = std::decay_t<decltype(t)>;
                    for (size_t i = 0; to_string(T(i))[0]; i++) {
                        w.writeln("case ebm::", cls.base, "Kind::", to_string(T(i)), ": {");
                        {
                            auto case_scope = w.indent_scope();
                            w.writeln("return dispatch_", cls.base, "_", to_string(T(i)), "<Result>(std::forward<Context>(ctx),in,alias_ref);");
                        }
                        w.writeln("}");
                    }
                };
                if (cls.base == "Statement") {
                    do_for_each(ebm::StatementKind{});
                }
                else if (cls.base == "Expression") {
                    do_for_each(ebm::ExpressionKind{});
                }
                else if (cls.base == "Type") {
                    do_for_each(ebm::TypeKind{});
                }
                w.writeln("default: {");
                {
                    auto default_scope = w.indent_scope();
                    w.writeln("return unexpect_error(\"Unknown ", cls.base, " kind: {}\", to_string(in.body.kind));");
                }
                w.writeln("}");
            }
            w.writeln("}");
        }
        w.writeln("}");
    }

    void generate_priority_check_macro(CodeWriter& w, const ContextClass& cls, size_t i) {
        auto macro_name = priority_check_macro_name(cls);
        w.writeln("#if !defined(", macro_name, ")");
        w.writeln("#define ", macro_name, " ", std::to_string(i));
        w.writeln("#endif");
    }

    void generate_generator_default_hook(CodeWriter& src, const HookType& hook, size_t index, const ContextClass& cls, bool is_codegen, const std::string_view result_type) {
        auto type_name = hook.visitor_instance_name(cls);
        src.writeln("template <>");
        src.writeln("struct ", type_name, " {");
        {
            auto scope = src.indent_scope();
            src.writeln("template<typename Context>");
            src.writeln("auto visit(Context&& ctx) {");
            {
                auto visit_scope = src.indent_scope();
                if (cls.has(ContextClassKind_Observe)) {
                    src.writeln("return pass;");
                }
                else if (cls.has(ContextClassKind_List)) {
                    src.writeln("return dispatch_", cls.class_name(), "_default(ctx,ctx.in);");
                }
                else if (cls.has(ContextClassKind_Generic)) {
                    src.writeln("return dispatch_", cls.class_name(), "_default(ctx,ctx.in,ctx.alias_ref);");
                }
                else {
                    std::string third_arg = "get_id(ctx.item_id)";
                    if (cls.has(ContextClassKind_Special)) {
                        third_arg = "0";
                    }
                    src.writeln("return visit_unimplemented(", get_visitor_fn, "<void>(ctx,ctx),\"", cls.class_name(), "\",", third_arg, ");");
                }
            }
            src.writeln("}");
        }
        src.writeln("};");
        generate_priority_check_macro(src, cls, index);
    }

    void include_or_default(CodeWriter& w, std::string_view header, auto&& then, auto&& otherwise) {
        w.writeln("#if __has_include(", header, ")");
        then();
        w.writeln("#else");
        otherwise();
        w.writeln("#endif");
    }

    auto define_local_macro(CodeWriter& w, std::string_view macro_name, std::string_view func_arg, std::string_view body) {
        w.writeln("#define ", macro_name, func_arg, " ", body);
        return futils::helper::defer([=, &w]() {
            w.writeln("#define TEMPORARY_CHECK_MACRO(x) static_assert(std::string_view(#x) == std::string_view(\"", body, "\"))");
            w.writeln("#define PASS_TO_CHECK(x) TEMPORARY_CHECK_MACRO(x)");
            w.writeln("PASS_TO_CHECK(", macro_name, func_arg, ");");
            w.writeln("#undef TEMPORARY_CHECK_MACRO");
            w.writeln("#undef PASS_TO_CHECK");
            w.writeln("#undef ", macro_name);
        });
    }

    void generate_common_include_guard(CodeWriter& w, bool open) {
        if (open) {
            w.writeln("// This is a measure to prevent any impact on compilation even if a user mistakenly changes something like #include \"lang/codegen.hpp\" to #include \"other_lang/codegen.hpp\".");
            w.writeln("#ifndef ", macro_CODEGEN_COMMON_INCLUDE_GUARD);
            w.writeln("#define ", macro_CODEGEN_COMMON_INCLUDE_GUARD, " 1");
        }
        else {
            w.writeln("#endif // ", macro_CODEGEN_COMMON_INCLUDE_GUARD);
        }
    }

    void generate_user_include_before(CodeWriter& w, const IncludeInfo& includes_locations) {
        user_config_include(w, includes_locations.user_include.includes, includes_locations);
    }

    void generate_user_include_after(CodeWriter& w, const IncludeInfo& includes_locations) {
        user_config_include(w, includes_locations.user_include.post_includes, includes_locations);
    }

    std::string dispatch_compatibility_name(const ContextClass& cls) {
        // base + "_dispatch" (+ ["_before"|"_after"])
        std::string name = std::string(cls.base) + "_dispatch";
        if (cls.has(ContextClassKind_Before)) {
            name += "_before";
        }
        else if (cls.has(ContextClassKind_After)) {
            name += "_after";
        }
        return name;
    }

    void generate_dummy_macro_for_class(CodeWriter& w, std::string_view ns_name, const HookType& hook, const ContextClass& cls) {
        auto instance = std::format("{}::{}", ns_name, context_instance_name(cls));
        auto visitor = hook.visitor_instance_name(cls, ns_name);
        auto upper_ns = upper(ns_name);
        auto cls_name = cls.class_name();
        if (cls.has(ContextClassKind_Generic)) {
            cls_name = dispatch_compatibility_name(cls);
        }
        w.writeln("#define ", upper_ns, "_", macro_CODEGEN_VISITOR, "_", cls_name, " ", visitor);
        // w.writeln("#define ", upper_ns, "_", macro_CODEGEN_CONTEXT_PARAMETERS, "_", cls_name, " ", cls.type_parameters_body(true));
        w.writeln("#define ", upper_ns, "_", macro_CODEGEN_CONTEXT, "_", cls_name, " ", instance);
    }

    void generate_undef_dummy_macros(CodeWriter& src) {
        src.writeln("#undef ", macro_CODEGEN_VISITOR);
        src.writeln("#undef ", macro_CODEGEN_CONTEXT_PARAMETERS);
        src.writeln("#undef ", macro_CODEGEN_CONTEXT);
    }

    void generate_dummy_macros(CodeWriter& hdr, std::string_view ns_name, const HookType& hook, const std::vector<ContextClasses>& context_classes) {
        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                generate_dummy_macro_for_class(hdr, ns_name, hook, cls);
            }
        }
        hdr.writeln("#define ", macro_CODEGEN_NAMESPACE, " ", ns_name);
        hdr.writeln("#define ", macro_CODEGEN_VISITOR, "(name) ", upper(ns_name), "_", macro_CODEGEN_VISITOR, "_##name");
        hdr.writeln("#define ", macro_CODEGEN_CONTEXT_PARAMETERS, "(name) ", upper(ns_name), "_", macro_CODEGEN_CONTEXT_PARAMETERS, "_##name");
        hdr.writeln("#define ", macro_CODEGEN_CONTEXT, "(name) ", upper(ns_name), "_", macro_CODEGEN_CONTEXT, "_##name");
    }

    void generate_get_visitor_from_context(CodeWriter& w, const std::string_view result_type) {
        w.writeln("template<typename Context>");
        w.writeln("concept HasVisitorInContext = requires(const Context& ctx) { ctx.visitor; };");
        w.writeln("template<typename Context>");
        w.writeln("concept HasLegacyVisitorInContext = requires(const Context& ctx) { *ctx.", legacy_compat_ptr_name, "; };");
        generate_visitor_requirements(w, result_type);

        w.writeln("template<typename Context>");
        w.writeln("decltype(auto) ", get_visitor_arg_fn, "(Context&& ctx) {");
        {
            auto scope = w.indent_scope();
            w.writeln("if constexpr (HasVisitorInContext<Context>) {");
            {
                auto if_scope = w.indent_scope();
                w.writeln("return *ctx.visitor.", legacy_compat_ptr_name, ";");
            }
            w.writeln("}");
            w.writeln("else if constexpr (HasLegacyVisitorInContext<Context>) {");
            {
                auto else_if_scope = w.indent_scope();
                w.writeln("return *ctx.", legacy_compat_ptr_name, ";");
            }
            w.writeln("}");
            w.writeln("else {");
            {
                auto else_scope = w.indent_scope();
                w.writeln("static_assert(dependent_false<Context>, \"No visitor found in context\");");
            }
            w.writeln("}");
        }
        w.writeln("}");

        w.writeln("template<", template_param_result(false), ",typename UserContext,typename TypeContext>");
        w.writeln("auto& ", get_visitor_fn, "(UserContext&& uctx,TypeContext&& ctx) {");
        {
            auto scope = w.indent_scope();
            w.writeln("if constexpr (HasVisitor<Result,UserContext,TypeContext>) {");
            {
                auto if_scope = w.indent_scope();
                w.writeln("return uctx;");
            }
            w.writeln("}");
            w.writeln("else {");
            {
                auto else_scope = w.indent_scope();
                w.writeln("return ", get_visitor_arg_fn, "(uctx);");
            }
            w.writeln("}");
        }
        w.writeln("}");
    }

    void generate_inlined_hook(CodeWriter& w,
                               std::string_view ns_name,
                               std::string_view header_name,
                               const HookType& hook, const ContextClass& cls, const std::string_view result_type, UtilityClasses& utils) {
        auto instance = hook.visitor_instance_name(cls, ns_name);
        w.writeln("// Inlined hook for ", cls.class_name(), " for backward compatibility");
        w.writeln("template <>");
        w.writeln("struct ", instance, " {");
        {
            auto scope = w.indent_scope();
            auto context_name = context_instance_name(cls);
            w.writeln("// for backward compatibility");
            w.writeln(ns_name, "::MergedVisitor* ", legacy_compat_ptr_name, " = nullptr;");
            // w.writeln(cls.type_parameters(true));
            w.writeln(result_type, " visit(", context_name, "& ctx) {");
            {
                auto visit_scope = w.indent_scope();
                auto deconstruct_macro = deconstruct_macro_name(ns_name, cls);
                w.writeln(deconstruct_macro, "(ctx);");
                auto& base_visitor = utils["BaseVisitor"];
                for (auto& field : base_visitor.fields) {
                    if (field.name == legacy_compat_ptr_name) {
                        continue;
                    }
                    w.writeln("auto& ", field.name, " = ctx.visitor.", field.name, ";");
                }
                w.writeln(legacy_compat_ptr_name, " = ctx.visitor.", legacy_compat_ptr_name, ";");
                w.writeln("// for backward compatibility");
                w.writeln("auto& root = wm.root;");
                w.writeln("auto add_writer = [&]{ return wm.add_writer(); };");
                w.writeln("auto get_writer = [&]{ return wm.get_writer(); };");
                w.writeln("using namespace ", ns_name, ";");
                w.writeln("#include ", header_name);
                if (cls.has(ContextClassKind_Observe)) {
                    w.writeln("return pass;");
                }
                else {
                    w.writeln("return {};");
                }
            }
            w.writeln("}");
        }
        w.writeln("};");
    }

    std::string header_name(const LocationInfo& location, const ContextClass& cls) {
        auto name = cls.class_name();
        if (cls.has(ContextClassKind_Generic)) {
            name = dispatch_compatibility_name(cls);
        }
        return std::format("{}{}", name, location.suffix);
    }

    void generate_user_implemented_includes(CodeWriter& w, std::string_view ns_name, const std::vector<HookType>& hooks, const IncludeLocations& locations, const std::vector<ContextClasses>& context_classes, const std::string_view result_type, UtilityClasses& utils) {
        assert(hooks.size() - 1 == locations.include_locations.size());
        // without last hidden hook
        for (auto i = 0; i < hooks.size() - 1; i++) {
            auto& hook = hooks[i];
            auto location = locations.include_locations[i];
            for (auto& cls_group : context_classes) {
                for (auto& cls : cls_group.classes) {
                    auto header = std::format("\"{}{}.hpp\"", location.location, header_name(location, cls));
                    auto instance = hook.visitor_instance_name(cls, ns_name);
                    include_or_default(
                        w, header,
                        [&] {
                            auto ctx_instance = context_instance_name(cls);
                            if (hook.name.contains("Inlined")) {
                                generate_inlined_hook(w, ns_name, header, hook, cls, result_type, utils);
                            }
                            else {
                                // auto type_param_body = cls.type_parameters_body(true);
                                auto current_class = define_local_macro(w, macro_CODEGEN_VISITOR, "(dummy_name)", instance);
                                // auto current_context_parameters = define_local_macro(w, macro_CODEGEN_CONTEXT_PARAMETERS, "(dummy_name)", type_param_body);
                                auto current_context = define_local_macro(w, macro_CODEGEN_CONTEXT, "(dummy_name)", ctx_instance);
                                w.writeln("#include ", header);
                            }
                            w.writeln("static_assert(sizeof(", instance, ")); // ensure included");
                            generate_priority_check_macro(w, cls, i);
                        },
                        [&] {
                            w.writeln("template <>");
                            w.writeln("struct ", instance, " {}; // Unimplemented");
                        });
                }
            }
        }
    }

    void generate_list_visit_for_ui(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_List)) {
            return;
        }
        w.writeln("// list visitor for ", cls.class_name());
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " visit_", cls.class_name(), "(Context&& ctx,const ebm::", cls.class_name(), "& in) {");
        {
            auto scope = w.indent_scope();
            auto dispatch_fn = dispatch_fn_name(cls);
            w.writeln("return ", dispatch_fn, "<Result>(std::forward<Context>(ctx),in);");
        }
        w.writeln("}");

        w.writeln("// for DSL convenience");
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " visit_Object(Context&& ctx,const ebm::", cls.class_name(), "& in)  {");
        {
            auto scope = w.indent_scope();
            w.writeln("return visit_", cls.class_name(), "<Result>(std::forward<Context>(ctx),in);");
        }
        w.writeln("}");
    }

    void generate_generic_visit_for_ui(CodeWriter& w, const ContextClass& cls, const std::string_view result_type) {
        if (!cls.has(ContextClassKind_Generic)) {
            return;
        }
        w.writeln("// generic visitor for ", cls.class_name());
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " visit_", cls.class_name(), "(Context&& ctx,const ebm::", cls.class_name(), "& in,ebm::", cls.ref_name(), " alias_ref = {}) {");
        {
            auto scope = w.indent_scope();
            auto dispatch_fn = dispatch_fn_name(cls);
            w.writeln("return ", dispatch_fn, "<Result>(std::forward<Context>(ctx),in,alias_ref);");
        }
        w.writeln("}");

        w.writeln("// short-hand visitor for ", cls.class_name());
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " visit_", cls.class_name(), "(Context&& ctx,const ebm::", cls.ref_name(), "& ref) {");
        {
            auto scope = w.indent_scope();
            w.writeln("MAYBE(elem, ", get_visitor_arg_fn, "(ctx).module_.get_", lower(cls.class_name()), "(ref));");
            auto dispatch_fn = dispatch_fn_name(cls);
            w.writeln("return ", dispatch_fn, "<Result>(std::forward<Context>(ctx),elem,ref);");
        }
        w.writeln("}");

        w.writeln("// for DSL convenience");
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " visit_Object(Context&& ctx,const ebm::", cls.class_name(), "& in, ebm::", cls.ref_name(), " alias_ref = {})  {");
        {
            auto scope = w.indent_scope();
            w.writeln("return visit_", cls.class_name(), "<Result>(std::forward<Context>(ctx),in,alias_ref);");
        }
        w.writeln("}");

        w.writeln("// for DSL convenience");
        w.writeln("template<", template_param_result(true), ", typename Context>");
        w.writeln(result_type, " visit_Object(Context&& ctx, ebm::", cls.ref_name(), " ref)  {");
        {
            auto scope = w.indent_scope();
            w.writeln("return visit_", cls.class_name(), "<Result>(std::forward<Context>(ctx),ref);");
        }
        w.writeln("}");
    }

    void generate_initial_context(CodeWriter& w) {
        w.writeln("struct InitialContext : ebmcodegen::util::ContextBase<InitialContext> {");
        {
            auto scope = w.indent_scope();
            w.writeln("BaseVisitor& visitor;");
        }
        w.writeln("};");
    }

    void generate_impl_getter_header(CodeWriter& w) {
        w.writeln("template<class R = void, typename Context,typename Callback>");
        w.writeln("R get_visitor_impl(Context&& ctx,Callback&& cb);");
    }

    void generate_impl_getter_source(CodeWriter& w) {
        w.writeln("template<class R, typename Context,typename Callback>");
        w.writeln("R get_visitor_impl(Context&& ctx,Callback&& cb) {");
        {
            auto scope = w.indent_scope();
            w.writeln("return cb(ctx.visitor.__legacy_compat_ptr->impl);");
        }
        w.writeln("}");
    }

    void generate_user_interface(CodeWriter& w, std::vector<ContextClasses>& context_classes, const std::string_view result_type) {
        w.writeln("// for backward compatibility");
        w.writeln();

        for (auto& cls : context_classes) {
            generate_generic_visit_for_ui(w, cls.main(), result_type);
            generate_list_visit_for_ui(w, cls.main(), result_type);
        }
    }

    void generate_adl_lookup_helper(CodeWriter& hdr) {
        hdr.writeln("// for adl lookup for sub-visitor");
        hdr.writeln("template<", template_param_result(true), ", typename Context>");
        hdr.writeln("auto visit_Object_adl(Context&& ctx,auto&& obj,BaseVisitor&) {");
        {
            auto scope = hdr.indent_scope();
            hdr.writeln("return visit_Object<Result>(std::forward<Context>(ctx),std::forward<decltype(obj)>(obj));");
        }
        hdr.writeln("}");

        hdr.writeln("template<", template_param_result(true), ", typename UserContext, typename TypeContext>");
        hdr.writeln("auto traverse_children_adl(UserContext&& uctx, TypeContext&& type_ctx,BaseVisitor&) {");
        {
            auto scope = hdr.indent_scope();
            hdr.writeln("return traverse_children<Result>(std::forward<UserContext>(uctx),std::forward<TypeContext>(type_ctx));");
        }
        hdr.writeln("}");
    }

    void generate_entry_point(CodeWriter& w, std::string_view ns_name) {
        auto fq_result_type = std::format("ebmgen::expected<{}::{}>", ns_name, "Result");
        w.writeln("DEFINE_ENTRY(", ns_name, "::Flags, ", ns_name, "::Output) {");
        {
            auto scope = w.indent_scope();
            w.writeln(ns_name, "::VisitorsImpl visitors_impl;");
            w.writeln(ns_name, "::MergedVisitor visitor{flags,output,w,ebm,visitors_impl};");
            w.writeln("auto entry_function = [&]() -> ", fq_result_type, " {");
            {
                auto entry_scope = w.indent_scope();
                w.writeln(ns_name, "::InitialContext initial_ctx{.visitor = visitor};");
                w.writeln("auto pre_visit_result = ", ns_name, "::dispatch_pre_visitor(initial_ctx,ebm);");
                handle_hijack_logic(w, "pre_visit_result");
                w.writeln("if(!visitor.module_.valid()) {");
                w.indent_writeln("visitor.module_.build_maps(); // initialize mapping tables if not yet");
                w.writeln("}");
                w.writeln("auto entry_result = ", ns_name, "::dispatch_entry(initial_ctx);");
                w.writeln("auto post_visit_result = ", ns_name, "::dispatch_post_entry(initial_ctx,entry_result);");
                handle_hijack_logic(w, "post_visit_result");
                w.writeln("return entry_result;");
            }
            w.writeln("};");
            w.writeln("auto result = entry_function();");
            w.writeln("if (!result) {");
            {
                auto err_scope = w.indent_scope();
                w.writeln("futils::wrap::cerr_wrap() << visitor.program_name << \": error: \" << result.error().error();");
                w.writeln("return 1;");
            }
            w.writeln("}");
            w.writeln("return 0;");
        }
        w.writeln("}");
    }

    auto generate_hooks() {
        // Inlined hooks are for backward compatibility
        std::vector<HookType> hooks = {
            {.name = "UserHook"},
            {.name = "UserInlinedHook", .is_config_include_target = true},
            {.name = "UserDSLHook"},
            {.name = "UserInlinedDSLHook", .is_config_include_target = true},
            {.name = "DefaultCodegenVisitorHook"},
            {.name = "DefaultCodegenVisitorInlinedHook", .is_config_include_target = true},
            {.name = "GeneratorDefaultHook"},  // this is hidden
        };
        return hooks;
    }

    void generate_traversal_children_for_class(CodeWriter& hdr, CodeWriter& src, const ContextClass& cls, std::string_view result_type, const std::map<std::string_view, Struct>& struct_map) {
        hdr.writeln("template<", template_param_result(true), ", typename UserContext,typename TypeContext>");
        hdr.writeln(result_type, " traverse_children_", cls.class_name(), "(UserContext&& ctx,TypeContext&& type_ctx);");
        src.writeln("template<", template_param_result(false), ", typename UserContext,typename TypeContext>");
        src.writeln(result_type, " traverse_children_", cls.class_name(), "(UserContext&& ctx,TypeContext&& type_ctx) {");
        {
            auto scope = src.indent_scope();
            auto do_full_name = [&](const StructField* field, std::string_view prefix, bool root) {
                auto full_name = std::format("{}{}", prefix, field->name);
                futils::helper::DynDefer if_scope;
                if (field->attr & PTR && !root) {
                    full_name = full_name + "()";
                    src.writeln("if (auto ptr = ", full_name, ") {");
                    if_scope = src.indent_scope_ex();
                    full_name = "(*ptr)";
                }
                src.writeln("if (!is_nil(", full_name, ")) {");
                {
                    auto inner_scope = src.indent_scope();
                    auto result_name = std::format("result_{}", field->name);
                    src.writeln("auto ", result_name, " = visit_Object<Result>(std::forward<UserContext>(ctx),", full_name, ");");
                    src.writeln("if (!", result_name, ") {");
                    src.indent_writeln("return unexpect_error(std::move(", result_name, ".error()));");
                    src.writeln("}");
                }
                src.writeln("}");
                if (if_scope) {
                    if_scope.execute();
                    src.writeln("}");
                }
            };
            auto handle_recursive = [&](auto&& handle_recursive, const StructField* field, std::string_view prefix, bool root) -> void {
                if (field->type.ends_with("Ref")) {
                    auto removed_ref_type = field->type.substr(0, field->type.size() - 3);
                    auto body_name = std::format("{}Body", removed_ref_type);
                    if (auto found = struct_map.find(body_name); found == struct_map.end()) {
                        return;  // Weak or LoweredRef
                    }
                    do_full_name(field, prefix, root);
                    return;
                }
                if (field->type == "Block" || field->type == "Expressions" || field->type == "Types" ||
                    field->type == "Statement" || field->type == "Expression" || field->type == "Type") {
                    bool has_alias_ref = (field->type == "Statement" || field->type == "Expression" || field->type == "Type");
                    // invoke default dispatch
                    auto dispatch_fn = std::format("dispatch_{}_default", field->type);
                    auto full_name = std::format("{}{}", prefix, field->name);
                    futils::helper::DynDefer if_scope;
                    if (field->attr & PTR && !root) {
                        full_name = full_name + "()";
                        src.writeln("if (auto ptr = ", full_name, ") {");
                        if_scope = src.indent_scope_ex();
                        full_name = "(*ptr)";
                    }
                    src.write("auto result_", field->name, " = ", dispatch_fn, "<Result>(std::forward<UserContext>(ctx),", full_name);
                    if (has_alias_ref) {
                        src.write(",", prefix, "alias_ref");
                    }
                    src.writeln(");");
                    src.writeln("if (!result_", field->name, ") {");
                    src.indent_writeln("return unexpect_error(std::move(result_", field->name, ".error()));");
                    src.writeln("}");
                    if (if_scope) {
                        if_scope.execute();
                        src.writeln("}");
                    }
                    return;
                }
                if (auto found = struct_map.find(field->type); found != struct_map.end()) {
                    auto& struct_def = found->second;
                    futils::helper::DynDefer if_scope, array_scope;
                    std::string new_prefix = std::format("{}{}.", prefix, field->name);
                    if (field->attr & PTR && !root) {
                        src.writeln("if (auto ptr_", field->name, " = ", prefix, field->name, "()) {");
                        if_scope = src.indent_scope_ex();
                        new_prefix = std::format("(*ptr_{}).", field->name);
                    }
                    if (field->attr & ARRAY) {
                        auto i = std::format("i_{}", field->name);
                        src.writeln("for (size_t ", i, " = 0; ", i, " < ", new_prefix, "size(); ++", i, ") {");
                        array_scope = src.indent_scope_ex();
                        auto prefix_base = field->attr & PTR && !root ? std::format("(*ptr_{})", field->name) : std::format("{}{}", prefix, field->name);
                        new_prefix = std::format("{}[{}].", prefix_base, i);
                    }
                    for (auto& sub_field : struct_def.fields) {
                        handle_recursive(handle_recursive, &sub_field, new_prefix, false);
                    }
                    if (array_scope) {
                        array_scope.execute();
                        src.writeln("}");
                    }
                    if (if_scope) {
                        if_scope.execute();
                        src.writeln("}");
                    }
                }
            };
            for (auto& field : cls.fields) {
                const StructField* field_ptr = nullptr;
                StructField dummy_field{};
                if (field.name == "in") {
                    auto it = struct_map.find(cls.class_name());
                    if (it != struct_map.end()) {
                        dummy_field = StructField{
                            .name = field.name,
                            .type = it->first,
                            .attr = NONE,
                        };
                        field_ptr = &dummy_field;
                    }
                }
                else if (field.base) {
                    field_ptr = field.base;
                }
                if (!field_ptr) {
                    continue;
                }
                auto prefix = std::format("type_ctx.", field_ptr->name);
                handle_recursive(handle_recursive, field_ptr, prefix, true);
            }
            if (cls.has(ContextClassKind_Generic)) {
                src.writeln("return result_in;");  // default propagation
            }
            else {
                src.writeln("return {};");
            }
        }
        src.writeln("}");
    }

    void generate_generic_traversal_children(CodeWriter& hdr, CodeWriter& src, const std::vector<ContextClasses>& context_classes, const std::string_view result_type) {
        hdr.writeln("template<", template_param_result(true), ", typename UserContext, typename TypeContext>");
        hdr.writeln(result_type, " traverse_children(UserContext&& uctx, TypeContext&& type_ctx);");
        src.writeln("template<", template_param_result(false), ", typename UserContext, typename TypeContext>");
        src.writeln(result_type, " traverse_children(UserContext&& uctx, TypeContext&& type_ctx) {");
        {
            auto scope = src.indent_scope();
            src.writeln("using TypeContextType = std::decay_t<TypeContext>;");
            src.writeln("if constexpr (false) {}");
            for (auto& cls_group : context_classes) {
                std::string main_before_or_after;
                for (auto& cls : cls_group.classes) {
                    auto context_name = context_instance_name(cls);
                    if (!main_before_or_after.empty()) {
                        main_before_or_after += " || ";
                    }
                    main_before_or_after += std::format("std::is_same_v<TypeContextType,{}>", context_name);
                }
                src.writeln("else if constexpr (", main_before_or_after, ") {");
                auto& cls = cls_group.main();
                {
                    auto if_scope = src.indent_scope();
                    src.writeln("return traverse_children_", cls.class_name(), "<Result>(std::forward<UserContext>(uctx),std::forward<TypeContext>(type_ctx));");
                }
                src.writeln("}");
            }
            src.writeln("else {");
            {
                auto else_scope = src.indent_scope();
                src.writeln("static_assert(dependent_false<TypeContext>, \"traverse_children not implemented for this context type\");");
            }
            src.writeln("}");
        }
        src.writeln("}");
    }

    void generate(const IncludeLocations& locations, CodeWriter& hdr, CodeWriter& src, std::map<std::string_view, Struct>& structs) {
        auto result_type = "expected<Result>";
        bool ebmgen_mode = locations.ebmgen_mode;  // on ebmgen_mode, omit some needless features like user defined includes
        auto context_classes = generate_context_classes(structs);
        auto utility_classes = generate_utility_classes(ebmgen_mode);
        auto hooks = generate_hooks();
        auto ns_name = locations.ns_name;
        auto is_codegen = locations.is_codegen;
        IncludeInfo includes_info{.includes = locations, .hooks = hooks};

        generate_common_include_guard(hdr, true);
        if (!ebmgen_mode) {
            generate_user_include_before(hdr, includes_info);
        }
        generate_namespace(hdr, ns_name, true);
        if (!ebmgen_mode) {
            generate_undef_dummy_macros(src);
            generate_forward_declaration_source(src, ns_name, result_type, is_codegen);
            generate_user_implemented_includes(src, ns_name, hooks, locations, context_classes, result_type, utility_classes);
            generate_namespace(src, ns_name, true);
        }
        auto ns_scope_hdr = hdr.indent_scope();
        auto ns_scope_src = src.indent_scope();
        if (!ebmgen_mode) {
            generate_forward_declaration_header(hdr);
        }
        generate_namespace_injection(hdr);
        generate_Result(hdr, is_codegen, includes_info);
        if (!ebmgen_mode) {
            generate_Flags(hdr, includes_info);
            generate_Output(hdr, includes_info);
        }

        if (!ebmgen_mode) {
            for (size_t i = 0; i < hooks.size() - 1; i++) {
                generate_hook_tag(hdr, hooks[i]);
            }
            generate_hook_tag(src, hooks.back());  // hidden hook in source
        }
        // generate_always_false_template(hdr);
        // generate_get_visitor_from_context(hdr, result_type);
        generate_hijack_logic_macro(src);
        for (auto& cls_group : context_classes) {
            generate_dispatcher_function(hdr, src, cls_group, result_type);
            generate_generic_dispatch_default(hdr, cls_group.main(), result_type);
            generate_traversal_children_for_class(hdr, src, cls_group.main(), result_type, structs);
        }
        generate_generic_traversal_children(hdr, src, context_classes, result_type);
        generate_user_interface(hdr, context_classes, result_type);
        for (auto& cls_group : context_classes) {
            generate_list_dispatch_default(hdr, cls_group.main(), is_codegen, result_type);
        }
        generate_impl_getter_header(hdr);
        if (!ebmgen_mode) {
            generate_Contexts_forward_declaration(hdr, context_classes);
        }
        generate_BaseVisitor(hdr, includes_info, utility_classes["BaseVisitor"], ebmgen_mode);
        generate_adl_lookup_helper(hdr);
        generate_initial_context(hdr);

        generate_visitor_customization_point(hdr);
        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                generate_context_class(hdr, cls);
                if (!ebmgen_mode) {
                    generate_visitor_tag(hdr, cls);
                    deconstruct_context_fields_macro(hdr, ns_name, cls);
                    generate_generator_default_hook(src, hooks.back(), hooks.size() - 1, cls, is_codegen, result_type);
                }
            }
        }

        if (!ebmgen_mode) {
            generate_merged_visitor(src, hooks, context_classes, result_type, utility_classes);
            generate_impl_getter_source(src);

            generate_unimplemented_stub_def(src, result_type, is_codegen);
        }

        if (!ebmgen_mode) {
            generate_dummy_macros(hdr, ns_name, hooks.front(), context_classes);
        }

        ns_scope_hdr.execute();
        ns_scope_src.execute();
        if (!ebmgen_mode) {
            generate_namespace(hdr, ns_name, false);
        }
        generate_namespace(src, ns_name, false);
        if (!ebmgen_mode) {
            generate_user_include_after(hdr, includes_info);
        }
        if (ebmgen_mode) {
            generate_common_include_guard(src, false);
        }
        else {
            generate_common_include_guard(hdr, false);
        }
        if (!ebmgen_mode) {
            generate_entry_point(src, ns_name);
        }
    }

    void generate_hook_description(CodeWriter& w, const ParsedHookName& hook_name, const ContextClass& cls, UtilityClasses& utils, const std::map<std::string_view, Struct>& struct_map) {
        w.writeln("/*");
        {
            auto scope0 = w.indent_scope();
            w.writeln("Name: ", hook_name.original);
            w.writeln("Available Variables:");
            auto print_struct = [&](auto&& print_struct, std::string_view type) -> void {
                if (auto found = struct_map.find(type); found != struct_map.end()) {
                    auto nest = w.indent_scope();
                    for (auto& field : found->second.fields) {
                        w.write(field.name, ": ");
                        if (field.attr & ebmcodegen::TypeAttribute::PTR) {
                            w.write("*");
                        }
                        if (field.attr & ebmcodegen::TypeAttribute::ARRAY) {
                            w.write("std::vector<", field.type, ">");
                        }
                        else {
                            w.write(field.type);
                        }
                        w.writeln();
                        if (field.type != "Varint" && !field.type.ends_with("Ref")) {
                            print_struct(print_struct, field.type);
                        }
                    }
                }
            };
            {
                auto scope = w.indent_scope();
                w.writeln("ctx: ", context_instance_name(cls));
                {
                    auto scope2 = w.indent_scope();
                    for (auto& field : cls.fields) {
                        if (field.name == "visitor") {
                            w.writeln("visitor: MergedVisitor&");
                            {
                                auto scope3 = w.indent_scope();
                                auto& base_visitor = utils["BaseVisitor"];
                                for (auto& field : base_visitor.fields) {
                                    if (field.name == legacy_compat_ptr_name) {
                                        continue;
                                    }
                                    w.writeln(field.name, ": ", field.type);
                                }
                            }
                            continue;
                        }
                        w.writeln(field.name, ": ", field.type);
                        if (field.base) {
                            print_struct(print_struct, field.base->type);
                        }
                    }
                }
            }
        }
        w.writeln("*/");
    }

    std::vector<std::string> class_based_hook_names(const std::map<std::string_view, Struct>& structs, const IncludeLocations& locations) {
        std::vector<std::string> names;
        auto context_classes = generate_context_classes(structs);
        auto hooks = generate_hooks();
        assert(hooks.size() - 1 == locations.include_locations.size());
        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                for (auto& loc : locations.include_locations) {
                    names.push_back(header_name(loc, cls));
                }
            }
        }
        UserConfigIncludes incs;
        incs.foreach ([&](auto&& v) {
            names.push_back(std::string(v));
            // before/after
            names.push_back(std::format("{}{}", v, suffixes[suffix_before]));
            names.push_back(std::format("{}{}", v, suffixes[suffix_after]));
        });
        return names;
    }

    ebmgen::expected<void> class_based_hook_descriptions(CodeWriter& w, const ParsedHookName& hook_name, const std::map<std::string_view, Struct>& structs) {
        auto context_classes = generate_context_classes(structs);
        auto utility_classes = generate_utility_classes(false);
        auto hooks = generate_hooks();

        for (auto& cls_group : context_classes) {
            for (auto& cls : cls_group.classes) {
                if (hook_name.target == cls.base && hook_name.kind == cls.variant ||
                    (hook_name.target == "Block" && cls.base == "Statement" && cls.has(ContextClassKind_List) ||
                     hook_name.target == "Expressions" && cls.base == "Expression" && cls.has(ContextClassKind_List) ||
                     hook_name.target == "Types" && cls.base == "Type" && cls.has(ContextClassKind_List))) {
                    auto before_after = cls.is_before_after();
                    if ((before_after == ContextClassKind_Before && hook_name.include_location == suffixes[suffix_before]) ||
                        (before_after == ContextClassKind_After && hook_name.include_location == suffixes[suffix_after]) ||
                        (before_after == ContextClassKind_Normal && hook_name.include_location.empty())) {
                        generate_hook_description(w, hook_name, cls, utility_classes, structs);
                        return {};
                    }
                }
            }
        }
        return futils::helper::either::unexpected(futils::error::StrError<std::string>{std::format("No matching class found for hook description: {}", hook_name.original)});
    }
}  // namespace ebmcodegen
