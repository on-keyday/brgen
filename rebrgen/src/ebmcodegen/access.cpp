/*license*/

#include <format>
#include <string_view>
#include "stub/structs.hpp"
namespace ebmcodegen {
    auto adjust_name(std::string_view name) {
        if (!name.starts_with("std::") && name != "bool") {
            return "ebm::" + std::string(name);
        }
        return std::string(name);
    }

    void generate_constexpr_access_helper(CodeWriter& w, const Struct& s, size_t type_index, std::map<std::string_view, size_t>& field_indices) {
        auto name = adjust_name(s.name);

        w.writeln("template<>");
        w.writeln("constexpr size_t get_type_index<", name, ">() {");
        w.writeln("    return " + std::to_string(type_index) + ";");
        w.writeln("}");

        w.writeln("template<>");
        w.writeln("constexpr size_t get_field_index<", std::to_string(type_index), ">(std::string_view field_name) {");
        {
            auto scope = w.indent_scope();
            for (size_t i = 0; i < s.fields.size(); i++) {
                auto& field = s.fields[i];
                w.writeln("if (field_name == \"", field.name, "\") {");
                {
                    auto if_scope = w.indent_scope();
                    auto field_index = field_indices[field.name];
                    w.writeln("return ", std::to_string(field_index), ";");
                }
                w.writeln("}");
            }
            w.writeln("if (std::is_constant_evaluated()) {");
            {
                auto if_scope = w.indent_scope();
                w.writeln("throw \"No such field\";");
            }
            w.writeln("}");
            w.writeln("return static_cast<size_t>(-1); // to avoid compile error");
        }
        w.writeln("}");
    }

    void generate_constexpr_access(CodeWriter& w, const std::map<std::string_view, Struct>& struct_map, const std::map<std::string_view, Enum>& enum_map) {
        std::map<std::string_view, size_t> type_indices;
        std::map<std::string_view, size_t> field_indices;
        for (auto& [name, s] : struct_map) {
            type_indices[name] = type_indices.size();
            for (auto& field : s.fields) {
                if (!field_indices.contains(field.name)) {
                    field_indices[field.name] = field_indices.size();
                }
            }
        }
        std::vector<std::string_view> additional_types = {
            "StatementRef",
            "TypeRef",
            "ExpressionRef",
            "IdentifierRef",
            "StringRef",
            "AnyRef",
            "Varint",
            "bool",
            "std::uint8_t",
        };
        for (auto& name : additional_types) {
            type_indices[name] = type_indices.size();
        }
        for (auto& [name, e] : enum_map) {
            type_indices[name] = type_indices.size();
        }
        w.writeln("#pragma once");
        w.writeln("#include <ebm/extended_binary_module.hpp>");
        w.writeln("namespace ebmcodegen {");
        {
            auto scope = w.indent_scope();
            w.writeln("struct TypeIndex {");
            {
                auto scope = w.indent_scope();
                w.writeln("size_t index = 0;");
                w.writeln("bool is_array = false;");
                w.writeln("bool is_ptr = false;");
            }
            w.writeln("};");

            w.writeln("template<class T>");
            w.writeln("constexpr size_t get_type_index();");
            w.writeln();
            w.writeln("template<size_t TypeIndex>");
            w.writeln("constexpr size_t get_field_index(std::string_view field_name);");
            w.writeln();
            w.writeln("template<size_t FieldIndex>");
            w.writeln("constexpr decltype(auto) get_field(auto&& in){");
            {
                auto scope = w.indent_scope();
                w.writeln("using T = std::decay_t<decltype(in)>;");
                w.writeln("if constexpr (false) {}");
                for (auto& [name, s] : struct_map) {
                    w.writeln("else if constexpr (std::is_same_v<T, ", adjust_name(s.name), ">) {");
                    {
                        auto if_scope = w.indent_scope();

                        w.writeln("if constexpr (false) {}");
                        for (size_t i = 0; i < s.fields.size(); i++) {
                            auto& field = s.fields[i];
                            auto field_index = field_indices[field.name];
                            w.writeln("else if constexpr (FieldIndex == ", std::to_string(field_index), ") {");
                            {
                                auto scope = w.indent_scope();
                                if (field.attr & TypeAttribute::PTR || field.attr & TypeAttribute::RVALUE) {
                                    w.writeln("return in.", field.name, "();");
                                }
                                else {
                                    w.writeln("auto& ref = in.", field.name, ";");
                                    w.writeln("return ref;");
                                }
                            }
                            w.writeln("}");
                        }
                    }
                    w.writeln("}");
                    w.writeln("else if constexpr (std::is_same_v<T,", adjust_name(s.name), "*> || std::is_same_v<T,const ", adjust_name(s.name), "*>) {");
                    {
                        auto if_scope = w.indent_scope();

                        w.writeln("if constexpr (false) {}");
                        for (size_t i = 0; i < s.fields.size(); i++) {
                            auto& field = s.fields[i];
                            auto field_index = field_indices[field.name];
                            w.writeln("else if constexpr (FieldIndex == ", std::to_string(field_index), ") {");
                            {
                                auto scope = w.indent_scope();

                                if (field.attr & TypeAttribute::PTR) {
                                    w.writeln("if (!in) {");
                                    {
                                        auto if_scope = w.indent_scope();
                                        w.writeln("return decltype(in->", field.name, "())();");
                                    }
                                    w.writeln("}");
                                    w.writeln("return in->", field.name, "();");
                                }
                                else if (field.attr & TypeAttribute::RVALUE) {
                                    w.writeln("if (!in) {");
                                    {
                                        auto if_scope = w.indent_scope();
                                        w.writeln("return std::optional<decltype(in->", field.name, "())>{};");
                                    }
                                    w.writeln("}");
                                    w.writeln("return std::optional<decltype(in->", field.name, "())>(in->", field.name, "());");
                                }
                                else {
                                    w.writeln("if (!in) {");
                                    {
                                        auto if_scope = w.indent_scope();
                                        w.writeln("return decltype(std::addressof(in->", field.name, "))();");
                                    }
                                    w.writeln("}");
                                    w.writeln("return std::addressof(in->", field.name, ");");
                                }
                            }
                            w.writeln("}");
                        }
                    }
                    w.writeln("}");
                }
                w.writeln("else {");
                {
                    auto else_scope = w.indent_scope();
                    w.writeln("static_assert(!std::is_same_v<T,T>, \"No such type for field access\");");
                }
                w.writeln("}");
            }
            w.writeln("}");
            w.writeln();

            for (auto& [name, s] : struct_map) {
                generate_constexpr_access_helper(w, s, type_indices[name], field_indices);
                w.writeln();
            }
            for (auto& name : additional_types) {
                Struct s;
                s.name = name;
                generate_constexpr_access_helper(w, s, type_indices[name], field_indices);
                w.writeln();
            }
            for (auto& [name, e] : enum_map) {
                Struct s;
                s.name = name;
                generate_constexpr_access_helper(w, s, type_indices[name], field_indices);
                w.writeln();
            }

            w.writeln("template<TypeIndex index>");
            w.writeln("constexpr decltype(auto) type_from_index() {");
            {
                auto scope = w.indent_scope();
                w.writeln("if constexpr (false) {}");
                auto get_from_index = [&](std::string_view in_name) {
                    auto name = adjust_name(in_name);
                    auto if_ptr = [&](std::string_view n) {
                        w.writeln("if constexpr (index.is_ptr) {");
                        {
                            auto scope = w.indent_scope();
                            w.writeln("return (", n, "*)nullptr;");
                        }
                        w.writeln("}");
                        w.writeln("else {");
                        {
                            auto scope = w.indent_scope();
                            w.writeln("return ", n, "{};");
                        }
                        w.writeln("}");
                    };
                    w.writeln("if constexpr (index.is_array) {");
                    {
                        auto scope = w.indent_scope();
                        if_ptr(std::format("std::vector<{}>", name));
                    }
                    w.writeln("}");
                    w.writeln("else {");
                    {
                        auto scope = w.indent_scope();
                        if_ptr(name);
                    }
                    w.writeln("}");
                };
                for (auto& [name, s] : struct_map) {
                    w.writeln("else if constexpr (index.index == ", std::to_string(type_indices[name]), ") {");
                    {
                        auto if_scope = w.indent_scope();
                        get_from_index(name);
                    }
                    w.writeln("}");
                }
                for (auto& name : additional_types) {
                    w.writeln("else if constexpr (index.index == ", std::to_string(type_indices[name]), ") {");
                    {
                        auto if_scope = w.indent_scope();
                        get_from_index(name);
                    }
                    w.writeln("}");
                }
                for (auto& [name, e] : enum_map) {
                    w.writeln("else if constexpr (index.index == ", std::to_string(type_indices[name]), ") {");
                    {
                        auto if_scope = w.indent_scope();
                        get_from_index(name);
                    }
                    w.writeln("}");
                }
                w.writeln("else {");
                {
                    auto else_scope = w.indent_scope();
                    w.writeln("throw \"No such type index\";");
                }
                w.writeln("}");
            }
            w.writeln("}");

            w.writeln("constexpr size_t get_field_index(std::string_view name) {");
            {
                auto scope = w.indent_scope();
                for (auto& [name, idx] : field_indices) {
                    w.writeln("if (name == \"", name, "\") {");
                    {
                        auto if_scope = w.indent_scope();
                        w.writeln("return ", std::to_string(idx), ";");
                    }
                    w.writeln("}");
                }
                w.writeln("return -1;");
            }
            w.writeln("}");

            w.writeln();

            w.writeln("constexpr std::string_view field_name_from_index(size_t index) {");
            {
                auto scope = w.indent_scope();
                w.writeln("switch (index) {");
                for (auto& [name, idx] : field_indices) {
                    w.writeln("case ", std::to_string(idx), ": return \"", name, "\";");
                }
                w.writeln("default: return \"\";");
                w.writeln("}");
            }
            w.writeln("}");

            w.writeln();

            w.writeln("constexpr std::string_view type_name_from_index(size_t index) {");
            {
                auto scope = w.indent_scope();
                w.writeln("switch (index) {");
                for (auto& [name, idx] : type_indices) {
                    w.writeln("case ", std::to_string(idx), ": return \"", name, "\";");
                }
                w.writeln("default: return \"\";");
                w.writeln("}");
            }
            w.writeln("}");

            w.writeln("constexpr size_t get_field_index(size_t type_index, std::string_view field_name) {");
            {
                auto scope = w.indent_scope();
                w.writeln("switch (type_index) {");
                for (auto& [name, s] : struct_map) {
                    w.writeln("case ", std::to_string(type_indices[name]), ": {");
                    {
                        auto case_scope = w.indent_scope();
                        w.writeln("return get_field_index<", std::to_string(type_indices[name]), ">(field_name);");
                    }
                    w.writeln("}");
                }
                w.writeln("default:");
                {
                    auto default_scope = w.indent_scope();
                    w.writeln("return -1;");
                }
                w.writeln("}");
            }
            w.writeln("}");

            w.writeln("constexpr size_t get_type_index(std::string_view type_name) {");
            {
                auto scope = w.indent_scope();
                for (auto& [name, s] : struct_map) {
                    w.writeln("if (type_name == \"", name, "\") {");
                    {
                        auto if_scope = w.indent_scope();
                        w.writeln("return ", std::to_string(type_indices[name]), ";");
                    }
                    w.writeln("}");
                }
                w.writeln("throw \"No such type name\";");
            }
            w.writeln("}");

            w.writeln("constexpr TypeIndex get_type_index_from_field(size_t type_index,size_t field_index) {");
            {
                auto scope = w.indent_scope();
                w.writeln("switch (type_index) {");
                for (auto& [name, s] : struct_map) {
                    w.writeln("case ", std::to_string(type_indices[name]), ": {");
                    {
                        auto case_scope = w.indent_scope();
                        for (auto& field : s.fields) {
                            w.writeln("if (field_index == get_field_index<", std::to_string(type_indices[name]), ">(\"", field.name, "\")) {");
                            {
                                auto if_scope = w.indent_scope();
                                auto found = type_indices.find(field.type);
                                if (found != type_indices.end()) {
                                    w.writeln("return ", "{.index = ", std::to_string(found->second), ", .is_array = ", (field.attr & TypeAttribute::ARRAY ? "true" : "false"), ", .is_ptr = ", (field.attr & TypeAttribute::PTR ? "true" : "false"), "};");
                                }
                                else {
                                    w.writeln("return {.index = static_cast<size_t>(-1), .is_array = ", (field.attr & TypeAttribute::ARRAY ? "true" : "false"), ", .is_ptr = ", (field.attr & TypeAttribute::PTR ? "true" : "false"), "};");
                                }
                            }
                            w.writeln("}");
                        }
                        w.writeln("return {.index = static_cast<size_t>(-1), .is_array = false, .is_ptr = false};");
                    }
                    w.writeln("}");
                }
                w.writeln("default:");
                {
                    auto default_scope = w.indent_scope();
                    w.writeln("return {.index = static_cast<size_t>(-1), .is_array = false, .is_ptr = false};");
                }
                w.writeln("}");
            }
            w.writeln("}");
        }
        w.writeln("} // namespace ebmcodegen");
    }
}  // namespace ebmcodegen