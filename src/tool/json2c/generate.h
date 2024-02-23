/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/sort.h>
#include <writer/writer.h>
#include <core/ast/tool/stringer.h>
#include "../common/line_map.h"

namespace json2c {
    namespace ast = brgen::ast;
    struct Generator {
        brgen::writer::Writer h_w;
        brgen::writer::Writer c_w;
        ast::tool::Stringer str;
        std::vector<std::string> struct_names;
        std::vector<LineMap> line_map;

        std::string get_type(const std::shared_ptr<ast::Type>& typ) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                auto bit = *int_ty->bit_size;
                if (int_ty->is_common_supported) {
                    return brgen::concat(int_ty->is_signed ? "" : "u", "int", brgen::nums(bit), "_t ");
                }
                else if (bit < 64) {
                    bit = brgen::ast::aligned_bit(bit);
                    return brgen::concat("uint", brgen::nums(bit), "_t ");
                }
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                return brgen::concat(get_type(arr_ty->element_type), "*");
            }
            return "";
        }

        void write_field(ast::Field* field) {
            auto typ = field->field_type;
            auto typ_str = get_type(typ);
            if (field->bit_alignment != field->eventual_bit_alignment) {
                return;
            }
            h_w.writeln(typ_str, field->ident->ident, ";");
            str.map_ident(field->ident, "${THIS}", field->ident->ident);
        }

        void write_struct_type(const std::shared_ptr<ast::StructType>& typ) {
            for (auto& field : typ->fields) {
                if (auto f = ast::as<ast::Field>(field)) {
                    write_field(f);
                }
            }
        }

        static constexpr auto buffer_offset = "*buffer_offset";
        static constexpr auto buffer_bit_offset = "*buffer_bit_offset";
        static constexpr auto buffer_size = "buffer_size";
        static constexpr auto buffer = "buffer";
        static constexpr auto bit_per_byte = "8";

        void encode_decode_int_field(ast::IntType* int_ty, std::string_view ident, bool encode, bool need_length_check = true) {
            if (int_ty->is_common_supported) {
                auto bit = *int_ty->bit_size;
                if (need_length_check) {
                    c_w.writeln("if (", buffer_offset, " + sizeof(", ident, ") > ", buffer_size, ") {");
                    c_w.indent_writeln("return -1;");
                    c_w.writeln("}");
                }
                c_w.writeln("for (size_t i = 0; i < sizeof(", ident, "); i++) {");
                auto i = "i";
                std::string shift_offset;
                if (int_ty->endian == ast::Endian::little) {
                    shift_offset = i;
                }
                else {
                    shift_offset = brgen::concat("(sizeof(", ident, ") - ", i, " - 1)");
                }
                if (encode) {
                    c_w.indent_writeln(
                        buffer, "[", buffer_offset, " + ", i, "] = ",
                        "(uint8_t)((((uint", brgen::nums(bit), "_t)", ident, ") >> (", shift_offset, " * ", bit_per_byte, "))&0xff);");
                }
                else {
                    c_w.indent_writeln(
                        ident, " |= ((uint", brgen::nums(bit), "_t)", buffer, "[", buffer_offset, " + ", i, "]) << ",
                        "(", shift_offset, " * ", bit_per_byte, ");");
                }
                c_w.writeln("}");
                c_w.writeln(buffer_offset, " += sizeof(", ident, ");");
            }
        }

        void write_format_type_encode(std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(int_ty, ident, true, need_length_check);
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
            }
        }

        void write_format_encode(const std::shared_ptr<ast::Format>& fmt) {
            h_w.writeln("int ", fmt->ident->ident, "_encode_ex(const ", fmt->ident->ident, "* this_, uint8_t* ", buffer, ", size_t ", buffer_size, ",size_t ", buffer_offset, ",size_t", buffer_bit_offset, ");");
            h_w.writeln("inline int ", fmt->ident->ident, "_encode(const ", fmt->ident->ident, "* this_, uint8_t* buffer, size_t buffer_size) {");
            h_w.indent_writeln("size_t buffer_offset = 0;");
            h_w.indent_writeln("size_t buffer_bit_offset = 0;");
            h_w.indent_writeln("return ", fmt->ident->ident, "_encode_ex(this_, buffer, buffer_size, &buffer_offset, &buffer_bit_offset);");
            h_w.writeln("}");
            c_w.writeln("int ", fmt->ident->ident, "_encode_ex(const ", fmt->ident->ident, "* this_, uint8_t* ", buffer, ", size_t ", buffer_size, ",size_t ", buffer_offset, ",size_t", buffer_bit_offset, ") {");

            {
                auto scope = c_w.indent_scope();
                futils::helper::DynDefer d;
                if (fmt->body->struct_type->bit_size) {
                    auto len = (fmt->body->struct_type->bit_size.value() + futils::bit_per_byte - 1) / futils::bit_per_byte;
                    c_w.writeln("if (", buffer_offset, " + ", brgen::nums(len), " > ", buffer_size, ") {");
                    c_w.indent_writeln("return -1;");
                    c_w.writeln("}");
                }
                for (auto& field : fmt->body->elements) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        auto ident = str.to_string(f->ident);
                        auto typ = f->field_type;
                        write_format_type_encode(ident, typ, fmt->body->struct_type->bit_size.has_value());
                    }
                }
                c_w.writeln("return 0;");
            }
            c_w.writeln("}");
        }

        void write_format_type_decode(std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(int_ty, ident, false, need_length_check);
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
            }
        }

        void write_format_decode(const std::shared_ptr<ast::Format>& fmt) {
            h_w.writeln("int ", fmt->ident->ident, "_decode_ex(", fmt->ident->ident, "* this_,const uint8_t* ", buffer, ", size_t ", buffer_size, ",size_t ", buffer_offset, ",size_t", buffer_bit_offset, ");");
            h_w.writeln("inline int ", fmt->ident->ident, "_decode(", fmt->ident->ident, "* this_,const uint8_t* buffer, size_t buffer_size) {");
            h_w.indent_writeln("size_t buffer_offset = 0;");
            h_w.indent_writeln("size_t buffer_bit_offset = 0;");
            h_w.indent_writeln("return ", fmt->ident->ident, "_decode_ex(this_, buffer, buffer_size, &buffer_offset, &buffer_bit_offset);");
            h_w.writeln("}");
            c_w.writeln("int ", fmt->ident->ident, "_decode_ex(", fmt->ident->ident, "* this_,const uint8_t* ", buffer, ", size_t ", buffer_size, ",size_t ", buffer_offset, ",size_t", buffer_bit_offset, ") {");
            if (fmt->body->struct_type->bit_size) {
                auto len = (fmt->body->struct_type->bit_size.value() + futils::bit_per_byte - 1) / futils::bit_per_byte;
                c_w.writeln("if (", buffer_offset, " + ", brgen::nums(len), " > ", buffer_size, ") {");
                c_w.indent_writeln("return -1;");
                c_w.writeln("}");
            }
            {
                auto scope = c_w.indent_scope();
                for (auto& field : fmt->body->elements) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        auto ident = str.to_string(f->ident);
                        auto typ = f->field_type;
                        write_format_type_decode(ident, typ, fmt->body->struct_type->bit_size.has_value());
                    }
                }
                c_w.writeln("return 0;");
            }
            c_w.writeln("}");
        }

        void write_enum_member(const std::shared_ptr<ast::EnumMember>& member) {
            auto& member_ident = member->ident->ident;
            auto value = str.to_string(member->value);
            h_w.writeln(member_ident, " = ", value, ",");
        }

        void write_enum(const std::shared_ptr<ast::Enum>& e) {
            auto& enum_ident = e->ident->ident;
            h_w.writeln("typedef enum ", enum_ident, " {");
            for (auto& member : e->members) {
                write_enum_member(member);
            }
            h_w.writeln("} ", enum_ident, ";");
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
            auto typ = fmt->body->struct_type;
            auto ident = fmt->ident->ident;
            h_w.writeln("typedef struct ", ident, " {");
            {
                auto scope = h_w.indent_scope();
                write_struct_type(typ);
            }
            h_w.writeln("} ", ident, ";");
            write_format_encode(fmt);
            write_format_decode(fmt);
        }

        void write_program(const std::shared_ptr<ast::Program>& prog) {
            h_w.writeln("//Code generated by json2c");
            h_w.writeln("#pragma once");
            h_w.writeln("#include <stddef.h>");
            h_w.writeln("#include <stdint.h>");
            c_w.writeln("//Code generated by json2c");
            // c_w.writeln("#include \"\"");
            str.this_access = "this_->";
            for (auto& fmt : prog->elements) {
                if (auto b = ast::as<ast::Binary>(fmt); b && b->op == ast::BinaryOp::const_assign && b->right->constant_level == ast::ConstantLevel::constant) {
                    auto ident = ast::as<ast::Ident>(b->left);
                    assert(ident);
                    h_w.writeln("#define ", ident->ident, " ", str.to_string(b->right));
                    str.map_ident(ast::cast_to<ast::Ident>(b->left), ident->ident);
                }
                if (auto e = ast::as<ast::Enum>(fmt); e) {
                    write_enum(ast::cast_to<ast::Enum>(fmt));
                }
            }
            ast::tool::FormatSorter s;
            auto sorted = s.topological_sort(prog);
            for (auto& fmt : sorted) {
                write_format(ast::cast_to<ast::Format>(fmt));
            }
        }
    };
}  // namespace json2c
