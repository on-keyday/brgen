/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/sort.h>
#include <writer/writer.h>
#include <core/ast/tool/stringer.h>
#include "../common/line_map.h"
#include "ctype.h"
#include <numeric>

namespace json2c {
    namespace ast = brgen::ast;

    enum class AssignMode {
        normal,
        bit_field,
        via_func,
    };

    struct Generator {
        brgen::writer::Writer h_w;
        brgen::writer::Writer c_w;
        ast::tool::Stringer str;
        std::vector<std::string> struct_names;
        std::vector<LineMap> line_map;
        bool encode = false;
        size_t seq = 0;
        std::map<std::shared_ptr<ast::Type>, std::string> typedef_map;

        size_t get_seq() {
            return seq++;
        }

        std::string typeof_(const std::shared_ptr<ast::Type>& ty) {
            if (auto ident = ast::as<ast::IdentType>(ty)) {
                return typeof_(ident->base.lock());
            }
            return typedef_map[ty];
        }

        std::string write_c_type_field(ast::Field* field, size_t depth, const std::shared_ptr<CType>& ctype) {
            if (ctype->kind == CTypeKind::pointer) {
                auto ptr = std::static_pointer_cast<CPointer>(ctype);
                auto base_ty = write_c_type_field(field, depth + 1, ptr->element_type);
                auto& ident = field->belong.lock()->ident->ident;
                auto name = brgen::concat(ident, "_", field->ident->ident, "_", brgen::nums(depth), "_", brgen::nums(get_seq()));
                h_w.writeln("struct ", name, " {");
                h_w.indent_writeln(base_ty, "* data;");
                h_w.indent_writeln("size_t size;");
                h_w.writeln("};");
                auto t = brgen::concat("struct ", name);
                typedef_map[ctype->base] = t;
                return t;
            }
            if (ctype->kind == CTypeKind::array) {
                auto arr = std::static_pointer_cast<CArray>(ctype);
                auto base_ty = write_c_type_field(field, depth + 1, arr->element_type);
                auto& ident = field->belong.lock()->ident->ident;
                auto name = brgen::concat(ident, "_", field->ident->ident, "_", brgen::nums(depth));
                h_w.writeln("typedef ", base_ty, " ", name, "[", arr->length, "];");
                typedef_map[ctype->base] = name;
                return name;
            }
            if (ctype->kind == CTypeKind::primitive) {
                auto prim = std::static_pointer_cast<CPrimitive>(ctype);
                typedef_map[ctype->base] = prim->name;
                return prim->name;
            }
            return "";
        }

        void write_struct_union_type(std::string_view prefix, brgen::writer::Writer& t_w, ast::StructUnionType* union_) {
            t_w.writeln("union {");
            for (auto& struct_ : union_->structs) {
                auto union_ident = "_" + brgen::nums(get_seq());
                auto new_prefix = brgen::concat(prefix, union_ident, ".");
                t_w.writeln("struct {");
                {
                    auto scope = t_w.indent_scope();
                    write_struct_type(new_prefix, t_w, struct_);
                }
                t_w.writeln("} ", union_ident, ";");
            }
            t_w.writeln("};");
        }

        void write_field(std::string_view prefix, brgen::writer::Writer& t_w, ast::Field* field) {
            if (field->bit_alignment == ast::BitAlignment::not_target) {
                return;
            }
            if (field->bit_alignment != field->eventual_bit_alignment) {
                return;
            }
            if (auto union_ = ast::as<ast::StructUnionType>(field->field_type)) {
                write_struct_union_type(prefix, t_w, union_);
                return;
            }
            auto typ = write_c_type_field(field, 0, get_type(field->field_type));
            t_w.writeln(typ, " ", field->ident->ident, ";");
            str.map_ident(field->ident, prefix, field->ident->ident);
        }

        void write_bit_fields(std::string_view prefix, brgen::writer::Writer& t_w, std::vector<std::shared_ptr<ast::Field>>& fields) {
            auto sum = std::accumulate(fields.begin(), fields.end(), std::optional<size_t>(0), [](auto p, auto f) -> std::optional<size_t> {
                if (!p) {
                    return std::nullopt;
                }
                if (!f->field_type->bit_size) {
                    return std::nullopt;
                }
                return p.value() + f->field_type->bit_size.value();
            });
            if (!sum) {
                return;
            }
            auto align = brgen::ast::aligned_bit(sum.value());
            auto name = brgen::concat("bit_field_", brgen::nums(get_seq()));
            t_w.writeln("uint", brgen::nums(align), "_t ", name, ";");
        }

        void write_struct_type(std::string_view prefix, brgen::writer::Writer& t_w, const std::shared_ptr<ast::StructType>& typ) {
            std::vector<std::shared_ptr<ast::Field>> fields;
            for (auto& field : typ->fields) {
                if (auto f = ast::as<ast::Field>(field)) {
                    if (f->bit_alignment != f->eventual_bit_alignment) {
                        fields.push_back(ast::cast_to<ast::Field>(field));
                    }
                    if (fields.size() > 0) {
                        write_bit_fields(prefix, t_w, fields);
                        continue;
                    }
                    write_field(prefix, t_w, f);
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
        static constexpr auto realloc = "realloc";
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

        std::string io_input() {
            return encode ? "output_" : "input_";
        }

        std::string io_input_type(std::string_view type) {
            if (encode) {
                return brgen::concat(type, "EncodeInput");
            }
            else {
                return brgen::concat(type, "DecodeInput");
            }
        }

        void write_return_error() {
            c_w.indent_writeln("return 0;");
        }

        void allocate(auto&& assign_to, auto&& align, auto&& size) {
            c_w.writeln("if (", io_(alloc), ") {");
            // TODO(on-keyday): cast for C++ compatibility?
            c_w.indent_writeln(assign_to, " = ", io_(alloc), "(&", io_(allocation_holder), ", &", io_(allocation_holder_size), ", ", size, ",", align, ");");
            {
                auto scope = c_w.indent_scope();
                c_w.writeln("if (!", assign_to, ") {");
                write_return_error();
                c_w.writeln("}");
            }
            c_w.writeln("} else {");
            write_return_error();
            c_w.writeln("}");
        }

        void reallocate(auto&& assign_to, auto&& old, auto&& align, auto&& size) {
            c_w.writeln("if (", io_(realloc), ") {");
            // TODO(on-keyday): cast for C++ compatibility?
            c_w.indent_writeln(assign_to, " = ", io_(realloc), "(", old, ", &", io_(allocation_holder), ", &", io_(allocation_holder_size), ", ", size, ",", align, ");");
            {
                auto scope = c_w.indent_scope();
                c_w.writeln("if (!", assign_to, ") {");
                write_return_error();
                c_w.writeln("}");
            }
            c_w.writeln("} else {");
            write_return_error();
            c_w.writeln("}");
        }

        std::string length_of(std::string_view ident) {
            return brgen::concat(ident, ".size");
        }

        std::string data_of(std::string_view ident) {
            return brgen::concat(ident, ".data");
        }

        void check_dynamic_array_length(auto&& ident, auto&& length) {
            c_w.writeln("if (", length_of(ident), " != ", length, ") {");
            write_return_error();
            c_w.writeln("}");
        }

        void check_buffer_length(auto&&... size) {
            c_w.writeln(
                "if (", io_(buffer_offset),
                " + (", size..., ") > ", io_(buffer_size), ") {");
            write_return_error();
            c_w.writeln("}");
        }

        void encode_decode_int_field(ast::IntType* int_ty, std::string_view ident, bool encode, bool need_length_check = true) {
            if (int_ty->is_common_supported) {
                auto bit = *int_ty->bit_size;
                if (need_length_check) {
                    check_buffer_length("sizeof(", ident, ")");
                }
                if (int_ty->bit_size == 8) {
                    if (encode) {
                        c_w.writeln(io_(buffer), "[", io_(buffer_offset), "] = ", ident, ";");
                    }
                    else {
                        c_w.writeln(ident, " = ", io_(buffer), "[", io_(buffer_offset), "];");
                    }
                    add_buffer_offset("1");
                    return;
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

        void write_array_for_loop(
            std::string_view ident, std::string_view length,
            const std::shared_ptr<ast::Type> elem_typ,
            bool encode,
            bool need_buffer_length_check,
            bool need_array_length_check) {
            c_w.writeln("for (size_t i = 0; i < ", length, "; i++) {");
            {
                auto scope = c_w.indent_scope();
                if (encode) {
                    // on encode, array length is fixed
                    write_type_encode(brgen::concat(ident, "[i]"), elem_typ, need_buffer_length_check);
                }
                else {
                    if (need_array_length_check) {
                        // at here, ident is dynamic array
                        assert(!ident.ends_with(".data"));
                        reallocate(data_of(ident), data_of(ident), brgen::concat("_Alignof(", typeof_(elem_typ), ")"), brgen::concat("(i + 1) * sizeof(", data_of(ident), "[0])"));
                        write_type_decode(brgen::concat(data_of(ident), "[i]"), elem_typ, need_buffer_length_check);
                    }
                    else {
                        write_type_decode(brgen::concat(ident, "[i]"), elem_typ, need_buffer_length_check);
                    }
                }
            }
            c_w.writeln("}");
        }

        void write_type_encode(std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(int_ty, ident, true, need_length_check);
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                auto for_loop = [&](auto&& ident, auto&& length, bool need_buffer_length_check) {
                    write_array_for_loop(ident, length, arr_ty->element_type, true, need_buffer_length_check, false);
                };
                if (arr_ty->bit_size) {
                    auto len = brgen::nums(arr_ty->bit_size.value() / futils::bit_per_byte);
                    if (need_length_check) {
                        check_buffer_length(len);
                    }
                    for_loop(ident, len, false);
                }
                else if (arr_ty->length_value) {
                    for_loop(ident, brgen::nums(*arr_ty->length_value), true);
                }
                else if (ast::is_any_range(arr_ty->length)) {
                    for_loop(data_of(ident), length_of(ident), true);
                }
                else {
                    auto len = str.to_string(arr_ty->length);
                    check_dynamic_array_length(ident, len);
                    if (auto ity = ast::as<ast::IntType>(arr_ty->element_type); ity && ity->is_common_supported) {
                        check_buffer_length(len, " * sizeof(", data_of(ident), "[0])");
                        for_loop(data_of(ident), len, false);
                    }
                    else {
                        for_loop(data_of(ident), len, true);
                    }
                }
            }
            if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
                write_type_encode(ident, ident_ty->base.lock(), need_length_check);
            }
            if (auto s = ast::as<ast::StructType>(typ)) {
                if (auto fmt = ast::as<ast::Format>(s->base.lock())) {
                    auto tmp = "tmp_" + brgen::nums(get_seq());
                    c_w.writeln(io_input_type(fmt->ident->ident), " ", tmp, ";");
                    copy_io_base(io_input(), brgen::concat("(&", tmp, ")"));
                    auto tmp_ret = "tmp_ret_" + brgen::nums(get_seq());
                    c_w.writeln("int ", tmp_ret, " = ", fmt->ident->ident, "_encode_ex(&", ident, ",&", tmp, ");");
                    copy_io_base(brgen::concat("(&", tmp, ")"), io_input());
                    c_w.writeln("if(", tmp_ret, " == 0) {");
                    c_w.indent_writeln("return 0;");
                    c_w.writeln("}");
                }
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
                auto for_loop = [&](auto&& ident, auto&& length, bool need_buffer_length_check, bool need_array_length_check) {
                    write_array_for_loop(ident, length, arr_ty->element_type, false, need_buffer_length_check, need_array_length_check);
                };
                if (arr_ty->bit_size) {
                    auto len = brgen::nums(arr_ty->bit_size.value() / futils::bit_per_byte);
                    check_buffer_length(len);
                    for_loop(ident, len, false, false);
                }
                else if (arr_ty->length_value) {
                    for_loop(ident, brgen::nums(*arr_ty->length_value), true, false);
                }
                else if (ast::is_any_range(arr_ty->length)) {
                    for_loop(ident, length_of(ident), true, true);
                }
                else {
                    auto len = str.to_string(arr_ty->length);
                    check_dynamic_array_length(ident, len);
                    allocate(data_of(ident), brgen::concat("_Alignof(", typeof_(arr_ty->element_type), ")"),
                             brgen::concat(len, "* sizeof(", data_of(ident), "[0])"));
                    if (auto ity = ast::as<ast::IntType>(arr_ty->element_type); ity && ity->is_common_supported) {
                        check_buffer_length(len, " * sizeof(", data_of(ident), "[0])");
                        for_loop(data_of(ident), len, false, false);
                    }
                    else {
                        for_loop(data_of(ident), len, true, false);
                    }
                }
            }
            if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
                write_type_decode(ident, ident_ty->base.lock(), need_length_check);
            }
            if (auto s = ast::as<ast::StructType>(typ)) {
                if (auto fmt = ast::as<ast::Format>(s->base.lock())) {
                    auto tmp = "tmp_" + brgen::nums(get_seq());
                    c_w.writeln(io_input_type(fmt->ident->ident), " ", tmp, ";");
                    copy_io_base(io_input(), brgen::concat("(&", tmp, ")"));
                    auto tmp_ret = "tmp_ret_" + brgen::nums(get_seq());
                    c_w.writeln("int ", tmp_ret, " = ", fmt->ident->ident, "_decode_ex(&", ident, ",&", tmp, ");");
                    copy_io_base(brgen::concat("(&", tmp, ")"), io_input());
                    c_w.writeln("if(", tmp_ret, " == 0) {");
                    c_w.indent_writeln("return 0;");
                    c_w.writeln("}");
                }
            }
        }

        void write_field_decode(const std::shared_ptr<ast::Field>& field, bool need_length_check) {
            auto ident = str.to_string(field->ident);
            write_type_decode(ident, field->field_type, need_length_check);
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
                h_w.writeln("void* (*", realloc, ")(void* p,void*** holder,size_t* holder_size ,size_t size,size_t align);");
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
            }
            h_w.writeln("} ", typ->ident->ident, "EncodeInput;");
        }

        void copy_io_base(auto&& from, auto&& to) {
            c_w.writeln(to, "->", buffer, " = ", from, "->", buffer, ";");
            c_w.writeln(to, "->", buffer_size, " = ", from, "->", buffer_size, ";");
            c_w.writeln(to, "->", buffer_offset, " = ", from, "->", buffer_offset, ";");
            c_w.writeln(to, "->", buffer_bit_offset, " = ", from, "->", buffer_bit_offset, ";");
            if (!encode) {
                c_w.writeln(to, "->", allocation_holder, " = ", from, "->", allocation_holder, ";");
                c_w.writeln(to, "->", allocation_holder_size, " = ", from, "->", allocation_holder_size, ";");
                c_w.writeln(to, "->", alloc, " = ", from, "->", alloc, ";");
                c_w.writeln(to, "->", realloc, " = ", from, "->", realloc, ";");
            }
        }

        void write_format_encode(const std::shared_ptr<ast::Format>& fmt) {
            encode = true;
            h_w.writeln("int ", fmt->ident->ident, "_encode_ex(const ", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* output_);");
            c_w.writeln("int ", fmt->ident->ident, "_encode_ex(const ", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* output_) {");
            {
                auto scope = c_w.indent_scope();
                futils::helper::DynDefer d;
                if (fmt->body->struct_type->bit_size) {
                    auto len = (fmt->body->struct_type->bit_size.value() + futils::bit_per_byte - 1) / futils::bit_per_byte;
                    check_buffer_length(brgen::nums(len));
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
            encode = false;
            h_w.writeln("int ", fmt->ident->ident, "_decode_ex(", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* input_);");
            c_w.writeln("int ", fmt->ident->ident, "_decode_ex(", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* input_) {");
            if (fmt->body->struct_type->bit_size) {
                auto len = (fmt->body->struct_type->bit_size.value() + futils::bit_per_byte - 1) / futils::bit_per_byte;
                check_buffer_length(brgen::nums(len));
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
            {
                auto scope = h_w.indent_scope();
                for (auto& member : e->members) {
                    write_enum_member(enum_ident, member);
                }
            }
            h_w.writeln("} ", enum_ident, ";");
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
            auto typ = fmt->body->struct_type;
            auto ident = fmt->ident->ident;
            brgen::writer::Writer tmp;
            write_struct_type("${THIS}", tmp, typ);
            h_w.writeln("typedef struct ", ident, " {");
            {
                auto scope = h_w.indent_scope();
                h_w.write_unformatted(tmp.out());
            }
            h_w.writeln("} ", ident, ";");
            write_format_input_type(fmt);
            write_format_encode(fmt);
            write_format_decode(fmt);
        }

        void write_program(const std::shared_ptr<ast::Program>& prog, const std::string& h_path) {
            str.type_resolver = [&](brgen::ast::tool::Stringer& s, const std::shared_ptr<brgen::ast::Type>& t) {
                return get_type(t)->to_string("");
            };
            str.cast_handler = [&](brgen::ast::tool::Stringer& s, const std::shared_ptr<brgen::ast::Cast>& t) {
                auto expr = s.to_string(t->expr);
                return brgen::concat("(", get_type(t->expr_type)->to_string(""), ")", expr);
            };
            h_w.writeln("//Code generated by json2c");
            h_w.writeln("#pragma once");
            h_w.writeln("#include <stddef.h>");
            h_w.writeln("#include <stdint.h>");
            c_w.writeln("//Code generated by json2c");
            c_w.writeln("#include \"", h_path, "\"");
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
