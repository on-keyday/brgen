/*license*/
#include <core/ast/tool/stringer.h>
#include <core/ast/tool/sort.h>
#include <writer/writer.h>
#include "generate.h"

namespace json2ts {
    namespace ast = brgen::ast;

    struct AnonymousType {
        std::string field_name;
        std::string type;
        std::shared_ptr<ast::Field> field;
    };
    struct Generator {
        ast::tool::Stringer str;
        brgen::writer::Writer w;
        bool encode = false;
        std::string prefix = "${THIS}";
        size_t seq_ = 0;
        std::map<std::shared_ptr<ast::StructType>, AnonymousType> anonymous_types;
        std::vector<std::shared_ptr<ast::Field>> bit_fields;

        size_t get_seq() {
            return seq_++;
        }

        std::string get_type(const std::shared_ptr<ast::Type>& type) {
            if (auto i = ast::as<ast::IntType>(type)) {
                if (i->bit_size == 1) {
                    return "boolean";
                }
                else if (i->bit_size <= 32) {
                    return "number";
                }
                else {
                    return "bigint";
                }
            }
            if (auto arr = ast::as<ast::ArrayType>(type)) {
                if (auto ity = ast::as<ast::IntType>(arr->element_type); ity && ity->is_common_supported) {
                    if (ity->bit_size == 8) {
                        if (ity->is_signed) {
                            return "Int8Array";
                        }
                        return "Uint8Array";
                    }
                    if (ity->bit_size == 16) {
                        if (ity->is_signed) {
                            return "Int16Array";
                        }
                        return "Uint16Array";
                    }
                    if (ity->bit_size == 32) {
                        if (ity->is_signed) {
                            return "Int32Array";
                        }
                        return "Uint32Array";
                    }
                    if (ity->bit_size == 64) {
                        if (ity->is_signed) {
                            return "BigInt64Array";
                        }
                        return "BigUint64Array";
                    }
                }
                return "Array<" + get_type(arr->element_type) + ">";
            }
            if (auto ident = ast::as<ast::EnumType>(type)) {
                return ident->base.lock()->ident->ident;
            }
            return "any";
        }

        void write_field(brgen::writer::Writer& wt, const std::shared_ptr<ast::Field>& field) {
            auto typ = field->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (ast::as<ast::UnionType>(typ)) {
                return;
            }
            if (auto u = ast::as<ast::StructUnionType>(typ)) {
                assert(!field->ident);
                bool first = true;
                const auto anonymous_field = "union_" + brgen::nums(get_seq());
                auto ident = std::make_shared<ast::Ident>();
                ident->base = field;
                ident->ident = anonymous_field;
                field->ident = ident;
                str.map_ident(ident, prefix, anonymous_field);
                wt.writeln(anonymous_field, ": ");
                for (auto s : u->structs) {
                    if (!first) {
                        w.writeln("|");
                    }
                    auto p = std::move(prefix);
                    prefix = brgen::concat(p, anonymous_field, "!.");
                    brgen::writer::Writer tmpw;

                    write_struct_type(tmpw, s);

                    prefix = std::move(p);
                    first = false;
                    wt.write_unformatted(tmpw.out());
                    char prev_char = 0;
                    std::erase_if(tmpw.out(), [&](char c) {
                        if (prev_char == ' ' && c == ' ') {
                            return true;
                        }
                        prev_char = c;
                        return c == '\n';
                    });
                    anonymous_types[s] = {anonymous_field, tmpw.out(), field};

                    wt.writeln();
                }
                if (!u->exhaustive) {
                    if (!first) {
                        wt.writeln("|");
                    }
                    wt.write("undefined");
                }
                wt.writeln(";");
                for (auto& f : u->union_fields) {
                    auto field = f.lock();
                    str.map_ident(field->ident, prefix, anonymous_field, "!.", field->ident->ident);
                }
                return;
            }
            auto type = get_type(typ);
            wt.writeln(field->ident->ident, ": ", type, ";");
            str.map_ident(field->ident, prefix, field->ident->ident);
        }

        void write_struct_type(brgen::writer::Writer& wt, const std::shared_ptr<ast::StructType>& typ) {
            wt.writeln("{");
            {
                auto s = wt.indent_scope();
                for (auto& field : typ->fields) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        write_field(wt, ast::cast_to<ast::Field>(field));
                    }
                }
            }
            wt.write("}");
        }

        void assert_obj(AnonymousType& typ, const std::shared_ptr<ast::StructType>& st) {
            if (encode) {
                w.writeln("if(obj.", typ.field_name, " === undefined) {");
                {
                    auto s = w.indent_scope();
                    w.writeln("throw new Error('field ", typ.field_name, " is undefined');");
                }
                w.writeln("}");
                for (auto& field : st->fields) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        auto ident = str.to_string(f->ident);
                        w.writeln("if(", ident, " === undefined) {");
                        {
                            auto s = w.indent_scope();
                            w.writeln("throw new Error('field ", ident, " is undefined');");
                        }
                        w.writeln("}");
                    }
                }
            }
            else {
                w.write("obj.", typ.field_name, " = {} as ");
                w.write_unformatted(typ.type);
                w.writeln(";");
            }
        }

        void write_if(const std::shared_ptr<ast::If>& if_) {
            auto cond = str.to_string(if_->cond);
            w.writeln("if (", cond, ") {");
            {
                auto s = w.indent_scope();
                auto& typ = anonymous_types[if_->then->struct_type];
                assert_obj(typ, if_->then->struct_type);
                write_single_node(if_->then);
            }
            w.writeln("}");
            if (if_->els) {
                w.write("else ");
                if (auto new_if = ast::as<ast::If>(if_->els)) {
                    write_if(ast::cast_to<ast::If>(if_->els));
                }
                else {
                    w.writeln("{");
                    {
                        auto s = w.indent_scope();
                        auto struct_ = ast::as<ast::IndentBlock>(if_->els)->struct_type;
                        auto& typ = anonymous_types[struct_];
                        assert_obj(typ, struct_);
                        write_single_node(if_->els);
                    }
                    w.writeln("}");
                }
            }
        }

        void write_match(const std::shared_ptr<ast::Match>& match) {
            auto cond = match->cond ? str.to_string(match->cond) : "true";
            bool first = true;
            for (auto& m : match->branch) {
                if (!first) {
                    w.write("else ");
                }
                if (ast::is_any_range(m->cond)) {
                    w.writeln("{");
                }
                else {
                    w.writeln("if (", cond, " === ", str.to_string(m->cond), ") {");
                }
                {
                    auto s = w.indent_scope();
                    if (auto scope = ast::as<ast::IndentBlock>(m->then)) {
                        assert_obj(anonymous_types[scope->struct_type], scope->struct_type);
                    }
                    else {
                        auto s = ast::as<ast::ScopedStatement>(m->then);
                        assert_obj(anonymous_types[s->struct_type], s->struct_type);
                    }
                    write_single_node(m->then);
                }
                w.writeln("}");
                first = false;
            }
        }

        void write_resize_check(std::string_view len, std::string_view field_name) {
            w.writeln("if (w.offset + ", len, " > w.view.byteLength) {");
            {
                auto s = w.indent_scope();
                w.writeln("// check resize method existence");
                w.writeln("// TODO: resize method is experimental feature so that we cast to any");
                w.writeln("if(typeof (w.view as any).resize == 'function') {");
                {
                    auto s = w.indent_scope();
                    w.writeln("(w.view as any).resize(w.view.byteLength + (", len, "));");
                }
                w.writeln("}");
                w.writeln("else {");
                {
                    auto s = w.indent_scope();
                    w.writeln("throw new Error('out of buffer at ", field_name, "');");
                }
                w.writeln("}");
            }
            w.writeln("}");
        }

        void read_input_size_check(std::string_view len, std::string_view field_name) {
            w.writeln("if (r.offset + ", len, " > r.view.byteLength) {");
            {
                auto s = w.indent_scope();
                w.writeln("throw new Error('out of buffer at ", field_name, "');");
            }
            w.writeln("}");
        }

        void write_type_encode(std::string_view err_ident, std::string_view ident, const std::shared_ptr<ast::Type>& type) {
            auto typ = type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }

            if (auto ity = ast::as<ast::IntType>(typ)) {
                if (ity->is_common_supported) {
                    auto bit = *ity->bit_size;
                    auto sign = ity->is_signed ? "Int" : "Uint";
                    write_resize_check(brgen::nums(bit / 8), err_ident);
                    auto endian = ity->endian == ast::Endian::little ? "true" : "false";
                    auto big = bit > 32 ? "Big" : "";
                    // auto ident = str.to_string(field->ident);
                    auto typ_str = get_type(typ);
                    w.write("w.view.set", big, sign, brgen::nums(bit), "(w.offset, ", ident, " as ", typ_str);
                    if (bit != 8) {
                        w.write(", ", endian);
                    }
                    w.writeln(");");
                    w.writeln("w.offset += ", brgen::nums(bit / 8), ";");
                    return;
                }
            }
            else if (auto arr = ast::as<ast::ArrayType>(typ)) {
                auto elm = arr->element_type;
                if (auto typ = ast::as<ast::IntType>(elm); typ && typ->is_common_supported) {
                    auto bit = *typ->bit_size;
                    auto sign = typ->is_signed ? "Int" : "Uint";
                    auto len = str.to_string(arr->length);
                    auto endian = typ->endian == ast::Endian::little ? "true" : "false";
                    auto big = bit > 32 ? "Big" : "";
                    auto class_ = brgen::concat(big, sign, brgen::nums(bit));
                    write_resize_check(brgen::concat(len, " * ", brgen::nums(bit / 8)), err_ident);
                    w.writeln("for (let i = 0; i < ", len, "; i++) {");
                    {
                        auto s = w.indent_scope();
                        // auto ident = str.to_string(field->ident);
                        auto typ = get_type(arr->element_type);
                        w.write("w.view.set", class_, "(w.offset + i * ", brgen::nums(bit / 8), ", ", ident, "[i]");
                        if (bit != 8) {
                            w.write(", ", endian);
                        }
                        w.writeln(");");
                    }
                    w.writeln("}");
                    w.writeln("w.offset += ", len, " * ", brgen::nums(bit / 8), ";");
                    return;
                }
            }
            w.writeln("throw new Error('unsupported type for ", ident, "');");
        }

        void write_type_decode(std::string_view err_ident, std::string_view ident, const std::shared_ptr<ast::Type>& type) {
            auto typ = type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (auto ity = ast::as<ast::IntType>(typ)) {
                if (ity->is_common_supported) {
                    auto bit = *ity->bit_size;
                    auto sign = ity->is_signed ? "Int" : "Uint";
                    read_input_size_check(brgen::nums(bit / 8), err_ident);
                    auto endian = ity->endian == ast::Endian::little ? "true" : "false";
                    // auto ident = str.to_string(field->ident);
                    auto big = bit > 32 ? "Big" : "";
                    w.write(ident, " = r.view.get", big, sign, brgen::nums(bit), "(r.offset");
                    if (bit != 8) {
                        w.write(", ", endian);
                    }
                    w.writeln(");");
                    w.writeln("r.offset += ", brgen::nums(bit / 8), ";");
                    return;
                }
            }
            else if (auto arr = ast::as<ast::ArrayType>(typ)) {
                auto elm = arr->element_type;
                if (auto typ = ast::as<ast::IntType>(elm); typ && typ->is_common_supported) {
                    auto bit = *typ->bit_size;
                    auto sign = typ->is_signed ? "Int" : "Uint";
                    auto len = str.to_string(arr->length);
                    auto endian = typ->endian == ast::Endian::little ? "true" : "false";
                    auto big = bit > 32 ? "Big" : "";
                    auto class_ = brgen::concat(big, sign, brgen::nums(bit), "Array");
                    // auto ident = str.to_string(field->ident);
                    auto total = brgen::concat(len, " * ", brgen::nums(bit / 8));
                    read_input_size_check(total, err_ident);
                    if (bit == 8) {
                        w.writeln(ident, " = new ", class_, "(r.view.buffer, r.offset, ", len, ")");
                    }
                    else {
                        auto native_endian = "((new Uint8Array( Uint16Array.of( 1 ).buffer ))[0] === 1)";
                        w.writeln("if(", native_endian, " === ", endian, ") {");
                        w.indent_writeln(ident, " = new ", class_, "(r.view.buffer, r.offset, ", len, ")");
                        w.writeln("else {");
                        w.indent_writeln(ident, " = new ", class_, "(r.view.buffer, r.offset, ", len, ")");
                        w.writeln("for (let i = 0; i < ", len, "; i++) {");
                        {
                            auto s = w.indent_scope();
                            w.write(ident, "[i] = r.view.get", big, sign, brgen::nums(bit), "(r.offset + i * ", brgen::nums(bit / 8));
                            if (bit != 8) {
                                w.write(", ", endian);
                            }
                            w.writeln(");");
                        }
                        w.writeln("}");
                    }
                    w.writeln("r.offset += ", total, ";");
                    return;
                }
            }
            w.writeln("throw new Error('unsupported type for ", err_ident, "');");
        }

        void write_field_code(const std::shared_ptr<ast::Field>& field) {
            if (encode) {
                auto ident = str.to_string(field->ident);
                write_type_encode(field->ident->ident, ident, field->field_type);
            }
            else {
                auto ident = str.to_string(field->ident);
                write_type_decode(field->ident->ident, ident, field->field_type);
            }
        }

        void write_single_node(const std::shared_ptr<ast::Node>& node, bool top = false) {
            if (auto block = ast::as<ast::IndentBlock>(node)) {
                for (auto& elem : block->elements) {
                    write_single_node(elem);
                }
            }
            else if (auto fmt = ast::as<ast::ScopedStatement>(node)) {
                write_single_node(fmt->statement);
            }
            else if (auto if_ = ast::as<ast::If>(node)) {
                write_if(ast::cast_to<ast::If>(node));
            }
            else if (auto m = ast::as<ast::Match>(node)) {
                write_match(ast::cast_to<ast::Match>(node));
            }
            else if (auto field = ast::as<ast::Field>(node)) {
                write_field_code(ast::cast_to<ast::Field>(node));
            }
            else if (auto bin = ast::as<ast::Binary>(node); bin && ast::is_assign_op(bin->op)) {
                if (bin->op == ast::BinaryOp::define_assign) {
                    auto init = str.to_string(bin->right);
                    auto ident = ast::cast_to<ast::Ident>(bin->left);
                    w.writeln("let ", ident->ident, " = ", init, ";");
                    str.map_ident(ident, ident->ident);
                }
                if (bin->op == ast::BinaryOp::const_assign) {
                    auto init = str.to_string(bin->right);
                    auto ident = ast::cast_to<ast::Ident>(bin->left);
                    w.writeln("const ", ident->ident, " = ", init, ";");
                    str.map_ident(ident, ident->ident);
                }
                else {
                    auto ident = str.to_string(bin->left);
                    auto right = str.to_string(bin->right);
                    w.writeln(ident, " ", ast::to_string(bin->op), " ", right, ";");
                }
            }
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
            w.write("export interface ", fmt->ident->ident, " ");
            write_struct_type(w, fmt->body->struct_type);
            w.writeln(";");
            w.write("export function ", fmt->ident->ident, "_encode(w :{view :DataView,offset :number}, obj: ", fmt->ident->ident, ") {");
            {
                encode = true;
                auto s = w.indent_scope();
                w.writeln("// ensure offset is unsigned integer");
                w.writeln("w.offset >>>= 0;");
                write_single_node(fmt->body, true);
            }
            w.writeln("}");
            w.writeln("export function ", fmt->ident->ident, "_decode(r :{view :DataView,offset :number}): ", fmt->ident->ident, " {");
            {
                encode = false;
                auto s = w.indent_scope();
                w.writeln("// ensure offset is unsigned integer");
                w.writeln("r.offset >>>= 0;");
                w.writeln("const obj = {} as ", fmt->ident->ident, ";");
                write_single_node(fmt->body, true);
                w.writeln("return obj;");
            }
            w.writeln("}");
        }

        void generate(const std::shared_ptr<ast::Program>& p) {
            for (auto& elem : p->elements) {
                if (auto enum_ = ast::as<ast::Enum>(elem)) {
                    w.writeln("export const enum ", enum_->ident->ident, " {");
                    {
                        auto s = w.indent_scope();
                        for (auto& elem : enum_->members) {
                            auto v = str.to_string(elem->value);
                            w.writeln(elem->ident->ident, " = ", v, ",");
                            str.map_ident(elem->ident, enum_->ident->ident, ".", elem->ident->ident);
                        }
                    }
                    w.writeln("}");
                }
            }
            auto s = ast::tool::FormatSorter{};
            auto sorted = s.topological_sort(p);
            for (auto& elem : sorted) {
                write_format(elem);
            }
        }
    };

    std::string generate(const std::shared_ptr<brgen::ast::Program>& p) {
        Generator g;
        g.str.this_access = "obj.";
        g.str.cast_handler = [](ast::tool::Stringer& s, const std::shared_ptr<ast::Cast>& c) {
            return s.to_string(c->expr);
        };
        g.str.bin_op_map[ast::BinaryOp::equal] = [](ast::tool::Stringer& s, const std::shared_ptr<ast::Binary>& v) {
            if (auto ity = ast::as<ast::IntType>(v->left->expr_type)) {
                if (ity->bit_size == 1) {
                    return "(" + s.to_string(v->left) + "?1:0)" + " === " + s.to_string(v->right);
                }
            }
            return s.to_string(v->left) + " === " + s.to_string(v->right);
        };
        g.generate(p);
        return g.w.out();
    }
}  // namespace json2ts
