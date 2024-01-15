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

    struct AnonymousStructMeta {
        std::string field_name;
        std::shared_ptr<ast::StructType> struct_;
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
        std::map<ast::Field*, AnonymousStructMeta> anonymous_structs;
        std::map<ast::Field*, LaterInfo> later_size;
        std::map<ast::EnumMember*, std::string> enum_member_map;
        std::vector<LineMap> line_map;
        bool enable_line_map = false;
        Context ctx;
        std::vector<std::string> struct_names;

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

        void write_bit_fields(std::vector<std::shared_ptr<ast::Field>>& non_align, size_t bit_size, bool is_int_set, bool include_non_simple) {
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
                        str.map_ident(n->ident, n->ident->ident + "()");
                    }
                    else if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                        auto enum_ = enum_ty->base.lock();
                        line_map.push_back({n->loc, w.line_count()});
                        w.writeln("bits_flag_alias_method_with_enum(flags_", brgen::nums(seq), "_,", brgen::nums(i), ",", n->ident->ident, ",", enum_->ident->ident, ");");
                        str.map_ident(n->ident, n->ident->ident + "()");
                    }
                    i++;
                }
                bit_fields[non_align.back().get()] = {brgen::concat("flags_", brgen::nums(seq), "_"), non_align};
            }
        }

        std::string get_type_name(const std::shared_ptr<ast::Type>& type) {
            if (auto int_ty = ast::as<ast::IntType>(type); int_ty) {
                return brgen::concat("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(*int_ty->bit_size), "_t");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                return enum_ty->base.lock()->ident->ident;
            }
            if (auto arr_ty = ast::as<ast::ArrayType>(type); arr_ty) {
                if (arr_ty->non_dynamic) {
                    auto len = str.to_string(arr_ty->length);
                    return "std::array<std::uint" + brgen::nums(*arr_ty->base_type->bit_size) + "_t," + len + ">";
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

        void write_struct_union(ast::StructUnionType* union_ty) {
            std::map<std::shared_ptr<ast::StructType>, std::string> tmp;
            map_line(union_ty->loc);
            w.writeln("union {");
            std::string prefix = "union_struct_";
            {
                auto indent = w.indent_scope();
                for (auto& f : union_ty->structs) {
                    auto c = brgen::concat(prefix, brgen::nums(get_seq()));
                    write_struct_type(f, c + ".");
                    auto& t = tmp[f];
                    t = c;
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
                            map_line(f->loc);
                            auto a = str.to_string(f->ident);
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
                            auto struct_ = f->belong_struct.lock();
                            assert(struct_);
                            auto indent = w.indent_scope();
                            auto access = tmp[struct_] + "." + f->ident->ident;
                            anonymous_structs[f.get()] = {access, struct_};
                            map_line(f->loc);
                            w.writeln("this->", access, " = v;");
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
                                    w.writeln("return true;");
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

        void write_struct_type(const std::shared_ptr<ast::StructType>& s, const std::string& prefix = "") {
            auto member = ast::as<ast::Member>(s->base.lock());
            bool has_ident = member && member->ident;
            assert(!member || member->node_type != ast::NodeType::field);
            if (has_ident) {
                map_line(member->loc);
            }
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
                            write_bit_fields(non_aligned, bit_size, is_int_set, include_non_simple);
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
                                w.writeln("std::", int_ty->is_signed ? "" : "u", "int", brgen::nums(*int_ty->bit_size), "_t ", f->ident->ident, ";");
                                str.map_ident(f->ident, "${THIS}", prefix, f->ident->ident);
                            }
                        }
                        if (auto enum_ty = ast::as<ast::EnumType>(type); enum_ty) {
                            auto enum_ = enum_ty->base.lock();
                            auto bit_size = enum_->base_type->bit_size;
                            map_line(f->loc);
                            w.writeln("std::uint", brgen::nums(*bit_size), "_t ", f->ident->ident, "_data;");
                            map_line(f->loc);
                            w.writeln(enum_->ident->ident, " ", f->ident->ident, "() const { return static_cast<", enum_->ident->ident, ">(this->", prefix, f->ident->ident, "_data); }");
                            str.map_ident(f->ident, "${THIS}", f->ident->ident + "()");
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
                            str.map_ident(f->ident, "${THIS}", prefix, f->ident->ident);
                        }
                        if (auto struct_ty = ast::as<ast::StructType>(type)) {
                            auto type_name = get_type_name(type);
                            map_line(f->loc);
                            w.writeln(type_name, " ", f->ident->ident, ";");
                            str.map_ident(f->ident, "${THIS}", prefix, f->ident->ident);
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
                    w.writeln("bool encode(::futils::binary::writer& w) const ;");
                    w.writeln("bool decode(::futils::binary::reader& r);");
                }
            }
            w.write("}");
        }

        void write_simple_struct(const std::shared_ptr<ast::Format>& fmt) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }
            struct_names.push_back(fmt->ident->ident);
            write_struct_type(fmt->body->struct_type);
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
                    w.writeln("return false;");
                }
                w.writeln("}");
                return;
            }
            auto ident = str.to_string(f->ident);
            if (ast::as<ast::EnumType>(f->field_type)) {
                ident = ident.substr(0, ident.size() - 2) + "_data";
            }
            auto typ = f->field_type;
            write_field_encode_impl(f->loc, ident, typ, f);
        }

        void write_field_encode_impl(brgen::lexer::Loc loc, std::string_view ident, std::shared_ptr<ast::Type> typ, ast::Field* fi) {
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            if (auto int_ty = ast::as<ast::IntType>(typ); int_ty) {
                auto type_name = get_type_name(typ);
                map_line(loc);
                w.writeln("if (!::futils::binary::write_num(w,static_cast<", type_name, ">(", ident, ") ,", ctx.endian_text(int_ty->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
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
                    auto length = str.to_string(arr_ty->length);
                    w.writeln("if (", length, "!=", ident, ".size()) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
                    }
                    w.writeln("}");
                }
                map_line(loc);
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->base_type); int_ty && int_ty->bit_size == 8) {
                    w.writeln("if (!w.write(", ident, ")) {");
                    {
                        auto indent = w.indent_scope();
                        w.writeln("return false;");
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
                w.writeln("if (!", ident, ".encode(w)) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
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
                    w.writeln("return false;");
                }
                w.writeln("}");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
                auto l = ast::as<ast::IntType>(enum_ty->base.lock()->base_type);
                map_line(loc);
                w.writeln("if (!::futils::binary::write_num(w,", ident, ",", ctx.endian_text(l->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
                }
                w.writeln("}");
            }
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
                    w.writeln("return false;");
                }
                w.writeln("}");
                return;
            }
            auto ident = str.to_string(f->ident);
            if (ast::as<ast::EnumType>(f->field_type)) {
                ident = ident.substr(0, ident.size() - 2) + "_data";
            }
            auto typ = f->field_type;
            write_field_decode_impl(f->loc, ident, typ, f);
        }

        void write_field_decode_impl(brgen::lexer::Loc loc, std::string_view ident, std::shared_ptr<ast::Type> typ, ast::Field* fi) {
            if (auto ident = ast::as<ast::IdentType>(typ); ident) {
                typ = ident->base.lock();
            }
            if (auto int_ty = ast::as<ast::IntType>(typ); int_ty) {
                map_line(loc);
                w.writeln("if (!::futils::binary::read_num(r,", ident, " ,", ctx.endian_text(int_ty->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
                }
                w.writeln("}");
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
                            w.writeln("return false;");
                        }
                        w.writeln("}");
                        len = brgen::concat("(r.remain().size() - ", require_remain, ")");
                    }
                    else {
                        len = "r.remain().size()";
                    }
                }
                else {
                    len = str.to_string(arr_ty->length);
                }
                map_line(loc);
                if (auto int_ty = ast::as<ast::IntType>(arr_ty->base_type); int_ty && int_ty->bit_size == 8) {
                    if (len) {
                        w.writeln("if (!r.read(", ident, ", ", *len, ")) {");
                        {
                            auto indent = w.indent_scope();
                            w.writeln("return false;");
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
                                w.writeln("return false;");
                            }
                            w.writeln("}");
                            w.writeln("if (", tmp, " == ", std::get<0>(*next), ") {");
                            {
                                auto indent = w.indent_scope();
                                w.writeln("r.reset(", base_offset_tmp, ");");
                                w.writeln("if(!r.read(", ident, ", ", tmp, ".size())){");
                                w.indent_writeln("return false;");
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
                            w.writeln("return false;");
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
                    if (len) {
                        auto tmp_i = brgen::concat("tmp_", brgen::nums(get_seq()), "_");
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
                            w.indent_writeln("return false;");
                            w.writeln("}");
                            w.writeln("if (", tmp, " == ", std::get<0>(*next), ") {");
                            w.indent_writeln("break;");
                            w.writeln("}");
                        }
                        w.writeln(type, " ", tmp, ";");
                        write_field_decode_impl(loc, tmp, arr_ty->base_type, fi);
                        w.writeln(ident, ".push_back(std::move(", tmp, "));");
                    }
                    w.writeln("}");
                }
            }
            if (auto struct_ty = ast::as<ast::StructType>(typ); struct_ty) {
                map_line(loc);
                w.writeln("if (!", ident, ".decode(r)) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
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
                    w.writeln("return false;");
                }
                w.writeln("}");
                map_line(loc);
                w.writeln("if (", tmp_var, " != ::futils::view::rvec(", value, ",", brgen::nums(len), ")) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
                }
                w.writeln("}");
            }
            if (auto enum_ty = ast::as<ast::EnumType>(typ)) {
                auto s = ast::as<ast::IntType>(enum_ty->base.lock()->base_type);
                map_line(loc);
                w.writeln("if (!::futils::binary::read_num(r,", ident, ",", ctx.endian_text(s->endian), ")) {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
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
            if (auto s = ast::as<ast::SpecifyEndian>(elem)) {
                if (ctx.dynamic_endian) {
                    if (s->endian_value) {
                        map_line(s->loc);
                        w.writeln("dynamic_endian____ = !/*reverse for library*/(/*true->little false->big*/", brgen::nums(*s->endian_value), ");");
                    }
                    else {
                        auto specify = str.to_string(s->endian);
                        map_line(s->loc);
                        w.writeln("dynamic_endian____ = !/*reverse for library*/(/*true->little false->big*/", specify, ");");
                    }
                }
                else {
                    ctx.endian = *s->endian_value ? ast::Endian::little : ast::Endian::big;
                }
            }
            if (auto a = ast::as<ast::Assert>(elem); a && (!ctx.encode || a->is_io_related)) {
                auto cond = str.to_string(a->cond);
                w.writeln("if (!", cond, ") {");
                {
                    auto indent = w.indent_scope();
                    w.writeln("return false;");
                }
                w.writeln("}");
            }
        }

        void code_indent_block(const std::shared_ptr<ast::IndentBlock>& block) {
            // encode fields
            for (auto& elem : block->elements) {
                code_one_node(elem);
            }
        }

        void write_code_fn(const std::shared_ptr<ast::Format>& fmt, bool encode) {
            if (fmt->body->struct_type->bit_alignment != ast::BitAlignment::byte_aligned) {
                return;  // skip
            }

            ctx.encode = encode;
            ctx.endian = ast::Endian::big;  // default to big endian
            ctx.dynamic_endian = false;
            for (auto& elem : fmt->body->elements) {
                if (auto f = ast::as<ast::SpecifyEndian>(elem)) {
                    if (!f->endian_value) {
                        ctx.dynamic_endian = true;
                        break;
                    }
                }
            }
            map_line(fmt->loc);
            w.writeln("inline bool ", fmt->ident->ident, "::", encode ? "encode(::futils::binary::writer& w) const" : "decode(::futils::binary::reader& r)", " {");
            {
                auto indent = w.indent_scope();
                if (ctx.dynamic_endian) {
                    map_line(fmt->loc);
                    w.writeln("bool dynamic_endian____ = true /*big endian*/;");
                }
                if (encode) {
                    // first, apply assertions
                    for (auto& elem : fmt->body->elements) {
                        if (auto a = ast::as<ast::Assert>(elem); a && !a->is_io_related) {
                            auto cond = str.to_string(a->cond);
                            w.writeln("if (!", cond, ") {");
                            {
                                auto indent = w.indent_scope();
                                w.writeln("return false;");
                            }
                            w.writeln("}");
                        }
                    }
                }
                // write code
                code_indent_block(fmt->body);
                w.writeln("return true;");
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
            w.writeln("#include <binary/flags.h>");
            w.writeln("#include <vector>");
            w.writeln("#include <array>");
            w.writeln("#include <view/iovec.h>");
            w.writeln("#include <optional>");
            w.writeln("#include <binary/number.h>");
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
