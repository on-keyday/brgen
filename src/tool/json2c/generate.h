/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/sort.h>
#include <writer/writer.h>
#include <core/ast/tool/stringer.h>
#include <core/ast/line_map.h>
#include "ctype.h"
#include <numeric>
#include <core/ast/tool/tmp_ident.h>

namespace json2c {
    namespace ast = brgen::ast;

    enum class AssignMode {
        normal,
        bit_field,
        via_func,
    };

    static constexpr auto buffer_offset = "buffer_offset";
    static constexpr auto buffer_bit_offset = "buffer_bit_offset";
    static constexpr auto buffer_size = "buffer_size";
    // immutable
    static constexpr auto buffer_size_origin = "buffer_size_origin";
    static constexpr auto buffer = "buffer";
    static constexpr auto allocation_holder = "allocation_holder";
    static constexpr auto allocation_holder_size = "allocation_holder_size";
    static constexpr auto alloc = "alloc";
    static constexpr auto error_msg = "error_msg_buf";
    static constexpr auto error_msg_size = "error_msg_size";
    static constexpr auto error_msg_callback = "error_msg_callback";
    static constexpr auto bit_per_byte = "8";

    struct BitField {
        std::vector<std::shared_ptr<ast::Field>> fields;
        std::shared_ptr<ast::Field> bit_field;
    };

    struct Generator {
        bool omit_error_callback = false;
        bool use_memcpy = false;
        bool zero_copy_optimization = false;
        brgen::writer::Writer h_w;
        brgen::writer::Writer c_w;
        ast::tool::Stringer str;
        std::vector<std::string> struct_names;
        std::vector<ast::LineMap> line_map;
        bool encode = false;
        size_t seq = 0;
        std::map<std::shared_ptr<ast::Type>, std::string> typedef_map;
        bool format_need_length_check = false;
        std::map<std::shared_ptr<ast::Field>, BitField> bit_fields;

        size_t get_seq() {
            return seq++;
        }

        std::string typeof_(const std::shared_ptr<ast::Type>& ty) {
            if (auto ident = ast::as<ast::IdentType>(ty)) {
                return typeof_(ident->base.lock());
            }
            auto found = typedef_map.find(ty);
            if (found != typedef_map.end()) {
                return found->second;
            }
            auto s = write_c_type_field("tmp_type", 0, get_type(ty));
            typedef_map[ty] = s;
            return s;
        }

        std::string write_c_type_field(std::string prefix, size_t depth, const std::shared_ptr<CType>& ctype) {
            if (ctype->kind == CTypeKind::dynamic_array) {
                auto ptr = std::static_pointer_cast<CPointer>(ctype);
                auto base_ty = write_c_type_field(prefix, depth + 1, ptr->element_type);
                auto name = brgen::concat(prefix, "_", brgen::nums(depth), "_", brgen::nums(get_seq()));
                h_w.writeln("typedef ", ctype->to_string(name), ";");
                auto t = brgen::concat(name);
                typedef_map[ctype->base] = t;
                return t;
            }
            if (ctype->kind == CTypeKind::array) {
                auto arr = std::static_pointer_cast<CArray>(ctype);
                auto name = brgen::concat(prefix, "_", brgen::nums(depth), "_", brgen::nums(get_seq()));
                h_w.writeln("typedef ", ctype->to_string(name), ";");
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

        void write_struct_union_type(const std::shared_ptr<ast::Field>& field, std::string_view prefix, brgen::writer::Writer& t_w, ast::StructUnionType* union_) {
            t_w.writeln("union {");
            for (auto& struct_ : union_->structs) {
                auto union_ident = "_" + brgen::nums(get_seq());
                auto new_prefix = brgen::concat(prefix, ".", union_ident);
                t_w.writeln("struct {");
                {
                    auto scope = t_w.indent_scope();
                    write_struct_type(new_prefix, t_w, struct_, false);
                }
                t_w.writeln("} ", union_ident, ";");
            }
            t_w.writeln("};");
            auto fmt = ast::as<ast::Format>(field->belong.lock());
            assert(fmt);
            auto fmt_name = fmt->ident->ident;
            for (auto& w_field : union_->union_fields) {
                auto field = w_field.lock();
                auto union_ty = ast::as<ast::UnionType>(field->field_type);
                if (union_ty->common_type) {
                    auto c_typ = typeof_(union_ty->common_type);
                    auto getter = brgen::concat(fmt_name, "_", field->ident->ident, "_get");
                    auto setter = brgen::concat(fmt_name, "_", field->ident->ident, "_set");
                    brgen::writer::Writer getter_w;
                    brgen::writer::Writer setter_w;
                    auto cast_to = c_typ;
                    std::string cond;
                    if (auto c = union_ty->cond.lock()) {
                        cond = str.to_string(c);
                    }
                    else {
                        cond = "1";
                    }
                    auto els_ = false;
                    bool final_els = false;
                    for (auto& cand : union_ty->candidates) {
                        auto cond1 = cand->cond.lock();
                        if (els_) {
                            getter_w.write(":");
                            setter_w.write(":");
                        }
                        if (ast::is_any_range(cond1)) {
                            auto f = cand->field.lock();
                            if (f) {
                                auto s = str.to_string(cand->field.lock()->ident);
                                getter_w.write(s);
                                setter_w.write("(", s, " = val____,1)");
                            }
                            else {
                                getter_w.write("(", cast_to, ")", "{}");
                                setter_w.write("0");
                            }
                            final_els = true;
                        }
                        else {
                            auto f = cand->field.lock();
                            auto cond1_str = str.to_string(cond1);
                            getter_w.write("(", cond, "==", cond1_str, ")?");
                            setter_w.write("(", cond, "==", cond1_str, ")?");
                            if (f) {
                                auto s = str.to_string(f->ident);
                                getter_w.write(s);
                                setter_w.write("(", s, " = val____,1)");
                            }
                            else {
                                getter_w.write("(", cast_to, ")", "{}");
                                setter_w.write("0");
                            }
                        }
                        els_ = true;
                    }
                    if (!final_els) {
                        getter_w.write(":(", cast_to, ")", "{}");
                        setter_w.write("0");
                    }
                    t_w.writeln("#define ", getter, "(this_) (", getter_w.out(), ")");
                    t_w.writeln("#define ", setter, "(this_,val____) (", setter_w.out(), ")");
                    str.map_ident(field->ident, getter, "(&", prefix, ")");
                }
            }
        }

        void write_field(std::string_view prefix, brgen::writer::Writer& t_w, const std::shared_ptr<ast::Field>& field, bool on_state) {
            if (!field->ident) {
                ast::tool::set_tmp_field_ident(get_seq(), field, "anonymous_");
            }
            if (field->bit_alignment == ast::BitAlignment::not_target) {
                return;
            }
            if (field->bit_alignment != field->eventual_bit_alignment) {
                return;
            }
            if (auto union_ = ast::as<ast::StructUnionType>(field->field_type)) {
                write_struct_union_type(field, prefix, t_w, union_);
                return;
            }
            if (auto str_literal = ast::as<ast::StrLiteralType>(field->field_type)) {
                c_w.writeln("// ", str_literal->base.lock()->value, " (", brgen::nums(str_literal->base.lock()->length), " byte)");
                return;
            }
            auto typ = get_type(field->field_type);
            auto fmt_name = ast::as<ast::Format>(field->belong.lock())->ident->ident;
            auto prefix_of_type = brgen::concat(fmt_name, "_", field->ident->ident);
            auto typ_str = write_c_type_field(prefix_of_type, 0, typ);
            t_w.writeln(typ_str, " ", field->ident->ident, ";");
            str.map_ident(field->ident, prefix, ".", field->ident->ident);
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
                auto fmt = ast::as<ast::Format>(fields[0]->belong.lock());
                assert(fmt);
                write_return_error(ast::cast_to<ast::Format>(fields[0]->belong.lock()), "non supported operation: dynamic bit field");
                return;
            }
            auto bit = brgen::ast::aligned_bit(sum.value());
            auto name = brgen::concat("bit_field_", brgen::nums(get_seq()));
            auto bit_field_type = brgen::concat("uint", brgen::nums(bit), "_t");
            t_w.writeln(bit_field_type, " ", name, ";");
            auto fmt_name = ast::as<ast::Format>(fields[0]->belong.lock())->ident->ident;
            size_t cur_sum = 0;
            for (auto& field : fields) {
                auto width = *field->field_type->bit_size;
                auto mask = (std::uint64_t(1) << width) - 1;
                cur_sum += width;
                auto shift = bit - cur_sum;
                if (!field->ident) {
                    ast::tool::set_tmp_field_ident(get_seq(), field, "anonymous_");
                }
                auto ident = field->ident->ident;
                auto mask_v = brgen::concat(fmt_name, "_", ident, "_mask");
                auto shift_v = brgen::concat(fmt_name, "_", ident, "_shift");
                auto setter = brgen::concat(fmt_name, "_", ident, "_set");
                auto getter = brgen::concat(fmt_name, "_", ident, "_get");
                t_w.writeln("#define ", mask_v, " ", "(", brgen::nums(mask), ")");
                t_w.writeln("#define ", shift_v, " ", "(", brgen::nums(shift), ")");
                t_w.writeln("#define ", setter, "(this_,val) ",
                            "((this_).", name, " = ((this_).", name, "&~(", mask_v, "<<", shift_v,
                            "))|(((val)&", mask_v, ")<<", shift_v, "))");
                auto type = typeof_(field->field_type);
                auto getter_impl = [&](auto&&... this_) {
                    return brgen::concat("(", "(", type, ")((", this_..., name, ">>", shift_v, ")&", mask_v, ")", ")");
                };
                t_w.writeln("#define ", getter, "(this_) ",
                            getter_impl("(this_)."));
                str.map_ident(field->ident, getter, "(", prefix, ")");
            }
            auto tmp_bit_field = std::make_shared<ast::Field>();
            tmp_bit_field->ident = std::make_shared<ast::Ident>(fields.back()->loc, name);
            tmp_bit_field->field_type = std::make_shared<ast::IntType>(fields.back()->loc, bit, ast::Endian::unspec, false);
            tmp_bit_field->ident->base = tmp_bit_field;
            str.map_ident(tmp_bit_field->ident, prefix, ".", tmp_bit_field->ident->ident);
            bit_fields[fields.back()] = {
                fields,
                std::move(tmp_bit_field),
            };
        }

        void write_struct_type(std::string_view prefix, brgen::writer::Writer& t_w, const std::shared_ptr<ast::StructType>& typ, bool on_state) {
            std::vector<std::shared_ptr<ast::Field>> fields;
            for (auto& field : typ->fields) {
                if (auto f = ast::as<ast::Field>(field)) {
                    if (f->bit_alignment != f->eventual_bit_alignment) {
                        fields.push_back(ast::cast_to<ast::Field>(field));
                        continue;
                    }
                    if (fields.size() > 0) {
                        fields.push_back(ast::cast_to<ast::Field>(field));
                        write_bit_fields(prefix, t_w, fields);
                        fields.clear();
                        continue;
                    }
                    write_field(prefix, t_w, ast::cast_to<ast::Field>(field), on_state);
                }
            }
        }

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

        void write_return_error_impl(auto&& msg0, auto&&... msg) {
            if (!omit_error_callback) {
                auto esc = brgen::escape(brgen::concat(msg0, msg...));
                {
                    auto scope = c_w.indent_scope();
                    c_w.writeln("if (", io_(error_msg_callback), ") {");
                    c_w.indent_writeln(io_(error_msg_callback), "(&", io_(error_msg), ", &", io_(error_msg_size), ", \"", esc, "\");");
                    c_w.writeln("}");
                }
            }
            c_w.indent_writeln("return 0;");
        }

        void write_return_error(const std::shared_ptr<ast::Field>& f, auto&& msg0, auto&&... msg) {
            auto fmt = ast::as<ast::Format>(f->belong.lock());
            assert(fmt);
            auto fmt_name = fmt->ident->ident;
            auto field_name = f->ident ? f->ident->ident : "(anonymous field)";
            write_return_error_impl(brgen::concat(fmt_name, "::", field_name, ": ", msg0, msg...));
        }

        void write_return_error(const std::shared_ptr<ast::Format>& fmt, auto&& msg0, auto&&... msg) {
            assert(fmt);
            auto fmt_name = fmt->ident->ident;
            write_return_error_impl(fmt_name, ": ", msg0, msg...);
        }

        void check_buffer_length(const std::shared_ptr<ast::Field>& f, auto&&... size) {
            c_w.writeln(
                "if (", io_(buffer_offset),
                " + (", size..., ") > ", io_(buffer_size), ") {");
            write_return_error(f, "require ", size..., " bytes but got ", io_(buffer_size), " is not enough");
            c_w.writeln("}");
        }

        void check_buffer_length(const std::shared_ptr<ast::Format>& f, auto&&... size) {
            c_w.writeln(
                "if (", io_(buffer_offset),
                " + (", size..., ") > ", io_(buffer_size), ") {");
            write_return_error(f, "require ", size..., " bytes but got ", io_(buffer_size), " is not enough");
            c_w.writeln("}");
        }

        void allocate(const std::shared_ptr<ast::Field>& f, auto&& assign_to, auto&& align, auto&& size, auto&& elem_size, bool may_realloc) {
            c_w.writeln("if (", io_(alloc), ") {");
            c_w.indent_writeln(io_(alloc), "((void**)&", assign_to, ",&", io_(allocation_holder), ", &", io_(allocation_holder_size), ", ",
                               size, ",", align, ",", elem_size, ",", may_realloc ? "1" : "0", ",",
                               io_(buffer), ",", io_(buffer_size), ",", io_(buffer_offset),
                               ");");
            {
                auto scope = c_w.indent_scope();
                c_w.writeln("if (!", assign_to, ") {");
                write_return_error(f, "allocation failed");
                c_w.writeln("}");
            }
            c_w.writeln("} else {");
            write_return_error(f, "no alloc function");
            c_w.writeln("}");
        }

        std::string length_of(std::string_view ident) {
            return brgen::concat(ident, ".size");
        }

        std::string data_of(std::string_view ident) {
            return brgen::concat(ident, ".data");
        }

        void check_dynamic_array_length(const std::shared_ptr<ast::Field>& f, auto&& ident, auto&& length) {
            c_w.writeln("if (", length_of(ident), " != ", length, ") {");
            write_return_error(f, "require ", length, " but got ", length_of(ident), " is not equal");
            c_w.writeln("}");
        }

        void encode_decode_int_field(
            const std::shared_ptr<ast::Field>& f,
            ast::IntType* ity,
            std::string_view ident,
            bool encode, bool need_length_check,
            std::string_view base_offset = {}) {
            encode_decode_int_field(f, *ity->bit_size, ity->is_common_supported, ity->endian, ident, encode, need_length_check, base_offset);
        }

        void encode_decode_int_field(
            const std::shared_ptr<ast::Field>& f,
            size_t bit_size,
            bool is_common_supported,
            ast::Endian endian,
            std::string_view ident,
            bool encode, bool need_length_check,
            std::string_view base_offset = {}) {
            bool no_offset = base_offset.empty();
            auto buf_offset = io_(buffer_offset);
            if (no_offset) {
                base_offset = buf_offset;
            }
            if (is_common_supported) {
                auto bit = bit_size;
                if (need_length_check) {
                    check_buffer_length(f, "sizeof(", ident, ")");
                }
                if (bit_size == 8) {
                    if (encode) {
                        c_w.writeln(io_(buffer), "[", base_offset, "] = ", ident, ";");
                    }
                    else {
                        c_w.writeln(ident, " = ", io_(buffer), "[", base_offset, "];");
                    }
                    if (no_offset) {
                        add_buffer_offset("1");
                    }
                    return;
                }
                if (!encode) {
                    c_w.writeln(ident, " = ", "0", ";");
                }
                auto i = "tmp_i_" + brgen::nums(get_seq());
                c_w.writeln("for (size_t ", i, " = 0; ", i, " < sizeof(", ident, "); ", i, "++) {");
                std::string shift_offset;
                if (endian == ast::Endian::little) {
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
                        io_(buffer), "[", base_offset, " + ", i, "]) << ",
                        "(", shift_offset, " * ", bit_per_byte, ");");
                }
                c_w.writeln("}");
                if (no_offset) {
                    add_buffer_offset("sizeof(", ident, ")");
                }
            }
        }

        // if length is "<EOF>", it means that the length is determined by the remaining buffer size.
        void write_array_for_loop(
            const std::shared_ptr<ast::Field>& field,
            std::string_view ident, std::string_view length,
            const std::shared_ptr<ast::Type> elem_typ,
            bool encode,
            bool need_buffer_length_check,
            bool need_array_length_check,
            bool dynamic_array) {
            // zero-copy optimization
            // if the element type is 8-bit integer, we can use the buffer directly
            futils::helper::DynDefer d;
            if (zero_copy_optimization && dynamic_array && !need_buffer_length_check && !need_array_length_check && !encode) {
                auto int_ty = ast::as<ast::IntType>(elem_typ);
                if (int_ty && int_ty->bit_size == 8) {
                    c_w.writeln("// zero-copy optimization");
                    c_w.writeln("if(", ident, " != ", io_(buffer), " + ", io_(buffer_offset), ") {");
                    auto ex = c_w.indent_scope_ex();
                    d = futils::helper::defer_ex([&, ex = std::move(ex)]() mutable {
                        ex.execute();
                        c_w.writeln("}");
                    });
                }
            }
            if (use_memcpy && !need_array_length_check && !need_buffer_length_check) {
                auto int_ty = ast::as<ast::IntType>(elem_typ);
                if (int_ty && int_ty->bit_size == 8) {
                    if (encode) {
                        c_w.writeln("memcpy(", io_(buffer), " + ", io_(buffer_offset), ", ", ident, ", ", length, ");");
                    }
                    else {
                        c_w.writeln("memcpy(", ident, ", ", io_(buffer), " + ", io_(buffer_offset), ", ", length, ");");
                    }
                    d.execute();
                    add_buffer_offset(length);
                    return;
                }
            }
            auto i = "tmp_i_" + brgen::nums(get_seq());
            if (length == "<EOF>") {
                c_w.writeln("for (size_t ", i, " = 0; ", io_(buffer_offset), " < ", io_(buffer_size), "; ", i, "++) {");
            }
            else {
                c_w.writeln("for (size_t ", i, " = 0; ", i, " < ", length, "; ", i, "++) {");
            }
            bool should_add_offset = false;
            {
                auto scope = c_w.indent_scope();
                if (encode) {
                    // optimize for common integer types
                    if (auto ity = ast::as<ast::IntType>(elem_typ); ity && !need_buffer_length_check) {
                        auto base_offset = brgen::concat(i, " * sizeof(", ident, "[", i, "])");
                        encode_decode_int_field(field, ity, brgen::concat(ident, "[", i, "]"), true, need_buffer_length_check, base_offset);
                        should_add_offset = true;
                    }
                    else {
                        // on encode, array length is fixed
                        write_type_encode(field, brgen::concat(ident, "[", i, "]"), elem_typ, need_buffer_length_check);
                    }
                }
                else {
                    if (need_array_length_check) {
                        // at here, ident is dynamic array
                        allocate(field, data_of(ident), brgen::concat("alignof(", typeof_(elem_typ), ")"), brgen::concat("(", i, " + 1) * sizeof(", data_of(ident), "[0])"), brgen::concat("sizeof(", data_of(ident), "[0])"), true);
                        c_w.writeln(length_of(ident), " = ", i, " + 1;");
                        write_type_decode(field, brgen::concat(data_of(ident), "[", i, "]"), elem_typ, need_buffer_length_check);
                    }
                    else {
                        // optimize for common integer types
                        if (auto ity = ast::as<ast::IntType>(elem_typ); ity && !need_buffer_length_check) {
                            auto base_offset = brgen::concat(i, " * sizeof(", ident, "[", i, "])");
                            encode_decode_int_field(field, ity, brgen::concat(ident, "[", i, "]"), false, need_buffer_length_check, base_offset);
                            should_add_offset = true;
                        }
                        else {
                            write_type_decode(field, brgen::concat(ident, "[", i, "]"), elem_typ, need_buffer_length_check);
                        }
                    }
                }
            }
            c_w.writeln("}");
            d.execute();
            if (should_add_offset) {
                add_buffer_offset(brgen::concat(length, " * sizeof(", ident, "[0])"));
            }
        }

        void write_type_encode(const std::shared_ptr<ast::Field>& f, std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(f, int_ty, ident, true, need_length_check);
            }
            else if (auto float_ty = ast::as<ast::FloatType>(typ)) {
                auto reinterpret_int = "tmp_" + brgen::nums(get_seq());
                auto map_type = map_float_type_to_int_type(*float_ty->bit_size);
                c_w.writeln(map_type, " ", reinterpret_int, " = ", "*((", map_type, "*)(&", ident, "));");
                encode_decode_int_field(f, *float_ty->bit_size, true, float_ty->endian, reinterpret_int, true, need_length_check);
            }
            else if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                auto for_loop = [&](auto&& ident, auto&& length, bool need_buffer_length_check, bool dynamic_array) {
                    write_array_for_loop(f, ident, length, arr_ty->element_type, true, need_buffer_length_check, false, dynamic_array);
                };
                if (arr_ty->bit_size) {
                    auto len = brgen::nums(arr_ty->bit_size.value() / futils::bit_per_byte);
                    if (need_length_check) {
                        check_buffer_length(f, len);
                    }
                    for_loop(ident, len, false, false);
                }
                else if (arr_ty->length_value) {
                    for_loop(ident, brgen::nums(*arr_ty->length_value), true, false);
                }
                else if (ast::is_any_range(arr_ty->length)) {
                    if (auto ity = ast::as<ast::IntType>(arr_ty->element_type); ity && ity->is_common_supported) {
                        check_buffer_length(f, length_of(ident), " * sizeof(", data_of(ident), "[0])");
                        for_loop(data_of(ident), length_of(ident), false, true);
                    }
                    else {
                        for_loop(data_of(ident), length_of(ident), true, true);
                    }
                }
                else {
                    auto len = str.to_string(arr_ty->length);
                    auto len_var = "tmp_len_" + brgen::nums(get_seq());
                    c_w.writeln("size_t ", len_var, " = ", len, ";");
                    check_dynamic_array_length(f, ident, len_var);
                    if (arr_ty->element_type->bit_size) {
                        check_buffer_length(f, len_var, " * sizeof(", data_of(ident), "[0])");
                        for_loop(data_of(ident), len_var, false, true);
                    }
                    else {
                        for_loop(data_of(ident), len_var, true, true);
                    }
                }
            }
            else if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
                write_type_encode(f, ident, ident_ty->base.lock(), need_length_check);
            }
            else if (auto s = ast::as<ast::StructType>(typ)) {
                if (auto fmt = ast::as<ast::Format>(s->base.lock())) {
                    assert(ast::as<ast::Format>(f->belong.lock()));
                    auto from_fmt = ast::cast_to<ast::Format>(f->belong.lock());
                    auto to_fmt = ast::cast_to<ast::Format>(s->base.lock());
                    switch_io(from_fmt, to_fmt, [&](auto&& cast, auto&& restore_io) {
                        auto tmp_ret = "tmp_ret_" + brgen::nums(get_seq());
                        c_w.writeln("int ", tmp_ret, " = ", fmt->ident->ident, "_encode_ex(&", ident, ",", cast, ");");
                        restore_io();
                        c_w.writeln("if(", tmp_ret, " == 0) {");
                        c_w.indent_writeln("return 0;");
                        c_w.writeln("}");
                    });
                }
            }
            else if (auto e = ast::as<ast::EnumType>(typ)) {
                auto b = e->base.lock()->base_type;
                auto base_ty = ast::as<ast::IntType>(b);
                if (base_ty) {
                    auto tmp = "tmp_" + brgen::nums(get_seq());
                    auto tmp_typ = typeof_(b);
                    c_w.writeln(tmp_typ, " ", tmp, " = (", tmp_typ, ")", ident, ";");
                    encode_decode_int_field(f, base_ty, tmp, true, need_length_check);
                }
            }
            else if (auto s = ast::as<ast::StrLiteralType>(typ)) {
                auto len = brgen::nums(s->base.lock()->length);
                if (need_length_check) {
                    check_buffer_length(f, len);
                }
                c_w.writeln("for(size_t i = 0; i < ", len, "; i++) {");
                c_w.indent_writeln(io_(buffer), "[", io_(buffer_offset), " + i] = ", s->base.lock()->value, "[i];");
                c_w.writeln("}");
                add_buffer_offset(len);
            }
            else {
                if (!typ) {
                    write_return_error(f, "null type");
                }
                else {
                    write_return_error(f, "non supported type: ", ast::node_type_to_string(typ->node_type));
                }
            }
        }

        void write_field_encode(const std::shared_ptr<ast::Field>& field, bool need_length_check) {
            if (field->bit_alignment != field->eventual_bit_alignment) {
                return;
            }
            if (auto found = bit_fields.find(field); found != bit_fields.end()) {
                auto ident = str.to_string(found->second.bit_field->ident);
                auto int_ty = ast::as<ast::IntType>(found->second.bit_field->field_type);
                encode_decode_int_field(field, int_ty, ident, true, need_length_check);
                return;
            }
            futils::helper::DynDefer d;
            if (field->arguments && field->arguments->sub_byte_length) {
                auto len_in_bytes = str.to_string(field->arguments->sub_byte_length);
                check_buffer_length(field, len_in_bytes);
                auto tmp_var = "tmp_save_buffer_size" + brgen::nums(get_seq());
                c_w.writeln("size_t ", tmp_var, " = ", io_(buffer_size), ";");
                c_w.writeln(io_(buffer_size), " = ", io_(buffer_offset), " + ", len_in_bytes, ";");
                d = futils::helper::DynDefer([=, this] {
                    c_w.writeln("if (", io_(buffer_size), " != ", io_(buffer_offset), ") {");
                    write_return_error(field, "require ", len_in_bytes, " bytes but got ", io_(buffer_size), " is not enough");
                    c_w.writeln("}");
                    c_w.writeln(io_(buffer_size), " = ", tmp_var, ";");
                });
            }
            auto ident = str.to_string(field->ident);
            if (field->arguments && field->arguments->arguments.size() == 1) {
                if (auto typ = ast::as<ast::IntType>(field->field_type)) {
                    auto val = str.to_string(field->arguments->arguments[0]);
                    c_w.writeln("if(", ident, " != ", val, ") {");
                    write_return_error(field, "require ", val, " but got ", ident, " is not equal");
                    c_w.writeln("}");
                }
            }
            write_type_encode(field, ident, field->field_type, need_length_check);
        }

        void write_type_decode(const std::shared_ptr<ast::Field>& f, std::string_view ident, const std::shared_ptr<ast::Type>& typ, bool need_length_check = true) {
            if (auto int_ty = ast::as<ast::IntType>(typ)) {
                encode_decode_int_field(f, int_ty, ident, false, need_length_check);
            }
            else if (auto float_ty = ast::as<ast::FloatType>(typ)) {
                auto reinterpret_int = "tmp_" + brgen::nums(get_seq());
                auto map_type = map_float_type_to_int_type(*float_ty->bit_size);
                c_w.writeln(map_type, " ", reinterpret_int, ";");
                encode_decode_int_field(f, *float_ty->bit_size, true, float_ty->endian, reinterpret_int, false, need_length_check);
                c_w.writeln(ident, " = *((", typeof_(typ), "*)(&", reinterpret_int, ");");
            }
            else if (auto arr_ty = ast::as<ast::ArrayType>(typ)) {
                auto do_alloc = [&](auto&& len) {
                    allocate(f, data_of(ident), brgen::concat("alignof(", typeof_(arr_ty->element_type), ")"),
                             brgen::concat(len, "* sizeof(", data_of(ident), "[0])"), brgen::concat("sizeof(", data_of(ident), "[0])"), false);
                    c_w.writeln(length_of(ident), " = ", len, ";");
                };
                auto for_loop = [&](auto&& ident, auto&& length, bool need_buffer_length_check, bool need_array_length_check, bool dynamic_array) {
                    write_array_for_loop(f, ident, length, arr_ty->element_type, false, need_buffer_length_check, need_array_length_check, dynamic_array);
                };
                if (arr_ty->bit_size) {
                    auto len = brgen::nums(arr_ty->bit_size.value() / futils::bit_per_byte);
                    if (need_length_check) {
                        check_buffer_length(f, len);
                    }
                    for_loop(ident, len, false, false, false);
                }
                else if (arr_ty->length_value) {
                    for_loop(ident, brgen::nums(*arr_ty->length_value), true, false, false);
                }
                else if (ast::is_any_range(arr_ty->length)) {
                    auto fmt = ast::as<ast::Format>(f->belong.lock());
                    assert(fmt);
                    auto tail_size = fmt->body->struct_type->fixed_tail_size;
                    c_w.writeln("if(", brgen::nums(tail_size), " + ", io_(buffer_offset), " > ", io_(buffer_size), ") {");
                    write_return_error(f, "require ", brgen::nums(tail_size), " bytes for tail size but not");
                    c_w.writeln("}");

                    if (arr_ty->element_type->bit_size) {
                        auto len_in_bytes = brgen::concat("(", io_(buffer_size), " - ", io_(buffer_offset), " - ", brgen::nums(tail_size), ")");
                        auto per_elem = brgen::nums(*arr_ty->element_type->bit_size / futils::bit_per_byte);
                        if (per_elem != "1") {
                            c_w.writeln("if(", len_in_bytes, " % ", per_elem, " != 0) {");
                            write_return_error(f, "require ", per_elem, " bytes aligned but not");
                            c_w.writeln("}");
                        }
                        auto len = brgen::concat("(", len_in_bytes, " / ", per_elem, ")");
                        auto len_var = "tmp_len_" + brgen::nums(get_seq());
                        c_w.writeln("size_t ", len_var, " = ", len, ";");
                        do_alloc(len_var);
                        for_loop(data_of(ident), len_var, false, false, true);
                    }
                    else {
                        auto len_in_bytes_without_tail = brgen::concat("(", io_(buffer_size), " - ", brgen::nums(tail_size), ")");
                        auto tmp_var = "tmp_save_buffer_size" + brgen::nums(get_seq());
                        c_w.writeln("size_t ", tmp_var, " = ", io_(buffer_size), ";");
                        c_w.writeln(io_(buffer_size), " = ", len_in_bytes_without_tail, ";");
                        for_loop(ident, "<EOF>", true, true, true);
                        c_w.writeln(io_(buffer_size), " = ", tmp_var, ";");
                    }
                }
                else {
                    auto len = str.to_string(arr_ty->length);
                    auto len_var = "tmp_len_" + brgen::nums(get_seq());
                    c_w.writeln("size_t ", len_var, " = ", len, ";");
                    if (arr_ty->element_type->bit_size) {
                        check_buffer_length(f, len_var, " * sizeof(", data_of(ident), "[0])");
                        do_alloc(len_var);
                        for_loop(data_of(ident), len_var, false, false, true);
                    }
                    else {
                        do_alloc(len_var);
                        for_loop(data_of(ident), len_var, true, false, true);
                    }
                }
            }
            else if (auto ident_ty = ast::as<ast::IdentType>(typ)) {
                write_type_decode(f, ident, ident_ty->base.lock(), need_length_check);
            }
            else if (auto s = ast::as<ast::StructType>(typ)) {
                if (auto fmt = ast::as<ast::Format>(s->base.lock())) {
                    assert(ast::as<ast::Format>(f->belong.lock()));
                    auto from_fmt = ast::cast_to<ast::Format>(f->belong.lock());
                    auto to_fmt = ast::cast_to<ast::Format>(s->base.lock());
                    switch_io(from_fmt, to_fmt, [&](auto&& cast, auto&& restore_io) {
                        auto tmp_ret = "tmp_ret_" + brgen::nums(get_seq());
                        c_w.writeln("int ", tmp_ret, " = ", fmt->ident->ident, "_decode_ex(&", ident, ",", cast, ");");
                        restore_io();
                        c_w.writeln("if(", tmp_ret, " == 0) {");
                        c_w.indent_writeln("return 0;");
                        c_w.writeln("}");
                    });
                }
            }
            else if (auto e = ast::as<ast::EnumType>(typ)) {
                auto b = e->base.lock()->base_type;
                auto base_ty = ast::as<ast::IntType>(b);
                if (base_ty) {
                    auto tmp = "tmp_" + brgen::nums(get_seq());
                    auto c_typ = typeof_(b);
                    c_w.writeln(c_typ, " ", tmp, ";");
                    encode_decode_int_field(f, base_ty, tmp, false, need_length_check);
                    c_w.writeln(ident, " = (", typeof_(typ), ")", tmp, ";");
                }
            }
            else if (auto c = ast::as<ast::StrLiteralType>(typ)) {
                auto len = brgen::nums(c->base.lock()->length);
                if (need_length_check) {
                    check_buffer_length(f, len);
                }
                c_w.writeln("for(size_t i = 0; i < ", len, "; i++) {");
                c_w.indent_writeln("if (", io_(buffer), "[", io_(buffer_offset), " + i] != ", c->base.lock()->value, "[i]) {");
                {
                    auto scope = c_w.indent_scope();
                    write_return_error(f, "require ", c->base.lock()->value, "[i] but got ", io_(buffer), "[", io_(buffer_offset), " + i]");
                }
                c_w.writeln("}");
            }
            else {
                if (!typ) {
                    write_return_error(f, "null type");
                }
                else {
                    write_return_error(f, "non supported type: ", ast::node_type_to_string(typ->node_type));
                }
            }
        }

        void write_field_decode(const std::shared_ptr<ast::Field>& field, bool need_length_check) {
            if (field->bit_alignment != field->eventual_bit_alignment) {
                return;
            }
            if (auto found = bit_fields.find(field); found != bit_fields.end()) {
                auto ident = str.to_string(found->second.bit_field->ident);
                auto int_ty = ast::as<ast::IntType>(found->second.bit_field->field_type);
                encode_decode_int_field(field, int_ty, ident, false, need_length_check);
                return;
            }
            futils::helper::DynDefer d;
            if (field->arguments && field->arguments->sub_byte_length) {
                auto len_in_bytes = str.to_string(field->arguments->sub_byte_length);
                check_buffer_length(field, len_in_bytes);
                auto tmp_var = "tmp_save_buffer_size" + brgen::nums(get_seq());
                c_w.writeln("size_t ", tmp_var, " = ", io_(buffer_size), ";");
                c_w.writeln(io_(buffer_size), " = ", io_(buffer_offset), " + ", len_in_bytes, ";");
                d = futils::helper::DynDefer([=, this] {
                    c_w.writeln("if (", io_(buffer_size), " != ", io_(buffer_offset), ") {");
                    write_return_error(field, "require ", len_in_bytes, " bytes but got ", io_(buffer_size), " is not enough");
                    c_w.writeln("}");
                    c_w.writeln(io_(buffer_size), " = ", tmp_var, ";");
                });
            }
            auto ident = str.to_string(field->ident);
            write_type_decode(field, ident, field->field_type, need_length_check);
            if (field->arguments && field->arguments->arguments.size() == 1) {
                if (auto typ = ast::as<ast::IntType>(field->field_type)) {
                    auto val = str.to_string(field->arguments->arguments[0]);
                    c_w.writeln("if(", ident, " != ", val, ") {");
                    write_return_error(field, "require ", val, " but got ", ident, " is not equal");
                    c_w.writeln("}");
                }
            }
        }

        void write_format_input_type_state_variables(const std::shared_ptr<ast::Format>& typ) {
            for (auto& s : typ->state_variables) {
                auto state_var = s.lock();
                auto ty = typeof_(state_var->field_type);
                h_w.writeln(ty, "* ", state_var->ident->ident, ";");
            }
        }

        void write_format_input_type(const std::shared_ptr<ast::Format>& typ) {
            auto write_error_callback = [&] {
                if (omit_error_callback) {
                    return;
                }
                h_w.writeln("char* ", error_msg, ";");
                h_w.writeln("size_t ", error_msg_size, ";");
                h_w.writeln("void (*", error_msg_callback, ")(char** buf,size_t* size,const char* msg);");
            };
            h_w.writeln("typedef struct ", typ->ident->ident, "DecodeInput {");
            {
                auto scope = h_w.indent_scope();
                write_error_callback();
                h_w.writeln("const uint8_t* ", buffer, ";");
                h_w.writeln("size_t ", buffer_size, ";");
                h_w.writeln("size_t ", buffer_size_origin, ";");
                h_w.writeln("size_t ", buffer_offset, ";");
                h_w.writeln("size_t ", buffer_bit_offset, ";");
                h_w.writeln("void** ", allocation_holder, ";");
                h_w.writeln("size_t ", allocation_holder_size, ";");
                h_w.writeln("void (*", alloc, ")(void** p,",
                            "void*** holder,",
                            "size_t* holder_size ,size_t size,size_t align,size_t elem_size,",
                            "int may_realloc,",
                            "const uint8_t* input_buffer,",
                            "size_t input_buffer_size,",
                            "size_t input_buffer_offset",
                            ");");
                write_format_input_type_state_variables(typ);
            }
            h_w.writeln("} ", typ->ident->ident, "DecodeInput;");
            h_w.writeln();
            h_w.writeln("typedef struct ", typ->ident->ident, "EncodeInput {");
            {
                auto scope = h_w.indent_scope();
                write_error_callback();
                h_w.writeln("uint8_t* ", buffer, ";");
                h_w.writeln("size_t ", buffer_size, ";");
                h_w.writeln("size_t ", buffer_offset, ";");
                h_w.writeln("size_t ", buffer_bit_offset, ";");
                write_format_input_type_state_variables(typ);
            }
            h_w.writeln("} ", typ->ident->ident, "EncodeInput;");
        }

        bool is_io_compatible(const std::shared_ptr<ast::Format>& from, const std::shared_ptr<ast::Format>& to) {
            if (from->state_variables.size() != to->state_variables.size()) {
                return false;
            }
            for (size_t i = 0; i < from->state_variables.size(); i++) {
                if (from->state_variables[i].lock() != to->state_variables[i].lock()) {
                    return false;
                }
            }
            return true;  // TODO(on-keyday): currently, no state is saved but in the future, it may be necessary to check the state.
        }

        // cb is (cast :string, restore_io :()->void)
        void switch_io(const std::shared_ptr<ast::Format>& from, const std::shared_ptr<ast::Format>& to,
                       auto&& cb) {
            if (is_io_compatible(from, to)) {
                // only cast is needed
                cb(brgen::concat("(", io_input_type(to->ident->ident), "*)(", io_input(), ")"), []() {});
            }
            else {
                auto tmp = "tmp_" + brgen::nums(get_seq());
                c_w.writeln(io_input_type(to->ident->ident), " ", tmp, ";");
                auto ptr = brgen::concat("(&", tmp, ")");
                copy_io(io_input(), ptr, to);
                cb(ptr, [&] {
                    copy_io(ptr, io_input(), nullptr);
                });
            }
        }

        void copy_io(auto&& from, auto&& to, const std::shared_ptr<ast::Format>& fmt) {
            // TODO(on-keyday): copy io state
            copy_io_base(from, to);
            if (fmt) {
                for (auto& s : fmt->state_variables) {
                    auto state_var = s.lock();
                    c_w.writeln(to, "->", state_var->ident->ident, " = ", from, "->", state_var->ident->ident, ";");
                }
            }
        }

        void copy_io_base(auto&& from, auto&& to) {
            if (!omit_error_callback) {
                c_w.writeln(to, "->", error_msg, " = ", from, "->", error_msg, ";");
                c_w.writeln(to, "->", error_msg_size, " = ", from, "->", error_msg_size, ";");
                c_w.writeln(to, "->", error_msg_callback, " = ", from, "->", error_msg_callback, ";");
            }
            c_w.writeln(to, "->", buffer, " = ", from, "->", buffer, ";");
            c_w.writeln(to, "->", buffer_size, " = ", from, "->", buffer_size, ";");
            c_w.writeln(to, "->", buffer_offset, " = ", from, "->", buffer_offset, ";");
            c_w.writeln(to, "->", buffer_bit_offset, " = ", from, "->", buffer_bit_offset, ";");
            if (!encode) {
                c_w.writeln(to, "->", buffer_size_origin, " = ", from, "->", buffer_size_origin, ";");
                c_w.writeln(to, "->", allocation_holder, " = ", from, "->", allocation_holder, ";");
                c_w.writeln(to, "->", allocation_holder_size, " = ", from, "->", allocation_holder_size, ";");
                c_w.writeln(to, "->", alloc, " = ", from, "->", alloc, ";");
            }
        }

        void write_if(ast::If* if_) {
            c_w.writeln("if (", str.to_string(if_->cond), ") {");
            {
                auto scope = c_w.indent_scope();
                write_node(if_->then);
            }
            c_w.writeln("}");
            if (if_->els) {
                c_w.writeln("else {");
                {
                    auto scope = c_w.indent_scope();
                    write_node(if_->els);
                }
                c_w.writeln("}");
            }
        }

        void write_match(ast::Match* m) {
            std::string cond;
            if (m->cond) {
                cond = str.to_string(m->cond);
            }
            else {
                cond = "1";
            }
            bool els_ = false;
            for (auto branch : m->branch) {
                if (els_) {
                    c_w.writeln("else ");
                }
                else {
                    els_ = true;
                }
                if (ast::is_any_range(branch->cond)) {
                    c_w.writeln("{");
                }
                else {
                    c_w.writeln("if (", cond, " == ", str.to_string(branch->cond), ") {");
                }
                {
                    auto scope = c_w.indent_scope();
                    write_node(branch->then);
                }
                c_w.writeln("}");
            }
        }

        void write_node(const std::shared_ptr<ast::Node>& node) {
            if (auto p = ast::as<ast::IndentBlock>(node)) {
                for (auto& n : p->elements) {
                    write_node(n);
                }
            }
            if (auto s = ast::as<ast::ScopedStatement>(node)) {
                write_node(s->statement);
            }
            if (auto f = ast::as<ast::Field>(node)) {
                if (encode) {
                    write_field_encode(ast::cast_to<ast::Field>(node), format_need_length_check);
                }
                else {
                    write_field_decode(ast::cast_to<ast::Field>(node), format_need_length_check);
                }
            }
            if (auto b = ast::as<ast::Binary>(node);
                b &&
                (b->op == ast::BinaryOp::define_assign || b->op == ast::BinaryOp::const_assign)) {
                auto ident = ast::as<ast::Ident>(b->left);
                assert(ident);
                auto value = str.to_string(b->right);
                // TODO(on-keyday): support non integer type
                auto left_typ = typeof_(b->left->expr_type);
                c_w.writeln(left_typ, " ", ident->ident, " = ", value, ";");
                str.map_ident(ast::cast_to<ast::Ident>(b->left), ident->ident);
            }
            if (auto i = ast::as<ast::If>(node)) {
                write_if(i);
            }
            if (auto m = ast::as<ast::Match>(node)) {
                write_match(m);
            }
        }

        void check_state_variables(const std::shared_ptr<ast::Format>& fmt) {
            for (auto& s : fmt->state_variables) {
                auto state_var = s.lock();
                auto ty = typeof_(state_var->field_type);
                c_w.writeln("if (", io_input(), "->", state_var->ident->ident, " == NULL) {");
                write_return_error(fmt, "state variable ", state_var->ident->ident, " is null");
                c_w.writeln("}");
                str.map_ident(state_var->ident, io_input(), "->", state_var->ident->ident);
            }
        }

        void write_format_encode(const std::shared_ptr<ast::Format>& fmt) {
            encode = true;
            h_w.writeln("int ", fmt->ident->ident, "_encode_ex(const ", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* output_);");
            c_w.writeln("int ", fmt->ident->ident, "_encode_ex(const ", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* output_) {");
            {
                auto scope = c_w.indent_scope();
                check_state_variables(fmt);
                if (fmt->body->struct_type->bit_size) {
                    auto len = (fmt->body->struct_type->bit_size.value() + futils::bit_per_byte - 1) / futils::bit_per_byte;
                    check_buffer_length(fmt, brgen::nums(len));
                }
                format_need_length_check = !fmt->body->struct_type->bit_size.has_value();
                write_node(fmt->body);
                c_w.writeln("return 0;");
            }
            c_w.writeln("}");
        }

        void write_format_decode(const std::shared_ptr<ast::Format>& fmt) {
            encode = false;
            h_w.writeln("int ", fmt->ident->ident, "_decode_ex(", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* input_);");
            c_w.writeln("int ", fmt->ident->ident, "_decode_ex(", fmt->ident->ident, "* this_, ", io_input_type(fmt->ident->ident), "* input_) {");
            {
                auto scope = c_w.indent_scope();
                check_state_variables(fmt);
                if (fmt->body->struct_type->bit_size) {
                    auto len = (fmt->body->struct_type->bit_size.value() + futils::bit_per_byte - 1) / futils::bit_per_byte;
                    check_buffer_length(fmt, brgen::nums(len));
                }
                format_need_length_check = !fmt->body->struct_type->bit_size.has_value();
                write_node(fmt->body);
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

        void write_state(const std::shared_ptr<ast::State>& s) {
            auto& ident = s->ident->ident;
            brgen::writer::Writer tmp;
            write_struct_type("${THIS}", tmp, s->body->struct_type, true);
            h_w.writeln("typedef struct ", ident, " {");
            {
                auto scope = h_w.indent_scope();
                h_w.write_unformatted(tmp.out());
            }
            h_w.writeln("} ", ident, ";");
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
            auto typ = fmt->body->struct_type;
            auto ident = fmt->ident->ident;
            brgen::writer::Writer tmp;
            write_struct_type("${THIS}", tmp, typ, false);
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
                return typeof_(t);
            };
            str.cast_handler = [&](brgen::ast::tool::Stringer& s, const std::shared_ptr<brgen::ast::Cast>& t) {
                auto expr = s.args_to_string(t->arguments);
                return brgen::concat("(", typeof_(t->expr_type), ")(", expr, ")");
            };
            h_w.writeln("//Code generated by json2c");
            h_w.writeln("#pragma once");
            h_w.writeln("#include <stddef.h>");
            h_w.writeln("#include <stdint.h>");
            h_w.writeln("#ifndef __cplusplus");
            h_w.writeln("#include <stdbool.h>");
            h_w.writeln("#include <stdalign.h>");
            h_w.writeln("#endif");
            if (h_path != "") {
                c_w.writeln("//Code generated by json2c");
                c_w.writeln("#include \"", h_path, "\"");
                if (use_memcpy) {
                    c_w.writeln("#include <string.h>");
                }
            }
            else {
                if (use_memcpy) {
                    h_w.writeln("#include <string.h>");
                }
            }
            str.this_access = "(*this_)";
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
                if (auto s = ast::as<ast::State>(fmt); s) {
                    write_state(ast::cast_to<ast::State>(fmt));
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
