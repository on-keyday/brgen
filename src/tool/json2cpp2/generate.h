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
    struct Generator {
        brgen::writer::Writer w;
        size_t seq = 0;
        ast::tool::Stringer str;
        std::map<ast::Field*, BitFieldMeta> bit_fields;
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
            w.writeln("struct ",
                      member && member->node_type != ast::NodeType::field && member->ident
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
                                        for (auto& c : ut->candidates) {
                                            auto f = c->field.lock();
                                            auto struct_ = f->belong_struct.lock();
                                            assert(struct_);
                                            auto cond = c->cond.lock();
                                            if (cond) {
                                                auto cond_s = str.to_string(cond);
                                                w.writeln("if (", cond_s, "==", cond_u, ") {");
                                                {
                                                    auto indent = w.indent_scope();
                                                    w.writeln("return this->", tmp[struct_], ".", f->ident->ident, ";");
                                                    // w.writeln("return this->", s->ident->ident, ".", tmp[s], ";");
                                                }
                                                w.writeln("}");
                                            }
                                        }
                                        w.writeln("return std::nullopt;");
                                    }
                                    w.writeln("}");
                                }
                            }
                        }
                    }
                }
                w.writeln("bool encode(::utils::binary::writer& w,int* state = nullptr);");
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

        void encode_if(ast::If* if_) {
            auto cond = if_->cond;
            auto cond_s = str.to_string(cond);
            w.writeln("if (", cond_s, ") {");
            {
                auto indent = w.indent_scope();
                encode_indent_block(if_->then);
            }
            w.writeln("}");
            auto elif_ = if_;
            while (elif_) {
                if (auto if_ = ast::as<ast::If>(elif_->els); if_) {
                    cond_s = str.to_string(if_->cond);
                    w.writeln("else if (", cond_s, ") {");
                    {
                        auto indent = w.indent_scope();
                        encode_indent_block(if_->then);
                    }
                    w.writeln("}");
                    elif_ = if_;
                    continue;
                }
                auto block = ast::cast_to<ast::IndentBlock>(elif_->els);
                w.writeln("else {");
                {
                    auto indent = w.indent_scope();
                    encode_indent_block(block);
                }
                w.writeln("}");
                break;
            }
        }

        void encode_indent_block(const std::shared_ptr<ast::IndentBlock>& block) {
            size_t state = 1;
            bool has_non_aligned = false;
            // encode fields
            for (auto& elem : block->elements) {
                if (auto f = ast::as<ast::Field>(elem)) {
                    if (f->bit_alignment == ast::BitAlignment::not_target) {
                        continue;
                    }
                    if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                        has_non_aligned = true;
                        continue;
                    }
                    if (has_non_aligned) {
                        if (auto found = bit_fields.find(f); found != bit_fields.end()) {
                            auto& meta = found->second;
                            auto& fields = meta.fields;
                            auto& field_name = meta.field_name;
                            w.writeln("if (*state == ", brgen::nums(state), ") {");
                            {
                                auto indent = w.indent_scope();
                                w.writeln("if (!::utils::binary::write_num(w,", field_name, ".as_value() ,true)) {");
                                {
                                    auto indent = w.indent_scope();
                                    w.writeln("return false;");
                                }
                                w.writeln("}");
                                w.writeln("*state += 1;");
                            }
                            w.writeln("}");
                            state++;
                        }
                        has_non_aligned = false;
                        continue;
                    }
                    if (auto int_ty = ast::as<ast::IntType>(f->field_type); int_ty) {
                        auto bit_size = int_ty->bit_size;
                        auto ident = f->ident->ident;
                        w.writeln("if (*state == ", brgen::nums(state), ") {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("if (!::utils::binary::write_num(w,", ident, ",", int_ty->endian == ast::Endian::little ? "false" : "true", ")) {");
                            {
                                auto indent = w.indent_scope();
                                w.writeln("return false;");
                            }
                            w.writeln("}");
                            w.writeln("*state += 1;");
                        }
                        w.writeln("}");
                        state++;
                    }
                }
                if (auto if_ = ast::as<ast::If>(elem)) {
                    encode_if(if_);
                }
            }
        }

        void write_encode(const std::shared_ptr<ast::Format>& fmt) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            w.writeln("bool ", fmt->ident->ident, "::encode(::utils::binary::writer& w,int* state) {");
            {
                auto indent = w.indent_scope();
                w.writeln("int tmp_state = 0;");
                w.writeln("if (!state) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("state = &tmp_state;");
                }
                w.writeln("}");
                w.writeln("if (*state == 0 ) {");
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
                    w.writeln("*state += 1;");
                }
                w.writeln("}");
                // encode fields
                encode_indent_block(fmt->body);
                w.writeln("return true;");
            }
            w.writeln("}");
        }

        void write_program(std::shared_ptr<ast::Program>& prog) {
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
                }
                if (auto e = ast::as<ast::Enum>(fmt); e) {
                    write_enum(ast::cast_to<ast::Enum>(fmt));
                }
            }
            for (auto& fmt : prog->elements) {
                if (auto f = ast::as<ast::Format>(fmt); f) {
                    write_encode(ast::cast_to<ast::Format>(fmt));
                }
            }
        }
    };
}  // namespace j2cp2
