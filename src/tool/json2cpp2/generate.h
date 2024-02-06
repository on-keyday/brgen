/*license*/
#pragma once

#include <helper/expected.h>
#include <core/ast/ast.h>
#include <writer/writer.h>
#include <core/ast/tool/stringer.h>
#include <core/ast/tool/sort.h>

namespace j2cp2 {
    namespace ast = brgen::ast;
    struct BitFieldMeta {
        std::string field_name;
        std::vector<std::shared_ptr<ast::Field>> fields;
    };

    struct AnonymousStruct {
        std::string type_name;
        std::string variant_name;
    };

    struct LineMap {
        brgen::lexer::Loc loc;
        size_t line;
    };

    void as_json(LineMap& mp, auto&& obj) {
        auto field = obj.object();
        field("loc", mp.loc);
        field("line", mp.line);
    }

    struct Context {
        bool encode = false;
        ast::Endian endian = ast::Endian::unspec;
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
            return "true";  // default to big
        }
    };

    struct LaterInfo {
        size_t size = 0;
        std::shared_ptr<ast::Field> next_field;
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
        std::map<ast::Field*, LaterInfo> later_size;
        std::map<std::shared_ptr<ast::StructType>, AnonymousStruct> anonymous_struct;
        std::map<ast::EnumMember*, std::string> enum_member_map;
        std::vector<LineMap> line_map;
        bool enable_line_map = false;
        bool use_error = false;
        bool use_variant = false;
        bool use_overflow_check = false;
        ast::Format* current_format = nullptr;
        Context ctx;
        std::vector<std::string> struct_names;

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

        void map_line(brgen::lexer::Loc l) {
            if (!enable_line_map) {
                return;
            }
            line_map.push_back({l, w.line_count()});
        }

        void clean_ident(std::string& s) {
            if (s == "class") {
                s = "class_";
            }
        }

        void write_bit_fields(std::string_view prefix, std::vector<std::shared_ptr<ast::Field>>& non_align, size_t bit_size, bool is_int_set, bool include_non_simple) {
            if (is_int_set && !include_non_simple) {
                w.write("::futils::binary::flags_t<std::uint", brgen::nums(bit_size), "_t");
                for (auto& n : non_align) {
                    auto type = n->field_type;
                    if (auto ident = ast::as<ast::IdentType>(type); ident) {
                        type = ident->base.lock();
                    }
                    if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                        w.write(", ", brgen::nums(*int_ty->bit_size));
                    }
                    else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                        auto bit_size = enum_ty->base.lock()->base_type->bit_size;
                        w.write(", ", brgen::nums(*bit_size));
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
                        str.map_ident(n->ident, "${THIS}", n->ident->ident + "()");
                    }
                    else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                        auto enum_ = enum_ty->base.lock();
                        line_map.push_back({n->loc, w.line_count()});
                        w.writeln("bits_flag_alias_method_with_enum(flags_", brgen::nums(seq), "_,", brgen::nums(i), ",", n->ident->ident, ",", enum_->ident->ident, ");");
                        str.map_ident(n->ident, "${THIS}", prefix, n->ident->ident + "()");
                    }
                    i++;
                }
                bit_fields[non_align.back().get()] = {brgen::concat("flags_", brgen::nums(seq), "_"), non_align};
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
                if (arr_ty->non_dynamic) {
                    auto len = str.to_string(arr_ty->length);
                    auto typ = get_type_name(arr_ty->base_type);
                    return brgen::concat("std::array<", typ, ",", len, ">");
                }
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->base_type); int_ty && int_ty->bit_size == 8) {
                    return "::futils::view::rvec";
                }
                return "std::vector<" + get_type_name(arr_ty->base_type) + ">";
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
                    auto& typ = found->second;
                    w.writeln("if(!std::holds_alternative<", typ.type_name, ">(", typ.variant_name, ")) {");
                    if (as_err) {
                        write_return_error(current_format, typ.variant_name, " variant alternative ", typ.type_name, " is not set");
                    }
                    else {
                        w.indent_writeln("return std::nullopt;");
                    }
                    w.writeln("}");
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

        void write_struct_union(ast::StructUnionType* union_ty) {
            map_line(union_ty->loc);
            if (use_variant) {
                auto variant_name = brgen::concat("union_variant_", brgen::nums(get_seq()));
                size_t i = 1;
                std::string variant_types;
                for (auto& f : union_ty->structs) {
                    auto c = brgen::concat("union_struct_", brgen::nums(get_seq()));
                    write_struct_type(c, f, brgen::concat("std::get<", brgen::nums(i), ">(${THIS}", variant_name, ")."));
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
                        write_struct_type("", f, brgen::concat("${THIS}", c, "."));
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
                    auto defs = str.collect_defined_ident(c);
                    for (auto& d : defs) {
                        code_one_node(d);
                    }
                    cond_u = str.to_string(c);
                }
                else {
                    cond_u = "true";
                }
                if (ut->common_type) {
                    // write getter func
                    map_line(uf->loc);
                    w.writeln("std::optional<", get_type_name(ut->common_type), "> ", uf->ident->ident, "() const {");
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
                            if (cond) {
                                auto defs = str.collect_defined_ident(cond);
                                for (auto& d : defs) {
                                    code_one_node(d);
                                }
                                auto cond_s = str.to_string(cond);
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
                        str.map_ident(uf->ident, "(*${THIS}" + uf->ident->ident + "())");
                    }
                    w.writeln("}");

                    // write setter func
                    map_line(uf->loc);
                    w.writeln("bool ", uf->ident->ident, "(const ", get_type_name(ut->common_type), "& v) {");
                    {
                        auto indent = w.indent_scope();
                        auto make_access = [&](const std::shared_ptr<ast::Field>& f) {
                            map_line(f->loc);
                            set_variant_alternative(f->belong_struct.lock());
                            auto a = str.to_string(f->ident);
                            w.writeln(a, " = v;");
                            w.writeln("return true;");
                        };
                        bool has_els = false;
                        bool end_else = false;
                        for (auto& c : ut->candidates) {
                            auto cond = c->cond.lock();
                            if (cond) {
                                auto defs = str.collect_defined_ident(cond);
                                for (auto& d : defs) {
                                    code_one_node(d);
                                }
                                auto cond_s = str.to_string(cond);
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
                }
            }
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
                auto indent = w.indent_scope();
                bool is_int_set = true;
                bool include_non_simple = false;
                size_t bit_size = 0;
                std::vector<std::shared_ptr<ast::Field>> non_aligned;
                auto is_simple_type = [&](const std::shared_ptr<ast::Type>& type) {
                    return ast::as<ast::IntType>(type) != nullptr || ast::as<ast::EnumType>(type) != nullptr;
                };
                for (auto i = 0; i < s->fields.size(); i++) {
                    auto& field = s->fields[i];
                    if (auto f = ast::as<ast::Field>(field); f) {
                        auto type = f->field_type;
                        futils::helper::DynDefer d;
                        bool hidden = false;
                        if (!f->ident) {
                            auto h = brgen::concat("hidden_field_", brgen::nums(get_seq()));
                            f->ident = std::make_shared<ast::Ident>();
                            f->ident->ident = std::move(h);
                            f->ident->base = field;
                            hidden = true;
                        }
                        clean_ident(f->ident->ident);
                        if (auto ident = ast::as<ast::IdentType>(type); ident) {
                            type = ident->base.lock();
                        }
                        if (f->bit_alignment == ast::BitAlignment::not_target) {
                            continue;
                        }
                        if (f->bit_alignment != ast::BitAlignment::byte_aligned) {
                            is_int_set = is_int_set && type->non_dynamic;
                            include_non_simple = include_non_simple || !is_simple_type(type);
                            bit_size += *type->bit_size;
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            continue;
                        }
                        if (non_aligned.size() > 0) {
                            is_int_set = is_int_set && type->non_dynamic;
                            include_non_simple = include_non_simple || !is_simple_type(type);
                            bit_size += *type->bit_size;
                            non_aligned.push_back(ast::cast_to<ast::Field>(field));
                            write_bit_fields(prefix, non_aligned, bit_size, is_int_set, include_non_simple);
                            non_aligned.clear();
                            is_int_set = true;
                            include_non_simple = false;
                            bit_size = 0;
                            continue;
                        }
                        if (auto union_ty = ast::as<ast::StructUnionType>(type)) {
                            write_struct_union(union_ty);
                            continue;
                        }
                        if (hidden) {
                            w.writeln("private:");
                            d = futils::helper::defer_ex([&] {
                                w.writeln("public:");
                            });
                        }
                        if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                            if (int_ty->is_common_supported) {
                                map_line(f->loc);
                                w.writeln("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(*int_ty->bit_size), "_t ", f->ident->ident, " = 0", ";");
                                str.map_ident(f->ident, prefix, f->ident->ident);
                            }
                            else if (*int_ty->bit_size == 24) {
                                map_line(f->loc);
                                w.writeln("std::uint32_t ", f->ident->ident, " = 0; // 24 bit int");
                                str.map_ident(f->ident, prefix, f->ident->ident);
                            }
                        }
                        if (auto float_ty = ast::as<ast::FloatType>(type); float_ty) {
                            if (float_ty->is_common_supported) {
                                map_line(f->loc);
                                w.writeln("::futils::binary::float", brgen::nums(*float_ty->bit_size), "_t ", f->ident->ident, " = 0", ";");
                                str.map_ident(f->ident, prefix, f->ident->ident);
                            }
                        }
                        if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                            auto enum_ = enum_ty->base.lock();
                            auto bit_size = enum_->base_type->bit_size;
                            map_line(f->loc);
                            w.writeln("std::uint", brgen::nums(*bit_size), "_t ", f->ident->ident, "_data = 0;");
                            map_line(f->loc);
                            w.writeln(enum_->ident->ident, " ", f->ident->ident, "() const { return static_cast<", enum_->ident->ident, ">(this->", f->ident->ident, "_data); }");
                            w.writeln("void ", f->ident->ident, "(", enum_->ident->ident, " v) { this->", f->ident->ident, "_data = static_cast<std::uint", brgen::nums(*bit_size), "_t>(v); }");
                            str.map_ident(f->ident, prefix, f->ident->ident + "()");
                        }
                        if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                            auto ty = get_type_name(type);
                            map_line(f->loc);
                            w.writeln(ty, " ", f->ident->ident, ";");
                            if (auto range = ast::as<ast::Range>(arr_ty->length)) {
                                if (f->eventual_follow == ast::Follow::end) {
                                    later_size[f].size = f->tail_offset_recent;
                                }
                                if (f->follow == ast::Follow::constant) {
                                    auto& later = later_size[f];
                                    for (auto j = i + 1; j < s->fields.size(); j++) {
                                        auto& f2 = s->fields[j];
                                        if (auto f2_ = ast::as<ast::Field>(f2); f2_) {
                                            later.next_field = ast::cast_to<ast::Field>(f2);
                                            break;
                                        }
                                    }
                                    assert(later.next_field);
                                }
                            }
                            str.map_ident(f->ident, prefix, f->ident->ident);
                        }
                        if (auto struct_ty = ast::as<ast::StructType>(type)) {
                            auto type_name = get_type_name(type);
                            map_line(f->loc);
                            w.writeln(type_name, " ", f->ident->ident, ";");
                            str.map_ident(f->ident, prefix, f->ident->ident);
                        }
                        if (auto str_type = ast::as<ast::StrLiteralType>(type)) {
                            auto len = str_type->strong_ref->length;
                            map_line(f->loc);
                            w.writeln("//", str_type->strong_ref->value, " (", brgen::nums(len), " bytes)");
                            str.map_ident(f->ident, str_type->strong_ref->value);
                        }
                    }
                }
                if (has_ident) {
                    w.writeln(member->ident->ident, "() {}");
                    if (use_error) {
                        w.writeln("::futils::error::Error<> encode(::futils::binary::writer& w) const ;");
                        w.writeln("::futils::error::Error<> decode(::futils::binary::reader& r);");
                    }
                    else {
                        w.writeln("bool encode(::futils::binary::writer& w) const ;");
                        w.writeln("bool decode(::futils::binary::reader& r);");
                    }
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

        void write_enum(const std::shared_ptr<ast::Enum>& enum_) {
            map_line(enum_->loc);
            w.write("enum class ", enum_->ident->ident);
            w.writeln(" {");
            {
                auto indent = w.indent_scope();
                for (auto& c : enum_->members) {
                    map_line(c->loc);
                    w.write(c->ident->ident);
                    if (c->expr) {
                        ast::tool::Stringer s;
                        w.write(" = ", s.to_string(c->expr));
                    }
                    w.writeln(",");
                    str.map_ident(c->ident, enum_->ident->ident, "::", c->ident->ident);
                }
            }
            w.writeln("};");
        }

        void code_if(ast::If* if_) {
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
                    w.writeln("if (", cond, "==", cond_s, ") {");
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
                w.writeln("if (!::futils::binary::write_num(w,", field_name, ".as_value() ,true)) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(f, "write bit field failed");
                }
                w.writeln("}");
                return;
            }
            auto ident = str.to_string(f->ident);
            auto typ = f->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            if (ast::as<ast::EnumType>(typ)) {
                ident = ident.substr(0, ident.size() - 2) + "_data";
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
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->base_type); int_ty && int_ty->bit_size == 8) {
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
                        write_field_encode_impl(loc, tmp, arr_ty->base_type, fi);
                    }
                    w.writeln("}");
                }
            }
            if (auto struct_ty = ast::as<ast::StructType>(typ); struct_ty) {
                map_line(loc);
                if (use_error) {
                    w.writeln("if (auto err = ", ident, ".encode(w)) {");
                }
                else {
                    w.writeln("if (!", ident, ".encode(w)) {");
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
                auto l = ast::as<ast::IntType>(enum_ty->base.lock()->base_type);
                map_line(loc);
                w.writeln("if (!::futils::binary::write_num(w,", ident, ",", ctx.endian_text(l->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(fi, "write enum failed");
                }
                w.writeln("}");
            }
        }

        std::string get_length(ast::Field* field, const std::shared_ptr<ast::Expr>& expr) {
            if (use_overflow_check) {
                std::vector<std::string> buffer;
                str.bin_op_func = [&](ast::tool::Stringer& s, const std::shared_ptr<ast::Binary>& r) {
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
                    return str.bin_op_with_lookup(r);
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
                w.writeln("if (!::futils::binary::read_num(r,", field_name, ".as_value() ,true)) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(f, "read bit field failed");
                }
                w.writeln("}");
                return;
            }
            auto ident = str.to_string(f->ident);
            auto typ = f->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            if (ast::as<ast::EnumType>(typ)) {
                ident = ident.substr(0, ident.size() - 2) + "_data";
            }
            write_field_decode_impl(f->loc, ident, typ, f);
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
                        w.writeln("if (r.remain().size() < ", require_remain, ") {");
                        {
                            auto indent = w.indent_scope();
                            write_return_error(fi, "remain size is not enough; ", "require ", require_remain);
                        }
                        w.writeln("}");
                        len = brgen::concat("(r.remain().size() - ", require_remain, ")");
                    }
                    else {
                        len = "r.remain().size()";
                    }
                }
                else if (!arr_ty->length_value) {
                    len = get_length(fi, arr_ty->length);
                }
                map_line(loc);
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->base_type); int_ty && int_ty->bit_size == 8) {
                    if (len) {
                        w.writeln("if (!r.read(", ident, ", ", *len, ")) {");
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
                    auto type = get_type_name(arr_ty->base_type);
                    map_line(loc);
                    if (!arr_ty->non_dynamic) {
                        w.writeln(ident, ".clear();");
                    }
                    std::string tmp_i;
                    if (len) {
                        tmp_i = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                        w.writeln("for (size_t  ", tmp_i, "= 0; ", tmp_i, "<", *len, "; ++", tmp_i, " ) {");
                    }
                    else {
                        w.writeln("for (;;) {");
                    }
                    // avoid reserve() call to prevent memory exhausted
                    {
                        auto indent = w.indent_scope();
                        if (next) {
                            auto next_len = brgen::nums(std::get<2>(*next));
                            auto tmp = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
                            w.writeln("auto ", tmp, " = r.remain().substr(0,", next_len, ");");
                            w.writeln("if (", tmp, ".size() < ", next_len, ") {");
                            {
                                auto indent = w.indent_scope();
                                auto val = brgen::escape(std::get<0>(*next));
                                write_return_error(fi, "read array failed; no terminator ", val, " found");
                            }
                            w.writeln("}");
                            w.writeln("if (", tmp, " == ::futils::view::rvec(", std::get<0>(*next), ", ", next_len, ")) {");
                            w.indent_writeln("break;");
                            w.writeln("}");
                        }
                        if (arr_ty->non_dynamic) {
                            write_field_decode_impl(loc, brgen::concat(ident, "[", tmp_i, "]"), arr_ty->base_type, fi);
                        }
                        else {
                            w.writeln(type, " ", tmp, ";");
                            write_field_decode_impl(loc, tmp, arr_ty->base_type, fi);
                            w.writeln(ident, ".push_back(std::move(", tmp, "));");
                        }
                    }
                    w.writeln("}");
                }
            }
            if (auto struct_ty = ast::as<ast::StructType>(typ); struct_ty) {
                map_line(loc);
                if (use_error) {
                    w.writeln("if (auto err = ", ident, ".decode(r)) {");
                }
                else {
                    w.writeln("if (!", ident, ".decode(r)) {");
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
                w.writeln("if (!r.read(", tmp_var, ", ", brgen::nums(len), ")) {");
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
                auto s = ast::as<ast::IntType>(enum_ty->base.lock()->base_type);
                map_line(loc);
                w.writeln("if (!::futils::binary::read_num(r,", ident, ",", ctx.endian_text(s->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    write_return_error(fi, "read enum failed");
                }
                w.writeln("}");
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
                w.writeln("if (!", cond, ") {");
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
            ctx.endian = ast::Endian::big;  // default to big endian
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
            w.writeln("inline ", return_type, " ", fmt->ident->ident, "::", encode ? "encode(::futils::binary::writer& w) const" : "decode(::futils::binary::reader& r)", " {");
            {
                auto indent = w.indent_scope();
                if (ctx.dynamic_endian) {
                    map_line(fmt->loc);
                    w.writeln("bool dynamic_endian____ = true /*big endian*/;");
                }
                // write code
                code_indent_block(fmt->body);
                if (use_error) {
                    w.writeln("return ::futils::error::Error<>{};");
                }
                else {
                    w.writeln("return true;");
                }
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
            w.writeln();
            w.writeln("#include <binary/flags.h>");
            w.writeln("#include <binary/float.h>");
            w.writeln("#include <view/iovec.h>");
            w.writeln("#include <binary/number.h>");
            if (use_error) {
                w.writeln("#include <error/error.h>");
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
            }
            ast::tool::FormatSorter s;
            auto sorted = s.topological_sort(prog);
            for (auto& fmt : sorted) {
                write_simple_struct(fmt);
                write_code_fn(fmt, true);
                write_code_fn(fmt, false);
            }
        }
    };
}  // namespace j2cp2
