/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/sort.h>
#include <writer/writer.h>
#include <core/ast/tool/stringer.h>
#include "../common/line_map.h"
#include "ctype.h"

namespace json2c {
    namespace ast = brgen::ast;

    struct Generator {
        brgen::writer::Writer h_w;
        brgen::writer::Writer c_w;
        ast::tool::Stringer str;
        std::vector<std::string> struct_names;
        std::vector<LineMap> line_map;
        bool encode = false;

        void write_field(brgen::writer::Writer& t_w, ast::Field* field) {
            if (field->bit_alignment != field->eventual_bit_alignment) {
                return;
            }
            auto typ = field->field_type;
            auto typ_str = get_type(typ);
            auto field_str = typ_str->to_string(field->ident->ident);
            t_w.writeln(field_str, ";");

            if (auto arr_ty = ast::as<ast::ArrayType>(field->field_type);
                arr_ty && !arr_ty->length_value) {
                t_w.writeln("size_t ", field->ident->ident, "_size;");
            }
        }

        void write_struct_type(brgen::writer::Writer& t_w, const std::shared_ptr<ast::StructType>& typ) {
            for (auto& field : typ->fields) {
                if (auto f = ast::as<ast::Field>(field)) {
                    write_field(t_w, f);
                }
            }
        }

        static constexpr auto buffer_offset = "buffer_offset";
        static constexpr auto buffer_bit_offset = "buffer_bit_offset";
        static constexpr auto buffer_size = "buffer_size";
        static constexpr auto buffer = "buffer";
        static constexpr auto allocation_holder = "allocation_holder";
        static constexpr auto allocation_holder_size = "allocation_holder_size";
        static constexpr auto alloc = "alloc";
        static constexpr auto bit_per_byte = "8";

        void add_buffer_offset(auto&&... offset) {
            c_w.writeln(io_(buffer_offset), " += ", offset..., ";");
        }

        std::string io_(std::string_view ident) {
            if (encode) {
                return brgen::concat("output_->", ident);
            }
            else {
                return brgen::concat("input_->", ident);
            }
        }

        std::string length_of(std::string_view ident) {
            return brgen::concat(ident, "_size");
        }

        void check_dynamic_array_length(auto&& ident, auto&& length) {
            c_w.writeln("if (", ident, "_size != ", length, ") {");
            c_w.indent_writeln("return -1;");
            c_w.writeln("}");
        }

        void check_buffer_length(auto&&... size) {
            c_w.writeln(
                "if (", io_(buffer_offset),
                " + (", size..., ") > ", io_(buffer_size), ") {");
            c_w.indent_writeln("return -1;");
            c_w.writeln("}");
        }

        void encode_decode_int_field(ast::IntType* int_ty, std::string_view ident, bool encode, bool need_length_check = true) {
            if (int_ty->is_common_supported) {
                auto bit = *int_ty->bit_size;
                if (need_length_check) {
                    check_buffer_length("sizeof(", ident, ")");
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
                        io_(buffer), "[", io_(buffer_offset), " + ", i, "] = ",
                        "(uint8_t)((((uint", brgen::nums(bit), "_t)", ident, ")", " >> ",
                        "(", shift_offset, " * ", bit_per_byte, "))&0xff);");
                }
                else {
                    c_w.indent_writeln(
                        ident, " |= ((uint", brgen::nums(bit), "_t)",
                        io_(buffer), "[", io_(buffer_offset), " + ", i, "]) << ",
                        "(", shift_offset, " * ", bit_per_byte, ");");
                }
                c_w.writeln("}");
                add_buffer_offset("sizeof(", ident, ")");
            }
        }

        void write_array_for_loop(std::string_view ident, std::string_view length, const std::shared_ptr<ast::Type> elem_typ, bool encode, bool need_length_check) {
            c_w.writeln("for (size_t i = 0; i < ", length, "; i++) {");
            if (encode) {
                write_type_encode(brgen::concat(ident, "[i]"), elem_typ, need_length_check);
            }
            else {
                write_type_decode(brgen::concat(ident, "[i]"), elem_typ, need_length_check);
            }
            c_w.writeln("}");
        }

        void write_type_encode(std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(int_ty, ident, true, need_length_check);
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                auto for_loop = [&](auto&& length, bool need_length_check) {
                    write_array_for_loop(ident, length, arr_ty->element_type, true, need_length_check);
                };
                if (arr_ty->bit_size) {
                    check_buffer_length(arr_ty->bit_size.value() / futils::bit_per_byte);
                    for_loop(brgen::nums(arr_ty->bit_size.value() / futils::bit_per_byte), false);
                }
                else if (arr_ty->length_value) {
                    for_loop(brgen::nums(*arr_ty->length_value), true);
                }
                else if (ast::is_any_range(arr_ty->length)) {
                    for_loop(length_of(ident), true);
                }
                else {
                    auto len = str.to_string(arr_ty->length);
                    check_dynamic_array_length(ident, len);
                    if (auto ity = ast::as<ast::IntType>(arr_ty->element_type); ity && ity->is_common_supported) {
                        check_buffer_length(len, " * sizeof(", ident, "[0])");
                        for_loop(len, false);
                    }
                    else {
                        for_loop(len, true);
                    }
                }
            }
            if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
                write_type_encode(ident, ident_ty->base.lock(), need_length_check);
            }
        }

        void write_field_encode(const std::shared_ptr<ast::Field>& field, bool need_length_check) {
            auto ident = str.to_string(field->ident);
            write_type_encode(ident, field->field_type, need_length_check);
        }

        void write_type_decode(std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(int_ty, ident, false, need_length_check);
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                auto for_loop = [&](auto&& length, bool need_length_check) {
                    write_array_for_loop(ident, length, arr_ty->element_type, false, need_length_check);
                };
                if (arr_ty->bit_size) {
                    auto len = brgen::nums(arr_ty->bit_size.value() / futils::bit_per_byte);
                    check_buffer_length(len);
                    c_w.writeln("for (size_t i = 0; i < ", len, "; i++) {");
                    write_type_decode(brgen::concat(ident, "[i]"), arr_ty->element_type, false);
                    c_w.writeln("}");
                }
                else if (arr_ty->length_value) {
                    c_w.writeln("for (size_t i = 0; i < ", arr_ty->length_value.value(), "; i++) {");
                    write_type_decode(brgen::concat(ident, "[i]"), arr_ty->element_type, true);
                    c_w.writeln("}");
                }
                else if (ast::is_any_range(arr_ty->length)) {
                    c_w.writeln("for (size_t i = 0; i < ", ident, "_size; i++) {");
                    write_type_decode(brgen::concat(ident, "[i]"), arr_ty->element_type, true);
                    c_w.writeln("}");
                }
                else {
                    auto len = str.to_string(arr_ty->length);
                    c_w.writeln("for (size_t i = 0; i < ", len, "; i++) {");
                    write_type_decode(brgen::concat(ident, "[i]"), arr_ty->element_type, true);
                    c_w.writeln("}");
                }
            }
            if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
                write_type_decode(ident, ident_ty->base.lock(), need_length_check);
            }
        }

        void write_field_decode(const std::shared_ptr<ast::Field>& field, bool need_length_check) {
        }

        void write_format_input_type(const std::shared_ptr<ast::Format>& typ) {
            h_w.writeln("typedef struct ", typ->ident->ident, "DecodeInput {");
            {
                auto scope = h_w.indent_scope();
                h_w.writeln("const uint8_t* ", buffer, ";");
                h_w.writeln("size_t ", buffer_size, ";");
                h_w.writeln("size_t ", buffer_offset, ";");
                h_w.writeln("size_t ", buffer_bit_offset, ";");
                h_w.writeln("void** ", allocation_holder, ";");
                h_w.writeln("size_t ", allocation_holder_size, ";");
                h_w.writeln("void* (*", alloc, ")(void*** holder,size_t* holder_size ,size_t size,size_t align);");
            }
            h_w.writeln("} ", typ->ident->ident, "DecodeInput;");
            h_w.writeln();
            h_w.writeln("typedef struct ", typ->ident->ident, "EncodeInput {");
            {
                auto scope = h_w.indent_scope();
                h_w.writeln("uint8_t* ", buffer, ";");
                h_w.writeln("size_t ", buffer_size, ";");
                h_w.writeln("size_t ", buffer_offset, ";");
                h_w.writeln("size_t ", buffer_bit_offset, ";");
                h_w.writeln("void** ", allocation_holder, ";");
                h_w.writeln("size_t ", allocation_holder_size, ";");
                h_w.writeln("void* (*", alloc, ")(void*** holder,size_t* holder_size ,size_t size,size_t align);");
            }
            h_w.writeln("} ", typ->ident->ident, "EncodeInput;");
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
                    c_w.writeln("if (", io_(buffer_offset), " + ",
                                brgen::nums(len), " > ", io_(buffer_size), ") {");
                    c_w.indent_writeln("return -1;");
                    c_w.writeln("}");
                }
                for (auto& field : fmt->body->elements) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        write_field_encode(ast::cast_to<ast::Field>(field), !fmt->body->struct_type->bit_size.has_value());
                    }
                }
                c_w.writeln("return 0;");
            }
            c_w.writeln("}");
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
                        write_field_decode(ast::cast_to<ast::Field>(field), !fmt->body->struct_type->bit_size.has_value());
                    }
                }
                c_w.writeln("return 0;");
            }
            c_w.writeln("}");
        }

        void write_enum_member(auto& enum_ident, const std::shared_ptr<ast::EnumMember>& member) {
            auto& member_ident = member->ident->ident;
            auto value = str.to_string(member->value);
            h_w.writeln(enum_ident, "_", member_ident, " = ", value, ",");
            str.map_ident(member->ident, enum_ident + "_" + member_ident);
        }

        void write_enum(const std::shared_ptr<ast::Enum>& e) {
            auto& enum_ident = e->ident->ident;
            h_w.writeln("typedef enum ", enum_ident, " {");
            for (auto& member : e->members) {
                write_enum_member(enum_ident, member);
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
