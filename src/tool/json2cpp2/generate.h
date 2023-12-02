/*license*/
#pragma once

#include "cpp_type.h"
#include <helper/expected.h>
#include <core/ast/ast.h>
#include <core/writer/writer.h>
#include <core/ast/tool/stringer.h>

namespace j2cp2 {

    struct BitFieldMeta {
        std::string field_name;
        std::vector<std::shared_ptr<ast::Field>> fields;
    };

    struct AnonymousStructMeta {
        std::string field_name;
        std::shared_ptr<ast::StructType> struct_;
    };
    struct Generator {
        brgen::writer::Writer w;
        size_t seq = 0;
        ast::tool::Stringer str;
        std::map<ast::Field*, BitFieldMeta> bit_fields;
        std::map<ast::Field*, AnonymousStructMeta> anonymous_structs;
        void write_bit_fields(std::vector<std::shared_ptr<ast::Field>>& non_align, size_t bit_size, bool is_int_set, bool include_non_simple) {
            if (is_int_set && !include_non_simple) {
                w.write("::utils::binary::flags_t<std::uint", brgen::nums(bit_size), "_t");
                for (auto& n : non_align) {
                    auto type = n->field_type;
                    if (auto ident = ast::as<ast::IdentType>(type); ident) {
                        type = ident->base.lock();
                    }
                    if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                        w.write(", ", brgen::nums(int_ty->bit_size));
                    }
                    else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                        auto bit_size = enum_ty->base.lock()->base_type->bit_size;
                        w.write(", ", brgen::nums(bit_size));
                    }
                }
                w.writeln("> flags_", brgen::nums(seq), "_;");
                size_t i = 0;
                for (auto& n : non_align) {
                    auto type = n->field_type;
                    if (auto ident = ast::as<ast::IdentType>(type); ident) {
                        type = ident->base.lock();
                    }
                    if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                        w.writeln("bits_flag_alias_method(flags_", brgen::nums(seq), "_,", brgen::nums(i), ",", n->ident->ident, ");");
                        str.ident_map[n->ident->ident] = n->ident->ident + "()";
                    }
                    else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                        auto enum_ = enum_ty->base.lock();
                        w.writeln("bits_flag_alias_method_with_enum(flags_", brgen::nums(seq), "_,", brgen::nums(i), ",", n->ident->ident, ",", enum_->ident->ident, ");");
                        str.ident_map[n->ident->ident] = n->ident->ident + "()";
                    }
                    i++;
                }
                bit_fields[non_align.back().get()] = {brgen::concat("flags_", brgen::nums(seq), "_"), non_align};
                seq++;
            }
        }

        std::string get_type_name(const std::shared_ptr<ast::Type>& type) {
            if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                return brgen::concat("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(int_ty->bit_size), "_t");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                return enum_ty->base.lock()->ident->ident;
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                if (arr_ty->is_int_set) {
                    auto len = str.to_string(arr_ty->length);
                    return "std::array<std::uint" + brgen::nums(arr_ty->base_type->bit_size) + "_t," + len + ">";
                }
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->base_type); int_ty && int_ty->bit_size == 8) {
                    return "::utils::view::rvec";
                }
                return "std::vector<" + get_type_name(arr_ty->base_type) + ">";
            }
            return "";
        }

        void write_struct_type(const std::shared_ptr<ast::StructType>& s) {
            auto member = ast::as<ast::Member>(s->base.lock());
            bool has_ident = member && member->node_type != ast::NodeType::field && member->ident;
            w.writeln("struct ",
                      has_ident
                          ? member->ident->ident + " "
                          : "",
                      "{");
            {
                auto indent = w.indent_scope();
                bool is_int_set = true;
                bool include_non_simple = false;
                size_t bit_size = 0;
                std::vector<std::shared_ptr<ast::Field>> non_aligned;
                auto is_simple_type = [&](const std::shared_ptr<ast::Type>& type) {
                    return ast::as<ast::IntType>(type) != nullptr || ast::as<ast::EnumType>(type) != nullptr;
                };
                for (auto& field : s->fields) {
                    if (auto f = ast::as<ast::Field>(field); f) {
                        auto type = f->field_type;
                        if (auto ident = ast::as<ast::IdentType>(type); ident) {
                            type = ident->base.lock();
                        }
                        if (f->bit_alignment == ast::BitAlignment::not_target) {
                            continue;
                        }
                        if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                            is_int_set = is_int_set && type->is_int_set;
                            include_non_simple = include_non_simple || !is_simple_type(type);
                            bit_size += type->bit_size;
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            continue;
                        }
                        if (non_aligned.size() > 0) {
                            is_int_set = is_int_set && type->is_int_set;
                            include_non_simple = include_non_simple || !is_simple_type(type);
                            bit_size += type->bit_size;
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            write_bit_fields(non_aligned, bit_size, is_int_set, include_non_simple);
                            non_aligned.clear();
                            is_int_set = true;
                            include_non_simple = false;
                            bit_size = 0;
                            continue;
                        }
                        if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                            if (int_ty->is_common_supported) {
                                w.writeln("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(int_ty->bit_size), "_t ", f->ident->ident, ";");
                            }
                        }
                        if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                            auto enum_ = enum_ty->base.lock();
                            auto bit_size = enum_->base_type->bit_size;
                            w.writeln("std::uint", brgen::nums(bit_size), "_t ", f->ident->ident, "_data;");
                            w.writeln(enum_->ident->ident, " ", f->ident->ident, "() const { return static_cast<", enum_->ident->ident, ">(this->", f->ident->ident, "_data); }");
                            str.ident_map[f->ident->ident] = f->ident->ident + "()";
                        }
                        if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                            auto ty = get_type_name(type);
                            w.writeln(ty, " ", f->ident->ident, ";");
                        }
                        if (auto union_ty = ast::as<ast::StructUnionType>(type)) {
                            std::map<std::shared_ptr<ast::StructType>, std::string> tmp;
                            w.writeln("union {");
                            std::string prefix = "union_struct_";
                            {
                                auto indent = w.indent_scope();
                                for (auto& f : union_ty->structs) {
                                    write_struct_type(f);
                                    auto& t = tmp[f];
                                    t = brgen::concat(prefix, brgen::nums(seq));
                                    seq++;
                                    w.writeln(" ", t, ";");
                                }
                            }
                            w.writeln("};");
                            for (auto& f : union_ty->union_fields) {
                                auto uf = f.lock();
                                auto ut = ast::as<ast::UnionType>(uf->field_type);
                                assert(ut);
                                auto c = ut->cond.lock();
                                std::string cond_u;
                                if (c) {
                                    cond_u = str.to_string(c);
                                }
                                else {
                                    cond_u = "true";
                                }
                                if (ut->common_type) {
                                    w.writeln("std::optional<", get_type_name(ut->common_type), "> ", uf->ident->ident, "() const {");
                                    {
                                        auto indent = w.indent_scope();
                                        auto make_access = [&](const std::shared_ptr<ast::Field>& f) {
                                            auto struct_ = f->belong_struct.lock();
                                            assert(struct_);
                                            auto indent = w.indent_scope();
                                            auto access = tmp[struct_] + "." + f->ident->ident;
                                            anonymous_structs[f.get()] = {access, struct_};
                                            w.writeln("return this->", access, ";");
                                        };
                                        bool has_els = false;
                                        for (auto& c : ut->candidates) {
                                            auto cond = c->cond.lock();
                                            if (cond) {
                                                auto cond_s = str.to_string(cond);
                                                w.writeln("if (", cond_s, "==", cond_u, ") {");
                                                {
                                                    auto f = c->field.lock();
                                                    if (!f) {
                                                        w.writeln("return std::nullopt;");
                                                    }
                                                    else {
                                                        make_access(f);
                                                    }
                                                }
                                                w.writeln("}");
                                            }
                                            else {
                                                auto f = c->field.lock();
                                                if (!f) {
                                                    w.writeln("return std::nullopt;");
                                                }
                                                else {
                                                    make_access(f);
                                                }
                                            }
                                        }
                                        w.writeln("return std::nullopt;");
                                        str.ident_map[uf->ident->ident] = "*" + uf->ident->ident + "()";
                                    }
                                    w.writeln("}");
                                }
                            }
                        }
                    }
                }
                if (has_ident) {
                    w.writeln("bool encode(::utils::binary::writer& w) const ;");
                    w.writeln("bool decode(::utils::binary::reader& r);");
                }
            }
            w.write("}");
        }

        void write_simple_struct(const std::shared_ptr<ast::Format>& fmt) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            write_struct_type(fmt->body->struct_type);
            w.writeln(";");
        }

        void write_enum(const std::shared_ptr<ast::Enum>& enum_) {
            w.write("enum class ", enum_->ident->ident);
            w.writeln(" {");
            {
                auto indent = w.indent_scope();
                for (auto& c : enum_->members) {
                    w.write(c->ident->ident);
                    if (c->expr) {
                        ast::tool::Stringer s;
                        w.write(" = ", s.to_string(c->expr));
                    }
                    w.writeln(",");
                }
            }
            w.writeln("};");
        }

        void code_if(ast::If* if_, bool encode) {
            auto cond = if_->cond;
            auto cond_s = str.to_string(cond);
            w.writeln("if (", cond_s, ") {");
            {
                auto indent = w.indent_scope();
                code_indent_block(if_->then, encode);
            }
            w.writeln("}");
            auto elif_ = if_;
            while (elif_) {
                if (auto if_ = ast::as<ast::If>(elif_->els); if_) {
                    cond_s = str.to_string(if_->cond);
                    w.writeln("else if (", cond_s, ") {");
                    {
                        auto indent = w.indent_scope();
                        code_indent_block(if_->then, encode);
                    }
                    w.writeln("}");
                    elif_ = if_;
                    continue;
                }
                if (auto b = ast::as<ast::IndentBlock>(elif_->els)) {
                    w.writeln("else {");
                    {
                        auto indent = w.indent_scope();
                        code_indent_block(ast::cast_to<ast::IndentBlock>(elif_->els), encode);
                    }
                    w.writeln("}");
                    break;
                }
                break;
            }
        }

        void code_match(ast::Match* m, bool encode) {
            std::string cond_s;
            if (m->cond) {
                cond_s = str.to_string(m->cond);
            }
            else {
                cond_s = "true";
            }
            bool has_case = false;
            for (auto& b : m->branch) {
                if (auto br = ast::as<ast::MatchBranch>(b)) {
                    auto cond = str.to_string(br->cond);
                    if (has_case) {
                        w.write("else ");
                    }
                    w.writeln("if (", cond, "==", cond_s, ") {");
                    {
                        auto indent = w.indent_scope();
                        encode_one_node(br->then, encode);
                    }
                    w.writeln("}");
                    has_case = true;
                }
            }
        }

        void write_field_code(ast::Field* f, bool encode) {
            if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;
            }
            auto ident_from_field = [&] {
                auto ident = f->ident->ident;
                if (auto anon = anonymous_structs.find(f); anon != anonymous_structs.end()) {
                    ident = anon->second.field_name;
                }
                return ident;
            };
            if (encode) {
                if (auto found = bit_fields.find(f); found != bit_fields.end()) {
                    auto& meta = found->second;
                    auto& fields = meta.fields;
                    auto& field_name = meta.field_name;
                    w.writeln("if (!::utils::binary::write_num(w,", field_name, ".as_value() ,true)) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
                    }
                    w.writeln("}");
                    return;
                }
                if (auto int_ty = ast::as<ast::IntType>(f->field_type); int_ty) {
                    auto bit_size = int_ty->bit_size;
                    auto ident = ident_from_field();
                    auto type_name = get_type_name(f->field_type);
                    w.writeln("if (!::utils::binary::write_num(w,static_cast<", type_name, ">(", ident, ") ,", int_ty->endian == ast::Endian::little ? "false" : "true", ")) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
                    }
                    w.writeln("}");
                }
                if (auto arr_ty = ast::as<ast::ArrayType>(f->field_type); arr_ty) {
                    auto typ = get_type_name(f->field_type);
                    if (typ == "::utils::view::rvec") {
                        auto ident = ident_from_field();
                        auto len = str.to_string(arr_ty->length);
                        w.writeln("if (", len, "!=", ident, ".size()) {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                        w.writeln("if (!w.write(", ident, ")) {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                    }
                    if (arr_ty->length->constant_level == ast::ConstantLevel::constant && arr_ty->base_type->bit_size == 8) {
                        auto ident = ident_from_field();
                        w.writeln("if (!w.write(", ident, ")) {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                    }
                }
            }
            else {
                if (auto found = bit_fields.find(f); found != bit_fields.end()) {
                    auto& meta = found->second;
                    auto& fields = meta.fields;
                    auto& field_name = meta.field_name;
                    w.writeln("if (!::utils::binary::read_num(r,", field_name, ".as_value() ,true)) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
                    }
                    w.writeln("}");
                    return;
                }
                if (auto int_ty = ast::as<ast::IntType>(f->field_type); int_ty) {
                    auto bit_size = int_ty->bit_size;
                    auto ident = ident_from_field();
                    auto type_name = get_type_name(f->field_type);
                    w.writeln("if (!::utils::binary::read_num(r,", ident, " ,", int_ty->endian == ast::Endian::little ? "false" : "true", ")) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
                    }
                    w.writeln("}");
                }
                if (auto arr_ty = ast::as<ast::ArrayType>(f->field_type); arr_ty) {
                    auto typ = get_type_name(f->field_type);
                    auto ident = ident_from_field();
                    if (typ == "::utils::view::rvec") {
                        auto len = str.to_string(arr_ty->length);
                        w.writeln("if (!r.read(", ident, ", ", len, ")) {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                    }
                    if (arr_ty->length->constant_level == ast::ConstantLevel::constant && arr_ty->base_type->bit_size == 8) {
                        w.writeln("if (!r.read(", ident, ")) {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                    }
                }
            }
        }

        void encode_one_node(const std::shared_ptr<ast::Node>& elem, bool encode) {
            if (auto f = ast::as<ast::Field>(elem)) {
                write_field_code(f, encode);
            }
            if (auto if_ = ast::as<ast::If>(elem)) {
                code_if(if_, encode);
            }
            if (auto block = ast::as<ast::IndentBlock>(elem)) {
                code_indent_block(ast::cast_to<ast::IndentBlock>(elem), encode);
            }
            if (auto match = ast::as<ast::Match>(elem)) {
                code_match(match, encode);
            }
        }

        void code_indent_block(const std::shared_ptr<ast::IndentBlock>& block, bool encode) {
            // encode fields
            for (auto& elem : block->elements) {
                encode_one_node(elem, encode);
            }
        }

        void write_code_fn(const std::shared_ptr<ast::Format>& fmt, bool encode) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            w.writeln("inline bool ", fmt->ident->ident, "::", encode ? "encode(::utils::binary::writer& w) const" : "decode(::utils::binary::reader& r)", " {");
            {
                auto indent = w.indent_scope();
                // first, apply assertions
                for (auto& elem : fmt->body->elements) {
                    if (auto a = ast::as<ast::Assert>(elem)) {
                        auto cond = str.to_string(a->cond);
                        w.writeln("if (!", cond, ") {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                    }
                }
                // encode fields
                code_indent_block(fmt->body, encode);
                w.writeln("return true;");
            }
            w.writeln("}");
        }

        void write_program(std::shared_ptr<ast::Program>& prog) {
            w.writeln("//Code generated by json2cpp2");
            w.writeln("#pragma once");
            w.writeln("#include <cstdint>");
            w.writeln("#include <binary/flags.h>");
            w.writeln("#include <vector>");
            w.writeln("#include <array>");
            w.writeln("#include <view/iovec.h>");
            w.writeln("#include <optional>");
            w.writeln("#include <binary/number.h>");
            for (auto& fmt : prog->elements) {
                if (auto f = ast::as<ast::Format>(fmt); f) {
                    write_simple_struct(ast::cast_to<ast::Format>(fmt));
                    write_code_fn(ast::cast_to<ast::Format>(fmt), true);
                    write_code_fn(ast::cast_to<ast::Format>(fmt), false);
                }
                if (auto e = ast::as<ast::Enum>(fmt); e) {
                    write_enum(ast::cast_to<ast::Enum>(fmt));
                }
            }
        }
    };
}  // namespace j2cp2
