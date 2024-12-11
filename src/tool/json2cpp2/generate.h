/*license*/
#pragma once

#include <helper/expected.h>
#include <core/ast/ast.h>
#include <writer/writer.h>
#include <core/ast/tool/stringer.h>
#include <core/ast/tool/sort.h>
#include <core/ast/line_map.h>
#include <core/ast/tool/tmp_ident.h>

namespace j2cp2 {
    namespace ast = brgen::ast;
    struct BitFieldMeta {
        std::string field_name;
        std::vector<std::shared_ptr<ast::Field>> fields;
        ast::Endian endian;
        std::optional<size_t> sum;
        size_t prefix_sum = 0;
    };

    struct AnonymousStruct {
        std::string type_name;
        std::string variant_name;
    };

    struct Context {
        bool encode = false;
        ast::Endian endian = ast::Endian::unspec;
        ast::Endian global_endian = ast::Endian::big;
        bool dynamic_endian = false;

       private:
        std::string prefix_;

       public:
        const std::string& prefix() const {
            return prefix_;
        }
        auto add_prefix(const std::string& s) {
            auto old = std::move(prefix_);
            prefix_ = old + s;
            return futils::helper::defer([this, old = std::move(old)] {
                prefix_ = std::move(old);
            });
        }

        std::string endian_text(ast::Endian e) {
            if (e == ast::Endian::big) {
                return "true";
            }
            if (e == ast::Endian::little) {
                return "false";
            }
            // unspecified
            if (dynamic_endian) {
                return "dynamic_endian____";
            }
            if (endian == ast::Endian::little) {
                return "false";
            }
            if (endian == ast::Endian::big) {
                return "true";
            }
            return global_endian == ast::Endian::big ? "true" : "false";
        }
    };

    struct LaterInfo {
        size_t size = 0;
        std::shared_ptr<ast::Field> next_field;
    };

    struct PrefixedBitField {
        std::string field_name;
        std::shared_ptr<ast::StructUnionType> union_type;
        std::shared_ptr<ast::Field> prefix;
        std::shared_ptr<ast::Field> union_field;
        std::shared_ptr<ast::UnionType> candidates;
    };

    struct Generator {
        brgen::writer::Writer w;
        size_t seq = 0;

        constexpr auto get_seq() {
            auto s = seq++;
            return s;
        }
        ast::tool::Stringer str;
        std::map<ast::Field*, BitFieldMeta> bit_fields;
        std::set<ast::Field*> bit_fields_part;
        std::map<ast::Field*, LaterInfo> later_size;
        std::map<std::shared_ptr<ast::StructType>, AnonymousStruct> anonymous_struct;
        std::map<ast::EnumMember*, std::string> enum_member_map;
        std::map<ast::StructUnionType*, PrefixedBitField> prefixed_bit_field;
        std::vector<ast::LineMap> line_map;
        std::vector<std::function<void()>> accessor_funcs;
        bool enable_line_map = false;
        bool use_error = false;
        bool use_variant = false;
        bool use_overflow_check = false;
        bool enum_stringer = false;
        bool add_visit = false;
        bool use_constexpr = false;
        ast::Format* current_format = nullptr;
        Context ctx;
        std::vector<std::string> struct_names;
        std::string bytes_type = "::futils::view::rvec";
        std::string vector_type = "std::vector";

        auto write_return_error(brgen::writer::Writer& w, ast::Field* field, auto&&... msg) {
            if (use_error) {
                auto f = ast::as<ast::Format>(field->belong.lock());
                assert(f);
                w.writeln("return ::futils::error::Error<>(\"", ctx.encode ? "encode: " : "decode: ",
                          f->ident->ident, "::", field->ident->ident, ": ", msg..., "\",::futils::error::Category::lib);");
            }
            else {
                w.writeln("return false;");
            }
        }

        auto write_return_error(ast::Field* field, auto&&... msg) {
            write_return_error(w, field, msg...);
        }

        auto write_return_error(ast::Format* fmt, auto&&... msg) {
            if (use_error) {
                w.writeln("return ::futils::error::Error<>(\"", ctx.encode ? "encode: " : "decode: ",
                          fmt->ident->ident, ": ", msg..., "\",::futils::error::Category::lib);");
            }
            else {
                w.writeln("return false;");
            }
        }

        void write_return_ok() {
            if (use_error) {
                w.writeln("return ::futils::error::Error<>();");
            }
            else {
                w.writeln("return true;");
            }
        }

        void map_line(brgen::lexer::Loc l) {
            if (!enable_line_map) {
                return;
            }
            line_map.push_back({l, w.line_count()});
        }

        void escape_keyword(std::string& s) {
            if (s == "class") {
                s = "class_";
            }
            if (s == "public") {
                s = "public_";
            }
        }

        std::pair<std::optional<size_t>, size_t> sum_fields(std::vector<std::shared_ptr<ast::Field>>& non_align) {
            size_t sum = 0;
            for (auto& field : non_align) {
                if (field->field_type->bit_size) {
                    sum += *field->field_type->bit_size;
                }
                else {
                    return {std::nullopt, sum};
                }
            }
            return {sum, sum};
        }

        void write_bit_fields_full_sum(std::string_view prefix, std::vector<std::shared_ptr<ast::Field>>& non_align, size_t bit_size, ast::Endian endian) {
            w.write("::futils::binary::flags_t<std::uint", brgen::nums(bit_size), "_t");
            for (auto& n : non_align) {
                auto type = n->field_type;
                if (auto ident = ast::as<ast::IdentType>(type); ident) {
                    type = ident->base.lock();
                }
                if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                    w.write(", ", brgen::nums(*int_ty->bit_size));
                    if (endian == ast::Endian::unspec) {
                        endian = int_ty->endian;  // first field decides endian
                    }
                }
                else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                    auto bit_size = enum_ty->bit_size;
                    w.write(", ", brgen::nums(*bit_size));
                    if (endian == ast::Endian::unspec) {
                        auto base_int = ast::as<ast::IntType>(enum_ty->base.lock());
                        if (base_int) {
                            endian = base_int->endian;
                        }
                    }
                }
            }
            auto seq = get_seq();
            w.writeln("> flags_", brgen::nums(seq), "_;");
            size_t i = 0;
            for (auto& n : non_align) {
                auto type = n->field_type;
                if (auto ident = ast::as<ast::IdentType>(type); ident) {
                    type = ident->base.lock();
                }
                if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                    map_line(n->loc);
                    w.writeln("bits_flag_alias_method(flags_", brgen::nums(seq), "_,", brgen::nums(i), ",", n->ident->ident, ");");
                    str.map_ident(n->ident, prefix, ".", n->ident->ident + "()");
                }
                else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                    auto enum_ = enum_ty->base.lock();
                    line_map.push_back({n->loc, w.line_count()});
                    w.writeln("bits_flag_alias_method_with_enum(flags_", brgen::nums(seq), "_,", brgen::nums(i), ",", n->ident->ident, ",", enum_->ident->ident, ");");
                    str.map_ident(n->ident, prefix, ".", n->ident->ident + "()");
                }
                bit_fields_part.insert(n.get());
                i++;
            }
            bit_fields[non_align.back().get()] = {brgen::concat("flags_", brgen::nums(seq), "_"), non_align, endian, bit_size, bit_size};
        }

        void write_bit_fields_prefixed(std::string_view prefix, std::vector<std::shared_ptr<ast::Field>>& non_align, size_t prefix_sum) {
            if (auto t = ast::as<ast::StructUnionType>(non_align[1]->field_type); t && ast::as<ast::Ident>(t->cond->expr) && t->exhaustive && t->union_fields.size() == 1) {
                auto [base, _] = *ast::tool::lookup_base(ast::cast_to<ast::Ident>(t->cond->expr));
                if (non_align[0]->ident != base) {
                    return;
                }
                size_t max_field_size = 0;
                auto union_f = t->union_fields[0].lock();
                auto union_ty = ast::cast_to<ast::UnionType>(union_f->field_type);
                for (auto& s : union_ty->candidates) {
                    auto field = s->field.lock();
                    if (!field) {
                        return;
                    }
                    if (auto ty = ast::as<ast::IntType>(field->field_type); ty) {
                        if ((*ty->bit_size + prefix_sum) % 8 != 0) {
                            return;
                        }
                        max_field_size = std::max(max_field_size, *ty->bit_size + prefix_sum);
                    }
                    else {
                        return;
                    }
                }
                if (max_field_size > 64) {
                    return;
                }
                auto seq = get_seq();
                auto bit_field = brgen::concat("flags_", brgen::nums(seq), "_");
                w.writeln("::futils::binary::flags_t<std::uint", brgen::nums(max_field_size), "_t,", brgen::nums(prefix_sum), ",", brgen::nums(max_field_size - prefix_sum), "> ", bit_field, ";");
                w.writeln("bits_flag_alias_method(flags_", brgen::nums(seq), "_,0,", non_align[0]->ident->ident, ");");
                w.writeln("bits_flag_alias_method(flags_", brgen::nums(seq), "_,1,", union_f->ident->ident, ");");
                bit_fields_part.insert(non_align[0].get());
                bit_fields[non_align[1].get()] = {bit_field, non_align, ast::Endian::unspec, std::nullopt, prefix_sum};
                str.map_ident(non_align[0]->ident, prefix, ".", non_align[0]->ident->ident + "()");
                str.map_ident(union_f->ident, prefix, ".", union_f->ident->ident + "()");
                prefixed_bit_field[t] = PrefixedBitField{
                    .field_name = bit_field,
                    .union_type = ast::cast_to<ast::StructUnionType>(non_align[1]->field_type),
                    .prefix = non_align[0],
                    .union_field = union_f,
                    .candidates = union_ty,
                };
            }
        }

        void write_bit_fields(std::string_view prefix, std::vector<std::shared_ptr<ast::Field>>& non_align, /* size_t bit_size,*/ ast::Endian endian) {
            auto [sum, prefix_sum] = sum_fields(non_align);
            if (sum) {
                write_bit_fields_full_sum(prefix, non_align, *sum, endian);
            }
            // special case
            // like this
            // format A:
            //  prefix: u1
            //  match prefix:
            //     0 => len :u7
            //     1 => len :u31
            else if (non_align.size() == 2 && prefix_sum == non_align[0]->field_type->bit_size && prefix_sum < 8) {
                write_bit_fields_prefixed(prefix, non_align, prefix_sum);
            }
        }

        std::string get_type_name(const std::shared_ptr<ast::Type>& type, bool align = false) {
            if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                auto size = *int_ty->bit_size;
                if (align) {
                    size = ast::aligned_bit(*int_ty->bit_size);
                }
                return brgen::concat("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(size), "_t");
            }
            if (auto typ = ast::as<ast::FloatType>(type); typ) {
                return brgen::concat("::futils::binary::float", brgen::nums(*typ->bit_size), "_t");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                return enum_ty->base.lock()->ident->ident;
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                if (arr_ty->non_dynamic_allocation) {
                    auto len = str.to_string(arr_ty->length);
                    auto typ = get_type_name(arr_ty->element_type);
                    return brgen::concat("std::array<", typ, ",", len, ">");
                }
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->element_type); int_ty && int_ty->bit_size == 8) {
                    return bytes_type;
                }
                return vector_type + "<" + get_type_name(arr_ty->element_type) + ">";
            }
            if (auto struct_ty = ast::as<ast::StructType>(type); struct_ty) {
                if (auto l = struct_ty->base.lock()) {
                    if (auto fmt = ast::as<ast::Format>(l)) {
                        return fmt->ident->ident;
                    }
                }
            }
            if (auto ident_ty = ast::as<ast::IdentType>(type); ident_ty) {
                return ident_ty->ident->ident;
            }
            return "";
        }

        void check_variant_alternative(const std::shared_ptr<ast::StructType>& s, bool as_err = false) {
            if (use_variant) {
                if (auto found = anonymous_struct.find(s); found != anonymous_struct.end()) {
                    if (s->fields.size()) {
                        auto& typ = found->second;
                        w.writeln("if(!std::holds_alternative<", typ.type_name, ">(", typ.variant_name, ")) {");
                        if (as_err) {
                            {
                                auto indent = w.indent_scope();
                                write_return_error(current_format, typ.variant_name, " variant alternative ", typ.type_name, " is not set");
                            }
                        }
                        else {
                            w.indent_writeln("return std::nullopt;");
                        }
                        w.writeln("}");
                    }
                }
            }
        }

        void set_variant_alternative(const std::shared_ptr<ast::StructType>& s) {
            if (use_variant) {
                if (auto found = anonymous_struct.find(s); found != anonymous_struct.end()) {
                    auto& typ = found->second;
                    w.writeln("if(!std::holds_alternative<", typ.type_name, ">(", typ.variant_name, ")) {");
                    w.indent_writeln(typ.variant_name, " = ", typ.type_name, "();");
                    w.writeln("}");
                }
            }
        }

        void variant_alternative(const std::shared_ptr<ast::StructType>& s, bool as_err = false) {
            if (ctx.encode) {
                check_variant_alternative(s, as_err);
            }
            else {
                set_variant_alternative(s);
            }
        }

        void write_getter(std::string_view prefix, ast::UnionType* union_ty, const std::shared_ptr<ast::Field>& union_field, const std::string& match_cond) {
            // write getter funca
            map_line(union_field->loc);
            auto make_access = [&](const std::shared_ptr<ast::Field>& f) {
                auto a = str.to_string(f->ident);
                check_variant_alternative(f->belong_struct.lock());
                map_line(f->loc);
                w.writeln("return ", a, ";");
            };
            if (use_constexpr) {
                w.write("constexpr ");
            }
            w.writeln("std::optional<", get_type_name(union_ty->common_type), "> ", union_field->ident->ident, "() const {");
            {
                auto indent = w.indent_scope();
                bool has_els = false;
                bool end_else = false;
                for (auto& c : union_ty->candidates) {
                    auto cond = c->cond.lock();
                    if (cond) {
                        auto defs = ast::tool::collect_defined_ident(cond);
                        for (auto& d : defs) {
                            code_one_node(d);
                        }
                        auto cond_s = str.to_string(cond);
                        map_line(c->loc);
                        w.writeln("if (", cond_s, "==", match_cond, ") {");
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
                        end_else = true;
                    }
                }
                if (!end_else) {
                    w.writeln("return std::nullopt;");
                }
                str.map_ident(union_field->ident, "(*", prefix, "." + union_field->ident->ident + "())");
            }
            w.writeln("}");
        }

        std::shared_ptr<ast::Ident> has_ident_len(const std::shared_ptr<ast::Type>& typ) {
            if (auto arr_ty = ast::as<ast::ArrayType>(typ);
                arr_ty && !arr_ty->length_value) {
                if (auto ident_len = ast::as<ast::Ident>(arr_ty->length)) {
                    return ast::cast_to<ast::Ident>(arr_ty->length);
                }
            }
            return nullptr;
        }

        void maybe_write_auto_length_set(std::string_view to_set, const std::shared_ptr<ast::Type>& typ) {
            if (auto ident_len = has_ident_len(typ)) {
                auto [base, _] = *ast::tool::lookup_base(ident_len);
                if (auto f = ast::as<ast::Field>(base->base.lock()); f && f->field_type->bit_size) {
                    auto typ = get_type_name(f->field_type);
                    auto size = *f->field_type->bit_size;
                    std::uint64_t max_size = (std::uint64_t(1) << size) - 1;
                    w.writeln("if(", to_set, "> 0x", brgen::nums(max_size, 16), ") {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
                    }
                    w.writeln("}");
                    auto ident = str.to_string(f->ident);
                    map_line(f->loc);
                    if (ast::as<ast::UnionType>(f->field_type)) {
                        while (ident.back() != '(') {
                            ident.pop_back();
                        }
                        ident.erase(0, 2);  // remove (*
                        auto fmt = ast::as<ast::Format>(f->belong.lock());
                        std::string args;
                        if (fmt) {
                            args = state_variable_to_argument(fmt->body->struct_type, false);
                        }
                        w.writeln(ident, to_set, args, ");");
                    }
                    else {
                        w.writeln(ident, " = ", to_set, ";");
                    }
                }
            }
        }

        bool maybe_set_dynamic_to_static_array(const std::shared_ptr<ast::Type>& to, const std::shared_ptr<ast::Type>& from, std::string_view dynamic_size) {
            if (auto to_arr = ast::as<ast::ArrayType>(to)) {
                if (auto from_arr = ast::as<ast::ArrayType>(from)) {
                    if (to_arr->length_value && !from_arr->length_value) {
                        auto len = brgen::nums(*to_arr->length_value);
                        w.writeln("if(", dynamic_size, "!= ", len, ") {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                        return true;
                    }
                }
            }
            return false;
        }

        void write_common_type_accessor(std::string prefix, const std::string& cond_u, const std::shared_ptr<ast::Field>& uf, ast::UnionType* ut) {
            // write getter func
            map_line(uf->loc);
            if (use_constexpr) {
                w.write("constexpr ");
            }
            auto fmt = ast::as<ast::Format>(uf->belong.lock());
            std::string set_args, get_args;
            std::string def_member;
            if (fmt) {
                def_member = fmt->ident->ident + "::";
                set_args = state_variable_to_argument(fmt->body->struct_type, true);
                if (set_args.size()) {
                    get_args = set_args;
                    get_args.erase(0, 1);
                }
            }
            w.writeln("std::optional<", get_type_name(ut->common_type), "> ", uf->ident->ident, "(", get_args, ") const;");
            w.writeln("bool ", uf->ident->ident, "(", get_type_name(ut->common_type), "&& v", set_args, ");");
            w.writeln("bool ", uf->ident->ident, "(const ", get_type_name(ut->common_type), "& v", set_args, ");");
            auto fn = [=, this] {
                // write getter func
                map_line(uf->loc);
                if (use_constexpr) {
                    w.write("constexpr ");
                }
                w.writeln("inline std::optional<", get_type_name(ut->common_type), "> ", def_member, uf->ident->ident, "(", get_args, ") const {");
                {
                    auto indent = w.indent_scope();
                    auto make_access = [&](const std::shared_ptr<ast::Field>& f) {
                        auto a = str.to_string(f->ident);
                        check_variant_alternative(f->belong_struct.lock());
                        map_line(f->loc);
                        w.writeln("return ", a, ";");
                    };
                    bool has_els = false;
                    bool end_else = false;
                    for (auto& c : ut->candidates) {
                        auto cond = c->cond.lock();
                        if (!ast::is_any_range(cond)) {
                            auto defs = ast::tool::collect_defined_ident(cond);
                            for (auto& d : defs) {
                                code_one_node(d);
                            }
                            auto cond_s = str.to_string_impl(cond, false);
                            map_line(c->loc);
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
                            end_else = true;
                        }
                    }
                    if (!end_else) {
                        w.writeln("return std::nullopt;");
                    }
                    auto args = state_variable_to_argument(fmt->body->struct_type, false);
                    args.erase(0, 1);
                    str.map_ident(uf->ident, "(*", prefix, ".", uf->ident->ident + "(", args, "))");
                }
                w.writeln("}");

                // write setter func
                auto write_setter = [&](bool mov) {
                    map_line(uf->loc);
                    if (use_constexpr) {
                        w.write("constexpr ");
                    }
                    if (mov) {
                        w.writeln("inline bool ", def_member, uf->ident->ident, "(", get_type_name(ut->common_type), "&& v", set_args, ") {");
                    }
                    else {
                        w.writeln("inline bool ", def_member, uf->ident->ident, "(const ", get_type_name(ut->common_type), "& v", set_args, ") {");
                    }
                    {
                        auto indent = w.indent_scope();
                        auto make_access = [&](const std::shared_ptr<ast::Field>& f) {
                            map_line(f->loc);
                            set_variant_alternative(f->belong_struct.lock());
                            auto a = str.to_string(f->ident);
                            maybe_write_auto_length_set("v.size()", f->field_type);
                            if (maybe_set_dynamic_to_static_array(f->field_type, ut->common_type, "v.size()")) {
                                w.writeln("std::copy(v.begin(), v.end(), ", a, ".begin());");
                            }
                            else {
                                if (mov) {
                                    w.writeln(a, " = std::move(v);");
                                }
                                else {
                                    w.writeln(a, " = v;");
                                }
                            }
                            w.writeln("return true;");
                        };
                        bool has_els = false;
                        bool end_else = false;
                        for (auto& c : ut->candidates) {
                            auto cond = c->cond.lock();
                            if (!ast::is_any_range(cond)) {
                                auto defs = ast::tool::collect_defined_ident(cond);
                                for (auto& d : defs) {
                                    code_one_node(d);
                                }
                                auto cond_s = str.to_string_impl(cond, false);
                                map_line(c->loc);
                                w.writeln("if (", cond_s, "==", cond_u, ") {");
                                {
                                    auto indent = w.indent_scope();
                                    auto f = c->field.lock();
                                    if (!f) {
                                        w.writeln("return false;");
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
                                    w.writeln("return false;");
                                }
                                else {
                                    make_access(f);
                                }
                                end_else = true;
                            }
                        }
                        if (!end_else) {
                            w.writeln("return false;");
                        }
                    }
                    w.writeln("}");
                };
                write_setter(false);
                write_setter(true);
            };
            accessor_funcs.push_back(std::move(fn));
        }

        void write_each_type_accessor(const std::string& cond_u, const std::shared_ptr<ast::Field>& uf, ast::UnionType* ut) {
            for (auto& cand : ut->candidates) {
            }
        }

        void write_struct_union(std::string_view prefix, ast::StructUnionType* union_ty) {
            map_line(union_ty->loc);
            if (use_variant) {
                auto variant_name = brgen::concat("union_variant_", brgen::nums(get_seq()));
                size_t i = 1;
                std::string variant_types;
                for (auto& f : union_ty->structs) {
                    auto c = brgen::concat("union_struct_", brgen::nums(get_seq()));
                    write_struct_type(c, f, brgen::concat("std::get<", brgen::nums(i), ">(", prefix, ".", variant_name, ")"));
                    auto& t = anonymous_struct[f];
                    t.type_name = c;
                    t.variant_name = variant_name;
                    w.writeln(";");
                    variant_types += ", " + c;
                    i++;
                }
                w.writeln("std::variant<std::monostate", variant_types, "> ", variant_name, ";");
            }
            else {
                w.writeln("union {");
                std::string prefix = "union_struct_";
                {
                    auto indent = w.indent_scope();
                    w.writeln("struct {} ", prefix, "dummy", brgen::nums(get_seq()), ";");
                    for (auto& f : union_ty->structs) {
                        auto c = brgen::concat(prefix, brgen::nums(get_seq()));
                        write_struct_type("", f, brgen::concat(prefix, ".", c));
                        w.writeln(" ", c, ";");
                    }
                }
                w.writeln("};");
            }
            for (auto& f : union_ty->union_fields) {
                auto uf = f.lock();
                auto ut = ast::as<ast::UnionType>(uf->field_type);
                assert(ut);
                auto c = ut->cond.lock();
                std::string cond_u;
                if (c) {
                    auto defs = ast::tool::collect_defined_ident(c);
                    for (auto& d : defs) {
                        code_one_node(d);
                    }
                    cond_u = str.to_string(c);
                }
                else {
                    cond_u = "true";
                }
                if (ut->common_type) {
                    write_common_type_accessor(std::string(prefix), cond_u, uf, ut);
                }
                else {
                    write_each_type_accessor(cond_u, uf, ut);
                }
            }
        }

        bool set_hidden_ident(const std::shared_ptr<ast::Field>& f) {
            if (!f->ident) {
                ast::tool::set_tmp_field_ident(get_seq(), f, "hidden_field_");
                return true;
            }
            return false;
        }

        std::shared_ptr<ast::Type> unwrap_ident_type(const std::shared_ptr<ast::Type>& type) {
            if (auto ident = ast::as<ast::IdentType>(type); ident) {
                return ident->base.lock();
            }
            return type;
        }

        void write_int_type_field(std::string_view prefix, brgen::lexer::Loc loc, std::string_view ident, size_t bit_size, bool is_common_supported, bool is_signed) {
            if (is_common_supported) {
                map_line(loc);
                w.writeln("std::", is_signed ? "" : "u", "int", brgen::nums(bit_size), "_t ", ident, " = 0", ";");
            }
            else if (bit_size == 24) {
                map_line(loc);
                w.writeln("std::uint32_t ", ident, " = 0; // 24 bit int");
            }
        }

        void write_field(
            std::vector<std::shared_ptr<ast::Field>>& non_aligned,
            const std::shared_ptr<ast::Field>& f, std::string_view prefix,
            bool on_state = false) {
            futils::helper::DynDefer d;
            auto is_simple_type = [&](const std::shared_ptr<ast::Type>& type) {
                return ast::as<ast::IntType>(type) != nullptr || ast::as<ast::EnumType>(type) != nullptr;
            };
            bool hidden = set_hidden_ident(f);
            escape_keyword(f->ident->ident);
            auto type = unwrap_ident_type(f->field_type);
            // only for state
            if (on_state && ast::as<ast::BoolType>(type)) {
                map_line(f->loc);
                w.writeln("bool ", f->ident->ident, " = false;");
                str.map_ident(f->ident, prefix, ".", f->ident->ident);
            }
            if (f->bit_alignment == ast::BitAlignment::not_target) {
                return;
            }
            if (f->bit_alignment != f->eventual_bit_alignment) {
                non_aligned.push_back(ast::cast_to<ast::Field>(f));
                return;
            }
            if (non_aligned.size() > 0) {
                non_aligned.push_back(ast::cast_to<ast::Field>(f));
                write_bit_fields(prefix, non_aligned, ast::Endian::unspec);
                non_aligned.clear();
                return;
            }
            if (auto union_ty = ast::as<ast::StructUnionType>(type)) {
                write_struct_union(prefix, union_ty);
                return;
            }
            if (hidden) {
                w.writeln("private:");
                d = futils::helper::defer_ex([&] {
                    w.writeln("public:");
                });
            }
            if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                write_int_type_field(prefix, f->loc, f->ident->ident, *int_ty->bit_size, int_ty->is_common_supported, int_ty->is_signed);
                str.map_ident(f->ident, prefix, ".", f->ident->ident);
            }
            if (auto float_ty = ast::as<ast::FloatType>(type); float_ty) {
                if (float_ty->is_common_supported) {
                    map_line(f->loc);
                    w.writeln("::futils::binary::float", brgen::nums(*float_ty->bit_size), "_t ", f->ident->ident, " = 0", ";");
                    str.map_ident(f->ident, prefix, ".", f->ident->ident);
                }
            }
            if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                auto enum_ = enum_ty->base.lock();
                auto bit_size = enum_->base_type->bit_size;
                map_line(f->loc);
                w.writeln(enum_->ident->ident, " ", f->ident->ident, "{};");
                str.map_ident(f->ident, prefix, ".", f->ident->ident);
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                auto ty = get_type_name(type);
                map_line(f->loc);
                w.writeln(ty, " ", f->ident->ident, ";");
                if (auto range = ast::as<ast::Range>(arr_ty->length)) {
                    if (f->eventual_follow == ast::Follow::end) {
                        later_size[f.get()].size = f->tail_offset_recent;
                    }
                    if (f->follow == ast::Follow::constant) {
                        auto& later = later_size[f.get()];
                        later.next_field = f->next.lock();
                        assert(later.next_field);
                    }
                }
                str.map_ident(f->ident, prefix, ".", f->ident->ident);
                if (auto ident = has_ident_len(type); ident && ast::tool::is_on_named_struct(f)) {
                    w.writeln("bool set_", f->ident->ident, "(auto&& v) {");
                    {
                        auto indent = w.indent_scope();
                        maybe_write_auto_length_set("v.size()", type);
                        auto to = str.to_string(f->ident);
                        w.writeln(to, " = std::forward<decltype(v)>(v);");
                        w.writeln("return true;");
                    }
                    w.writeln("}");
                }
            }
            if (auto struct_ty = ast::as<ast::StructType>(type)) {
                auto type_name = get_type_name(type);
                map_line(f->loc);
                w.writeln(type_name, " ", f->ident->ident, ";");
                str.map_ident(f->ident, prefix, ".", f->ident->ident);
            }
            if (auto str_type = ast::as<ast::StrLiteralType>(type)) {
                auto len = str_type->strong_ref->length;
                map_line(f->loc);
                w.writeln("//", str_type->strong_ref->value, " (", brgen::nums(len), " bytes)");
                str.map_ident(f->ident, str_type->strong_ref->value);
            }
        }

        void write_visit_struct(const std::shared_ptr<ast::StructType>& s) {
            if (!add_visit) {
                return;
            }
            w.writeln("template<typename Visitor>");
            w.writeln("void visit(Visitor&& v) {");
            {
                auto indent = w.indent_scope();
                for (auto& f : s->fields) {
                    if (auto field = ast::as<ast::Field>(f); field) {
                        if (ast::as<ast::StructUnionType>(field->field_type)) {
                            continue;
                        }
                        auto ident = str.to_string(field->ident);
                        w.writeln("v(v, \"", field->ident->ident, "\",", ident, ");");
                    }
                }
            }
            w.writeln("}");
        }

        std::string state_variable_to_argument(const std::shared_ptr<ast::StructType>& s, bool as_param) {
            if (auto fmt = ast::as<ast::Format>(s->base.lock())) {
                std::string args;
                for (auto& weak_arg : fmt->state_variables) {
                    auto arg = weak_arg.lock();
                    auto& typ = ast::as<ast::IdentType>(arg->field_type)->ident->ident;
                    auto& ident = arg->ident;
                    escape_keyword(ident->ident);
                    str.map_ident(ident, ident->ident);
                    auto& param_name = arg->ident->ident;
                    if (as_param) {
                        args += "," + typ + "& " + param_name;
                    }
                    else {
                        args += "," + param_name;
                    }
                }
                return args;
            }
            return "";
        }

        void write_struct_type(std::string_view struct_name, const std::shared_ptr<ast::StructType>& s, const std::string& prefix = "") {
            auto member = ast::as<ast::Member>(s->base.lock());
            bool has_ident = member && member->ident;
            assert(!member || member->node_type != ast::NodeType::field);
            if (has_ident) {
                map_line(member->loc);
            }
            w.writeln("struct ", struct_name, "{");
            {
                bool is_type_mapped = false;
                if (s->type_map && s->bit_size && s->type_map->type_literal->bit_size == s->bit_size) {
                    is_type_mapped = true;
                }
                auto indent = w.indent_scope();
                bool is_int_set = true;
                bool include_non_simple = false;
                std::vector<std::shared_ptr<ast::Field>> non_aligned;
                for (auto i = 0; i < s->fields.size(); i++) {
                    auto& field = s->fields[i];
                    if (auto f = ast::as<ast::Field>(field); f) {
                        if (is_type_mapped) {
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            continue;
                        }
                        write_field(non_aligned, ast::cast_to<ast::Field>(field), prefix);
                    }
                }
                if (is_type_mapped) {
                    ast::Endian endian = ast::Endian::unspec;
                    if (auto ity = ast::as<ast::IntType>(s->type_map->type_literal); ity) {
                        endian = ity->endian;
                    }
                    write_bit_fields(prefix, non_aligned, endian);
                }
                if (has_ident) {
                    if (!use_variant) {  // for raw union
                        w.writeln(member->ident->ident, "() {}");
                    }
                    std::string ret_type;
                    if (use_error) {
                        ret_type = "::futils::error::Error<>";
                    }
                    else {
                        ret_type = "bool";
                    }
                    if (use_constexpr) {
                        ret_type = "constexpr " + ret_type;
                    }
                    auto params = state_variable_to_argument(s, true);
                    w.writeln(ret_type, " encode(::futils::binary::writer& w", params, ") const ;");
                    w.writeln(ret_type, " decode(::futils::binary::reader& r", params, ");");
                    if (s->fixed_header_size) {
                        w.writeln("static constexpr size_t fixed_header_size = ", brgen::nums(s->fixed_header_size / 8), ";");
                    }
                    write_visit_struct(s);
                }
            }
            w.write("}");
        }

        void write_simple_struct(const std::shared_ptr<ast::Format>& fmt) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            struct_names.push_back(fmt->ident->ident);
            write_struct_type(fmt->ident->ident, fmt->body->struct_type, "${THIS}");
            w.writeln(";");
        }

        void write_state(const std::shared_ptr<ast::State>& state) {
            map_line(state->loc);
            w.writeln("struct ", state->ident->ident, " {");
            {
                auto indent = w.indent_scope();
                std::vector<std::shared_ptr<ast::Field>> non_align;
                for (auto& f : state->body->struct_type->fields) {
                    if (auto field = ast::as<ast::Field>(f); field) {
                        write_field(non_align, ast::cast_to<ast::Field>(f), "${THIS}", true);
                    }
                }
            }
            w.writeln("};");
        }

        void write_format_fns(const std::shared_ptr<ast::Format>& fmt) {
            write_code_fn(fmt, true);
            write_code_fn(fmt, false);
        }

        void write_enum(const std::shared_ptr<ast::Enum>& enum_) {
            map_line(enum_->loc);
            w.write("enum class ", enum_->ident->ident);
            if (enum_->base_type) {
                auto underlying = get_type_name(enum_->base_type, true);
                w.write(" : ", underlying);
            }
            w.writeln(" {");
            {
                auto indent = w.indent_scope();
                for (auto& c : enum_->members) {
                    map_line(c->loc);
                    w.write(c->ident->ident);
                    if (c->value) {
                        w.write(" = ", str.to_string(c->value));
                    }
                    w.writeln(",");
                    str.map_ident(c->ident, enum_->ident->ident, "::", c->ident->ident);
                }
            }
            w.writeln("};");
            if (enum_stringer) {
                w.writeln("constexpr const char* to_string(", enum_->ident->ident, " e) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("switch(e) {");
                    {
                        auto indent = w.indent_scope();
                        for (auto& c : enum_->members) {
                            map_line(c->loc);
                            auto str = "\"" + c->ident->ident + "\"";
                            if (c->str_literal) {
                                str = c->str_literal->value;
                            }
                            w.writeln("case ", enum_->ident->ident, "::", c->ident->ident, ": return ", str, ";");
                        }
                    }
                    w.writeln("}");
                    w.writeln("return \"\";");
                }
                w.writeln("}");
                w.writeln();
                w.writeln("constexpr std::optional<", enum_->ident->ident, "> ", enum_->ident->ident, "_from_string(std::string_view str) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("if (str.empty()) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return std::nullopt;");
                    }
                    w.writeln("}");
                    for (auto& c : enum_->members) {
                        map_line(c->loc);
                        auto str = "\"" + c->ident->ident + "\"";
                        if (c->str_literal) {
                            str = c->str_literal->value;
                        }
                        w.writeln("if (str == ", str, ") {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return ", enum_->ident->ident, "::", c->ident->ident, ";");
                        }
                        w.writeln("}");
                    }
                    w.writeln("return std::nullopt;");
                }
                w.writeln("}");
            }
        }

        void write_prefixed_bit_field_encode_decode(PrefixedBitField& t) {
            auto tmp = brgen::concat("tmp", brgen::nums(get_seq()));
            auto cond = str.to_string(t.candidates->cond.lock());

            if (ctx.encode) {
                bool should_else = false;
                for (auto& cand : t.candidates->candidates) {
                    auto c = cand->cond.lock();
                    auto cond_num = str.to_string(c);
                    if (should_else) {
                        w.writeln("else ");
                    }
                    else {
                        should_else = true;
                    }
                    w.writeln("if(", cond, "==", cond_num, ") {");
                    {
                        auto indent = w.indent_scope();
                        auto f = cand->field.lock();
                        auto bit_size = *t.prefix->field_type->bit_size + *f->field_type->bit_size;
                        auto typ = brgen::concat("std::uint", brgen::nums(bit_size), "_t");
                        w.writeln(typ, " ", tmp, " = 0;");
                        auto ident = str.to_string(t.union_field->ident);
                        w.writeln(tmp, " = ", ident, ";");
                        w.writeln(tmp, " |= ", typ, "(", cond_num, ")", "<<", brgen::nums(bit_size - *t.prefix->field_type->bit_size), ";");
                        w.writeln("if (!::futils::binary::write_num(w,", tmp, ",", ctx.endian_text(ast::Endian::big), ")) {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(t.union_field.get(), "write bit field failed");
                        }
                        w.writeln("}");
                    }
                    w.writeln("}");
                }
            }
            else {
                w.writeln("if(!r.load_stream(1)) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(t.union_field.get(), "read bit field failed");
                }
                w.writeln("}");
                auto mask = brgen::concat("0x", brgen::nums((1 << *t.prefix->field_type->bit_size) - 1, 16));
                auto shift = brgen::nums(8 - *t.prefix->field_type->bit_size);
                w.writeln("std::uint8_t ", tmp, " = (r.top() >> ", shift, " ) & ", mask, ";");
                auto setter = cond;
                setter.pop_back();  // remove ')'
                w.writeln(setter, tmp, ");");
                bool should_else = false;
                for (auto& cand : t.candidates->candidates) {
                    auto c = cand->cond.lock();
                    auto cond_num = str.to_string(c);
                    if (should_else) {
                        w.writeln("else ");
                    }
                    else {
                        should_else = true;
                    }
                    w.writeln("if(", cond, "==", cond_num, ") {");
                    {
                        auto indent = w.indent_scope();
                        auto f = cand->field.lock();
                        auto bit_size = *t.prefix->field_type->bit_size + *f->field_type->bit_size;
                        auto tmp2 = brgen::concat("tmp", brgen::nums(get_seq()));
                        auto typ = brgen::concat("std::uint", brgen::nums(bit_size), "_t");
                        w.writeln(typ, " ", tmp2, " = 0;");
                        w.writeln("if(!::futils::binary::read_num(r,", tmp2, ",", ctx.endian_text(ast::Endian::big), ")) {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(t.union_field.get(), "read bit field failed");
                        }
                        w.writeln("}");
                        w.writeln(tmp2, " &= ~(", typ, "(", mask, ")", "<<", brgen::nums(bit_size - *t.prefix->field_type->bit_size), ");");
                        auto setter = str.to_string(t.union_field->ident);
                        setter.pop_back();  // remove ')'
                        w.writeln(setter, tmp2, ");");
                    }
                    w.writeln("}");
                }
            }
        }

        void code_if(ast::If* if_) {
            if (auto it = prefixed_bit_field.find(if_->struct_union_type.get()); it != prefixed_bit_field.end()) {
                write_prefixed_bit_field_encode_decode(it->second);
                return;
            }
            auto cond = if_->cond;
            auto cond_s = str.to_string(cond);
            map_line(if_->loc);
            w.writeln("if (", cond_s, ") {");
            {
                auto indent = w.indent_scope();
                code_indent_block(if_->then);
            }
            w.writeln("}");
            auto elif_ = if_;
            while (elif_) {
                if (auto if_ = ast::as<ast::If>(elif_->els); if_) {
                    cond_s = str.to_string(if_->cond);
                    map_line(if_->loc);
                    w.writeln("else if (", cond_s, ") {");
                    {
                        auto indent = w.indent_scope();
                        code_indent_block(if_->then);
                    }
                    w.writeln("}");
                    elif_ = if_;
                    continue;
                }
                if (auto b = ast::as<ast::IndentBlock>(elif_->els)) {
                    map_line(b->loc);
                    w.writeln("else {");
                    {
                        auto indent = w.indent_scope();
                        code_indent_block(ast::cast_to<ast::IndentBlock>(elif_->els));
                    }
                    w.writeln("}");
                    break;
                }
                break;
            }
        }

        void code_match(ast::Match* m) {
            if (auto it = prefixed_bit_field.find(m->struct_union_type.get()); it != prefixed_bit_field.end()) {
                write_prefixed_bit_field_encode_decode(it->second);
                return;
            }
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
                    map_line(br->loc);
                    if (has_case) {
                        w.write("else ");
                    }
                    // any match (`..`) case
                    if (ast::is_any_range(br->cond)) {
                        // nothing to write
                        w.writeln("{");
                    }
                    else {
                        w.writeln("if (", cond, "==", cond_s, ") {");
                    }
                    {
                        auto indent = w.indent_scope();
                        code_one_node(br->then);
                    }
                    w.writeln("}");
                    has_case = true;
                }
            }
        }

        void write_field_encode(ast::Field* f) {
            if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;
            }
            if (auto found = bit_fields.find(f); found != bit_fields.end()) {
                auto& meta = found->second;
                auto& fields = meta.fields;
                auto& field_name = meta.field_name;
                map_line(f->loc);
                w.writeln("if (!::futils::binary::write_num(w,", field_name, ".as_value() ,", ctx.endian_text(meta.endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(f, "write bit field failed");
                }
                w.writeln("}");
                return;
            }
            else if (bit_fields_part.find(f) != bit_fields_part.end()) {
                return;
            }
            auto ident = str.to_string(f->ident);
            auto typ = f->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            futils::helper::DynDefer peek;
            if (f->arguments) {
                if (f->arguments->arguments.size() == 1) {
                    auto arg = f->arguments->arguments[0];
                    auto expr = str.to_string(arg);
                    w.writeln("if (", expr, " != ", ident, ") {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(f, "field value is not equal to ", expr);
                    }
                    w.writeln("}");
                }
                if (f->arguments->peek) {
                    if (f->arguments->peek_value && *f->arguments->peek_value) {
                        map_line(f->loc);
                        auto seq = get_seq();
                        w.writeln("auto peek_pos_", brgen::nums(seq), "_ = w.offset();");
                        peek = futils::helper::defer_ex([&, seq] {
                            map_line(f->loc);
                            w.writeln("w.reset(peek_pos_", brgen::nums(seq), "_);");
                        });
                    }
                }
            }
            write_field_encode_impl(f->loc, ident, typ, f);
        }

        void write_field_encode_impl(brgen::lexer::Loc loc, std::string_view ident, std::shared_ptr<ast::Type> typ, ast::Field* fi) {
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            if (auto int_ty = ast::as<ast::IntType>(typ); int_ty) {
                auto type_name = get_type_name(typ, true);
                if (*typ->bit_size == 24) {  // TODO(on-keyday): make arbitrary bit size int
                    map_line(loc);
                    w.writeln("if (!::futils::binary::write_uint24(w,", ident, ")) {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(fi, "write 24 bit int failed");
                    }
                    w.writeln("}");
                }
                else {
                    map_line(loc);
                    w.writeln("if (!::futils::binary::write_num(w,static_cast<", type_name, ">(", ident, ") ,", ctx.endian_text(int_ty->endian), ")) {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(fi, "write ", type_name, " failed");
                    }
                    w.writeln("}");
                }
            }
            if (auto float_ty = ast::as<ast::FloatType>(typ); float_ty) {
                map_line(loc);
                auto tmp = brgen::concat("::futils::binary::make_float(", ident, ").to_int()");
                w.writeln("if (!::futils::binary::write_num(w,", tmp, ", ", ctx.endian_text(float_ty->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(fi, "write ", get_type_name(typ), " failed");
                }
                w.writeln("}");
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ); arr_ty) {
                if (auto found = later_size.find(fi); found != later_size.end()) {
                    // nothing to check
                }
                else if (!arr_ty->length_value) {
                    // check dynamic length is compatible with its length
                    map_line(loc);
                    auto length = get_length(fi, arr_ty->length);
                    w.writeln("if (", length, "!=", ident, ".size()) {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(fi, "dynamic length is not compatible with its length; ", length, "!=", ident, ".size()");
                    }
                    w.writeln("}");
                }
                map_line(loc);
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->element_type); int_ty && int_ty->bit_size == 8) {
                    w.writeln("if (!w.write(", ident, ")) {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(fi, "write array failed");
                    }
                    w.writeln("}");
                }
                else {
                    auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                    w.writeln("for (auto& ", tmp, " : ", ident, ") {");
                    {
                        auto indent = w.indent_scope();
                        write_field_encode_impl(loc, tmp, arr_ty->element_type, fi);
                    }
                    w.writeln("}");
                }
            }
            if (auto struct_ty = ast::as<ast::StructType>(typ); struct_ty) {
                map_line(loc);
                auto args = state_variable_to_argument(ast::cast_to<ast::StructType>(typ), false);
                auto call = brgen::concat(ident, ".encode(w", args, ")");
                if (use_error) {
                    w.writeln("if (auto err = ", call, ") {");
                }
                else {
                    w.writeln("if (!", call, ") {");
                }
                {
                    auto indent = w.indent_scope();
                    if (use_error) {
                        w.writeln("return err;");
                    }
                    else {
                        w.writeln("return false;");
                    }
                }
                w.writeln("}");
            }
            if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
                auto len = str_ty->strong_ref->length;
                auto value = str_ty->strong_ref->value;
                map_line(loc);
                w.writeln("if (!w.write(::futils::view::rvec(", value, ", ", brgen::nums(len), "))) {");
                {
                    auto indent = w.indent_scope();
                    auto val = brgen::escape(str_ty->strong_ref->value);
                    write_return_error(fi, "write string failed; ", val);
                }
                w.writeln("}");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
                auto base_type = get_type_name(enum_ty->base.lock()->base_type);
                auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                map_line(loc);
                w.writeln("auto ", tmp, " = static_cast<", base_type, ">(", ident, ");");
                write_field_encode_impl(loc, tmp, enum_ty->base.lock()->base_type, fi);
            }
        }

        std::string get_length(ast::Field* field, const std::shared_ptr<ast::Expr>& expr) {
            if (use_overflow_check) {
                std::vector<std::string> buffer;
                str.bin_op_func = [&](ast::tool::Stringer& s, const std::shared_ptr<ast::Binary>& r, bool root) {
                    brgen::writer::Writer w;
                    auto op = r->op;
                    if (op == ast::BinaryOp::add ||
                        op == ast::BinaryOp::sub ||
                        op == ast::BinaryOp::mul) {
                        auto left = s.to_string(r->left);
                        auto right = s.to_string(r->right);
                        auto tmp_buf = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                        auto typ = get_type_name(r->expr_type, true);
                        std::map<ast::BinaryOp, std::string> op_map = {
                            {ast::BinaryOp::add, "add"},
                            {ast::BinaryOp::sub, "sub"},
                            {ast::BinaryOp::mul, "mul"},
                        };
                        w.writeln(typ, " ", tmp_buf, " = 0;");
                        w.writeln("if(__builtin_", op_map[op], "_overflow(", left, ",", right, ",&", tmp_buf, ")) {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(w, field, op_map[op], " overflow");
                        }
                        w.writeln("}");
                        buffer.push_back(std::move(w.out()));
                        return tmp_buf;
                    }
                    return str.bin_op_with_lookup(r, root);
                };
                auto result = str.to_string(expr);
                str.bin_op_func = nullptr;
                for (auto& b : buffer) {
                    w.write_unformatted(b);
                }
                auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                w.writeln("auto ", tmp, " = ", result, ";");
                return tmp;
            }
            auto result = str.to_string(expr);
            auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
            w.writeln("auto ", tmp, " = ", result, ";");
            return tmp;
        }

        void write_field_decode(ast::Field* f) {
            if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;
            }
            if (auto found = bit_fields.find(f); found != bit_fields.end()) {
                auto& meta = found->second;
                auto& fields = meta.fields;
                auto& field_name = meta.field_name;
                map_line(f->loc);
                w.writeln("if (!::futils::binary::read_num(r,", field_name, ".as_value() ,", ctx.endian_text(meta.endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(f, "read bit field failed");
                }
                w.writeln("}");
                return;
            }
            else if (bit_fields_part.find(f) != bit_fields_part.end()) {
                return;
            }
            auto ident = str.to_string(f->ident);
            auto typ = f->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            futils::helper::DynDefer peek;
            if (f->arguments && f->arguments->peek) {
                if (f->arguments->peek_value && *f->arguments->peek_value) {
                    map_line(f->loc);
                    auto seq = get_seq();
                    w.writeln("auto peek_pos_", brgen::nums(seq), "_ = r.offset();");
                    peek = futils::helper::defer_ex([&, seq] {
                        map_line(f->loc);
                        w.writeln("r.reset(peek_pos_", brgen::nums(seq), "_);");
                    });
                }
            }
            write_field_decode_impl(f->loc, ident, typ, f);
            if (f->arguments && f->arguments->arguments.size() == 1) {
                auto arg = f->arguments->arguments[0];
                auto expr = str.to_string(arg);
                w.writeln("if (", ident, " != ", expr, ") {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(f, "field value is not equal to ", expr);
                }
                w.writeln("}");
            }
        }

        void write_field_decode_impl(brgen::lexer::Loc loc, std::string_view ident, std::shared_ptr<ast::Type> typ, ast::Field* fi) {
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            if (auto int_ty = ast::as<ast::IntType>(typ); int_ty) {
                if (*int_ty->bit_size == 24) {  // TODO(on-keyday): make arbitrary bit size int
                    map_line(loc);
                    w.writeln("if (!::futils::binary::read_uint24(r,", ident, ")) {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(fi, "read 24 bit int failed");
                    }
                    w.writeln("}");
                }
                else {
                    map_line(loc);
                    w.writeln("if (!::futils::binary::read_num(r,", ident, " ,", ctx.endian_text(int_ty->endian), ")) {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(fi, "read int failed");
                    }
                    w.writeln("}");
                }
            }
            if (auto float_ty = ast::as<ast::FloatType>(typ); float_ty) {
                auto int_ty = brgen::concat("std::uint", brgen::nums(*float_ty->bit_size), "_t");
                map_line(loc);
                auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                w.writeln(int_ty, " ", tmp, " = 0;");
                w.writeln("if (!::futils::binary::read_num(r,", tmp, " ,", ctx.endian_text(float_ty->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(fi, "read ", get_type_name(typ), " failed");
                }
                w.writeln("}");
                map_line(loc);
                w.writeln(ident, " = ::futils::binary::make_float(", tmp, ").to_float();");
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(typ); arr_ty) {
                std::optional<std::string> len;
                std::optional<std::string> require_remain_bytes;
                std::optional<std::tuple<std::string, brgen::lexer::Loc, size_t>> next;
                if (auto found = later_size.find(fi); found != later_size.end()) {
                    if (found->second.next_field) {
                        auto next_expect = str.to_string(found->second.next_field->ident);
                        auto next_loc = found->second.next_field->loc;
                        auto size = *found->second.next_field->field_type->bit_size / 8;
                        next = {next_expect, next_loc, size};
                    }
                    else if (found->second.size != 0) {
                        auto require_remain = brgen::nums(found->second.size / 8);
                        map_line(loc);
                        w.writeln("if(r.is_stream()) {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(fi, "read array failed; stream mode is currently not supported for fixed terminator");
                        }
                        w.writeln("}");
                        map_line(loc);
                        w.writeln("if (r.remain().size() < ", require_remain, ") {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(fi, "remain size is not enough; ", "require ", require_remain);
                        }
                        w.writeln("}");
                        len = brgen::concat("(r.remain().size() - ", require_remain, ")");
                        require_remain_bytes = require_remain;
                    }
                    else {
                        len = "$EOF";
                    }
                }
                else if (!arr_ty->length_value) {
                    len = get_length(fi, arr_ty->length);
                }
                map_line(loc);
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->element_type); int_ty && int_ty->bit_size == 8) {
                    if (len) {
                        if (len == "$EOF") {
                            w.writeln("if (!r.read_until_eof(", ident, ")) {");
                        }
                        else {
                            w.writeln("if (!r.read(", ident, ", ", *len, ")) {");
                        }
                        {
                            auto indent = w.indent_scope();
                            write_return_error(fi, "read byte array failed");
                        }
                        w.writeln("}");
                    }
                    else if (next) {
                        // TODO(on-keyday): with boyer moore if long length string?
                        auto base_offset_tmp = brgen::concat("base_offset_", brgen::nums(get_seq()), "_");
                        w.writeln("auto ", base_offset_tmp, " = r.offset();");
                        w.writeln("for(;;) {");
                        {
                            auto indent = w.indent_scope();
                            auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                            auto term_len = brgen::nums(std::get<2>(*next));
                            w.writeln("auto ", tmp, " = r.remain().substr(0,", term_len, ");");
                            w.writeln("if (", tmp, ".size() < ", term_len, ") {");
                            {
                                auto indent = w.indent_scope();
                                write_return_error(fi, "read byte array failed; no terminator found");
                            }
                            w.writeln("}");
                            w.writeln("if (", tmp, " == ::futils::view::rvec(", std::get<0>(*next), ", ", term_len, ")) {");
                            {
                                auto indent = w.indent_scope();
                                w.writeln("r.reset(", base_offset_tmp, ");");
                                w.writeln("if(!r.read(", ident, ", ", tmp, ".size())){");
                                {
                                    auto indent = w.indent_scope();
                                    write_return_error(fi, "read byte array failed; fatal error");
                                }
                                w.writeln("}");
                                w.writeln("break;");
                            }
                            w.writeln("}");
                            w.writeln("r.offset(1);");
                        }
                        w.writeln("}");
                    }
                    else {
                        w.writeln("if (!r.read(", ident, ")) {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(fi, "read byte array failed");
                        }
                        w.writeln("}");
                    }
                }
                else {
                    if (!len && !next) {
                        len = brgen::nums(*arr_ty->length_value);
                    }
                    auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                    auto type = get_type_name(arr_ty->element_type);
                    map_line(loc);
                    if (!arr_ty->non_dynamic_allocation) {
                        w.writeln(ident, ".clear();");
                    }
                    std::string tmp_i;
                    if (len && require_remain_bytes) {
                        w.writeln("while (r.remain().size() > ", *require_remain_bytes, ") {");
                    }
                    else if (len && *len != "$EOF") {
                        tmp_i = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                        w.writeln("for (size_t  ", tmp_i, "= 0; ", tmp_i, "<", *len, "; ++", tmp_i, " ) {");
                    }
                    else {
                        w.writeln("for (;;) {");
                    }
                    // avoid reserve() call to prevent memory exhausted
                    {
                        auto indent = w.indent_scope();
                        if (len == "$EOF") {
                            w.writeln("if(!r.load_stream(1)) {");
                            w.indent_writeln("break; // reached EOF");
                            w.writeln("}");
                        }
                        else if (next) {
                            auto next_len = brgen::nums(std::get<2>(*next));
                            auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                            w.writeln("if(!r.load_stream(", next_len, ")) {");
                            {
                                auto indent = w.indent_scope();
                                auto val = brgen::escape(std::get<0>(*next));
                                write_return_error(fi, "read array failed; no terminator ", val, " found");
                            }
                            w.writeln("}");
                            w.writeln("auto ", tmp, " = r.remain().substr(0,", next_len, ");");
                            w.writeln("if (", tmp, " == ::futils::view::rvec(", std::get<0>(*next), ", ", next_len, ")) {");
                            w.indent_writeln("break;");
                            w.writeln("}");
                        }
                        if (arr_ty->non_dynamic_allocation) {
                            write_field_decode_impl(loc, brgen::concat(ident, "[", tmp_i, "]"), arr_ty->element_type, fi);
                        }
                        else {
                            w.writeln(type, " ", tmp, ";");
                            write_field_decode_impl(loc, tmp, arr_ty->element_type, fi);
                            w.writeln(ident, ".push_back(std::move(", tmp, "));");
                        }
                    }
                    w.writeln("}");
                }
            }
            if (auto struct_ty = ast::as<ast::StructType>(typ); struct_ty) {
                map_line(loc);
                auto args = state_variable_to_argument(ast::cast_to<ast::StructType>(typ), false);
                auto call = brgen::concat(ident, ".decode(r", args, ")");
                if (use_error) {
                    w.writeln("if (auto err = ", call, ") {");
                }
                else {
                    w.writeln("if (!", call, ") {");
                }
                {
                    auto indent = w.indent_scope();
                    if (use_error) {
                        w.writeln("return err;");
                    }
                    else {
                        w.writeln("return false;");
                    }
                }
                w.writeln("}");
            }
            if (auto str_ty = ast::as<ast::StrLiteralType>(typ)) {
                auto len = str_ty->strong_ref->length;
                auto value = str_ty->strong_ref->value;
                map_line(loc);
                auto tmp_var = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                w.writeln("::futils::view::rvec ", tmp_var, " = {};");
                map_line(loc);
                w.writeln("if (!r.read_direct(", tmp_var, ", ", brgen::nums(len), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(fi, "read string failed");
                }
                w.writeln("}");
                map_line(loc);
                w.writeln("if (", tmp_var, " != ::futils::view::rvec(", value, ",", brgen::nums(len), ")) {");
                {
                    auto indent = w.indent_scope();
                    auto val = brgen::escape(str_ty->strong_ref->value);
                    write_return_error(fi, "read string failed; not match to ", val);
                }
                w.writeln("}");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
                auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                auto int_ty = get_type_name(enum_ty->base.lock()->base_type, true);
                map_line(loc);
                w.writeln(int_ty, " ", tmp, " = 0;");
                write_field_decode_impl(loc, tmp, enum_ty->base.lock()->base_type, fi);
                map_line(loc);
                w.writeln(ident, " = static_cast<", enum_ty->base.lock()->ident->ident, ">(", tmp, ");");
            }
        }

        void write_field_code(ast::Field* f) {
            if (ctx.encode) {
                write_field_encode(f);
            }
            else {
                write_field_decode(f);
            }
        }

        void code_one_node(const std::shared_ptr<ast::Node>& elem) {
            if (auto f = ast::as<ast::Field>(elem)) {
                write_field_code(f);
            }
            if (auto if_ = ast::as<ast::If>(elem)) {
                code_if(if_);
            }
            if (auto return_ = ast::as<ast::Return>(elem)) {
                write_return_ok();
            }
            if (auto block = ast::as<ast::IndentBlock>(elem)) {
                code_indent_block(ast::cast_to<ast::IndentBlock>(elem));
            }
            if (auto match = ast::as<ast::Match>(elem)) {
                code_match(match);
            }
            if (auto scoped = ast::as<ast::ScopedStatement>(elem)) {
                variant_alternative(scoped->struct_type, true);
                code_one_node(scoped->statement);
            }
            if (auto b = ast::as<ast::Binary>(elem);
                b &&
                (b->op == ast::BinaryOp::define_assign ||
                 b->op == ast::BinaryOp::const_assign)) {
                auto ident = ast::as<ast::Ident>(b->left);
                assert(ident);
                auto expr = str.to_string(b->right);
                map_line(b->loc);
                w.writeln("auto ", ident->ident, " = ", expr, ";");
                str.map_ident(ast::cast_to<ast::Ident>(b->left), ident->ident);
            }
            if (auto s = ast::as<ast::SpecifyOrder>(elem); s && s->order_type == ast::OrderType::byte) {
                if (ctx.dynamic_endian) {
                    if (s->order_value) {
                        map_line(s->loc);
                        w.writeln("dynamic_endian____ = !/*reverse for library*/(/*true->little false->big*/", brgen::nums(*s->order_value), ");");
                    }
                    else {
                        auto specify = str.to_string(s->order);
                        map_line(s->loc);
                        w.writeln("dynamic_endian____ = !/*reverse for library*/(/*true->little false->big*/", specify, ");");
                    }
                }
                else {
                    ctx.endian = *s->order_value ? ast::Endian::little : ast::Endian::big;
                }
            }
            if (auto a = ast::as<ast::Assert>(elem)) {
                auto cond = str.to_string(a->cond);
                w.writeln("if (!(", cond, ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(current_format, "assertion failed; ", cond);
                }
                w.writeln("}");
            }
            if (auto io_op = ast::as<ast::IOOperation>(elem)) {
                if (io_op->method == ast::IOMethod::input_backward) {
                    std::string len = "1";
                    if (io_op->arguments.size()) {
                        len = str.to_string(io_op->arguments[0]);
                    }
                    map_line(io_op->loc);
                    auto t = ctx.encode ? "w" : "r";
                    w.writeln("if(", t, ".offset() < ", len, ") {");
                    {
                        auto indent = w.indent_scope();
                        write_return_error(current_format, "input backward failed; offset is less than ", len);
                    }
                    w.writeln("}");
                    map_line(io_op->loc);
                    w.writeln(t, ".reset(", t, ".offset() - ", len, ");");
                }
            }
        }

        void code_indent_block(const std::shared_ptr<ast::IndentBlock>& block) {
            variant_alternative(block->struct_type, true);
            // encode fields
            for (auto& elem : block->elements) {
                code_one_node(elem);
            }
        }

        void write_code_fn(const std::shared_ptr<ast::Format>& fmt, bool encode) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            current_format = fmt.get();
            ctx.encode = encode;
            ctx.endian = ast::Endian::unspec;
            ctx.dynamic_endian = false;
            for (auto& elem : fmt->body->elements) {
                if (auto f = ast::as<ast::SpecifyOrder>(elem); f && f->order_type == ast::OrderType::byte) {
                    if (!f->order_value) {
                        ctx.dynamic_endian = true;
                        break;
                    }
                }
            }
            map_line(fmt->loc);
            auto return_type = "bool";
            if (use_error) {
                return_type = "::futils::error::Error<>";
            }
            auto args = state_variable_to_argument(fmt->body->struct_type, true);
            if (use_constexpr) {
                w.write("constexpr ");
            }
            if (encode) {
                w.writeln("inline ", return_type, " ", fmt->ident->ident, "::encode(::futils::binary::writer& w", args, ") const {");
            }
            else {
                w.writeln("inline ", return_type, " ", fmt->ident->ident, "::decode(::futils::binary::reader& r", args, ") {");
            }
            {
                auto indent = w.indent_scope();
                if (ctx.dynamic_endian) {
                    map_line(fmt->loc);
                    w.writeln("bool dynamic_endian____ = true /*big endian*/;");
                }
                // write code
                code_indent_block(fmt->body);
                write_return_ok();
            }
            w.writeln("}");
        }

        void write_program(std::shared_ptr<ast::Program>& prog) {
            str.type_resolver = [&](auto& s, const std::shared_ptr<ast::Type>& t) {
                return get_type_name(t);
            };
            str.io_op_handler = [&](auto& s, const std::shared_ptr<ast::IOOperation>& t) {
                if (t->method == ast::IOMethod::input_offset) {
                    if (ctx.encode) {
                        return "w.offset()";
                    }
                    else {
                        return "r.offset()";
                    }
                }
                return "";
            };
            w.writeln("//Code generated by json2cpp2");
            w.writeln("#pragma once");
            w.writeln("#include <cstdint>");
            w.writeln("#include <vector>");
            w.writeln("#include <array>");
            w.writeln("#include <optional>");
            if (use_variant) {
                w.writeln("#include <variant>");
            }
            if (enum_stringer) {
                w.writeln("#include <string_view>");
            }
            w.writeln();
            w.writeln("#include <binary/flags.h>");
            w.writeln("#include <binary/float.h>");
            w.writeln("#include <view/iovec.h>");
            w.writeln("#include <binary/number.h>");
            if (use_error) {
                w.writeln("#include <error/error.h>");
            }
            futils::helper::DynDefer defer;
            std::vector<std::string> namespaces;
            for (auto& wm : prog->metadata) {
                auto m = wm.lock();
                auto str_lit = ast::as<ast::StrLiteral>(m->values[0]);
                if (!str_lit) {
                    continue;
                }
                auto name = brgen::unescape(str_lit->value);
                if (!name) {
                    continue;
                }
                if (m->name == "config.cpp.namespace") {
                    namespaces.push_back(*name);
                }
                else if (m->name == "config.cpp.bytes_type") {
                    this->bytes_type = std::move(*name);
                }
                else if (m->name == "config.cpp.vector_type") {
                    this->vector_type = std::move(*name);
                }
                else if (m->name == "config.cpp.sys_include") {
                    w.writeln("#include <", *name, ">");
                }
                else if (m->name == "config.cpp.include") {
                    w.writeln("#include \"", *name, "\"");
                }
            }
            if (auto specify_order = prog->endian.lock()) {
                if (specify_order->order_value) {
                    ctx.global_endian = *specify_order->order_value ? ast::Endian::little : ast::Endian::big;
                }
            }
            for (auto& name : namespaces) {
                w.writeln("namespace ", name, " {");
                auto ind = w.indent_scope_ex();
                defer = futils::helper::defer_ex([&, ind = std::move(ind), d = std::move(defer), name = std::move(name)]() mutable {
                    ind.execute();
                    w.writeln("} // namespace ", name);
                });
            }
            for (auto& fmt : prog->elements) {
                if (auto b = ast::as<ast::Binary>(fmt); b && b->op == ast::BinaryOp::const_assign && b->right->constant_level == ast::ConstantLevel::constant) {
                    auto ident = ast::as<ast::Ident>(b->left);
                    assert(ident);
                    map_line(b->loc);
                    w.writeln("constexpr auto ", ident->ident, " = ", str.to_string(b->right), ";");
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
            // forward declaration
            for (auto& fmt : sorted) {
                if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                    continue;
                }
                w.writeln("struct ", fmt->ident->ident, ";");
            }
            for (auto& fmt : sorted) {
                write_simple_struct(fmt);
            }
            for (auto& acc : accessor_funcs) {
                acc();
            }
            for (auto& fmt : sorted) {
                write_format_fns(ast::cast_to<ast::Format>(fmt));
            }
        }
    };
}  // namespace j2cp2
