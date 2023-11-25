/*license*/
#pragma once

#include "cpp_type.h"
#include <helper/expected.h>
#include <core/ast/ast.h>
#include <core/writer/writer.h>

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
                }
                w.writeln("> ", non_align[0]->ident->ident, ";");
            }
        }

        void write_struct_type(const std::shared_ptr<ast::StructType>& s) {
            auto member = ast::as<ast::Member>(s->base.lock());
            w.writeln("struct ", member && member->ident ? member->ident->ident + " " : "", "{");
            {
                auto indent = w.indent_scope();
                bool is_int_set = true;
                bool include_non_simple = false;
                size_t bit_size = 0;
                std::vector<std::shared_ptr<ast::Field>> non_aligned;
                for (auto& field : s->fields) {
                    if (auto f = ast::as<ast::Field>(field); f) {
                        if (f->bit_alignment == ast::BitAlignment::not_target) {
                            continue;
                        }
                        if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                            is_int_set = is_int_set && f->field_type->is_int_set;
                            include_non_simple = include_non_simple || ast::as<ast::IntType>(f->field_type) == nullptr;
                            bit_size += f->field_type->bit_size;
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            continue;
                        }
                        if (non_aligned.size() > 0) {
                            is_int_set = is_int_set && f->field_type->is_int_set;
                            include_non_simple = include_non_simple || ast::as<ast::IntType>(f->field_type) == nullptr;
                            bit_size += f->field_type->bit_size;
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            write_bit_fields(non_aligned, bit_size, is_int_set, include_non_simple);
                            non_aligned.clear();
                            is_int_set = true;
                            include_non_simple = false;
                            bit_size = 0;
                        }
                        if (auto int_ty = ast::as<ast::IntType>(f->field_type); int_ty) {
                            if (int_ty->is_common_supported) {
                                w.writeln("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(int_ty->bit_size), "_t ", f->ident->ident, ";");
                            }
                        }
                    }
                }
            }
            w.writeln("};");
        }

        void write_simple_struct(const std::shared_ptr<ast::Format>& fmt) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            write_struct_type(fmt->body->struct_type);
        }

        void write_program(std::shared_ptr<ast::Program>& prog) {
            w.writeln("#pragma once");
            w.writeln("#include <cstdint>");
            w.writeln("#include <binary/flags.h>");
            for (auto& fmt : prog->elements) {
                if (auto f = ast::as<ast::Format>(fmt); f) {
                    write_simple_struct(ast::cast_to<ast::Format>(fmt));
                }
            }
        }
    };
}  // namespace j2cp2
