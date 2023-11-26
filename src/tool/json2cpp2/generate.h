/*license*/
#pragma once

#include "cpp_type.h"
#include <helper/expected.h>
#include <core/ast/ast.h>
#include <core/writer/writer.h>
#include <core/ast/tool/stringer.h>

namespace j2cp2 {
    struct Generator {
        brgen::writer::Writer w;
        void write_bit_fields(std::vector<std::shared_ptr<ast::Field>>& non_align, size_t bit_size, bool is_int_set, bool include_non_simple) {
            if (is_int_set && !include_non_simple) {
                w.write("::utils::binary::flags_t<std::uint", brgen::nums(bit_size), "_t");
                for (auto& n : non_align) {
                    if (auto int_ty = ast::as<ast::IntType>(n->field_type); int_ty) {
                        w.write(", ", brgen::nums(int_ty->bit_size));
                    }
                    else if (auto enum_ty = ast::as<ast::EnumType>(n->field_type); enum_ty) {
                        auto bit_size = enum_ty->base.lock()->base_type->bit_size;
                        w.write(", ", brgen::nums(bit_size));
                    }
                }
                w.writeln("> ", non_align[0]->ident->ident, ";");
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
                    ast::tool::Stringer s;
                    auto len = s.to_string(arr_ty->length);
                    return "std::uint" + brgen::nums(arr_ty->base_type->bit_size) + "_t[" + len + "]";
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
                        }
                        if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                            if (arr_ty->is_int_set) {
                                ast::tool::Stringer s;
                                auto len = s.to_string(arr_ty->length);
                                w.writeln("std::uint", brgen::nums(arr_ty->base_type->bit_size), "_t ", f->ident->ident, "[", len, "];");
                            }
                            else {
                                w.writeln("std::vector<", get_type_name(arr_ty->base_type), "> ", f->ident->ident, ";");
                            }
                        }
                        if (auto union_ty = ast::as<ast::StructUnionType>(type)) {
                            w.writeln("union {");
                            {
                                auto indent = w.indent_scope();
                                for (auto& f : union_ty->structs) {
                                    write_struct_type(f);
                                    w.writeln(";");
                                }
                            }
                            w.writeln("};");
                        }
                    }
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

        void write_program(std::shared_ptr<ast::Program>& prog) {
            w.writeln("#pragma once");
            w.writeln("#include <cstdint>");
            w.writeln("#include <binary/flags.h>");
            w.writeln("#include <vector>");
            for (auto& fmt : prog->elements) {
                if (auto f = ast::as<ast::Format>(fmt); f) {
                    write_simple_struct(ast::cast_to<ast::Format>(fmt));
                }
            }
        }
    };
}  // namespace j2cp2
