/*license*/
#include <core/ast/tool/stringer.h>
#include <core/ast/tool/sort.h>
#include <writer/writer.h>
#include "generate.h"

namespace json2ts {
    namespace ast = brgen::ast;
    struct Generator {
        ast::tool::Stringer str;
        brgen::writer::Writer w;
        bool encode = false;
        std::string prefix = "${THIS}";
        size_t seq_ = 0;

        size_t get_seq() {
            return seq_++;
        }

        std::string get_type(const std::shared_ptr<ast::Type>& type) {
            if (auto i = ast::as<ast::IntType>(type)) {
                if (i->bit_size <= 32) {
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

        void write_field(const std::shared_ptr<ast::Field>& field) {
            auto typ = field->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (auto u = ast::as<ast::StructUnionType>(typ)) {
                bool first = true;
                auto anonymous_field = "union_" + brgen::nums(get_seq());
                w.writeln(anonymous_field, ": ");
                for (auto s : u->structs) {
                    if (!first) {
                        w.writeln("|");
                    }
                    auto p = std::move(prefix);
                    prefix = brgen::concat(p, anonymous_field, ".");
                    write_struct_type(s);
                    prefix = std::move(p);
                    first = false;
                }
                if (!u->exhaustive) {
                    if (!first) {
                        w.writeln("|");
                    }
                    w.write("undefined");
                }
                w.writeln(";");
                return;
            }
            auto type = get_type(typ);
            w.writeln(field->ident->ident, ": ", type, ";");
            str.map_ident(field->ident, prefix, field->ident->ident);
        }

        void write_struct_type(const std::shared_ptr<ast::StructType>& typ) {
            w.writeln("{");
            {
                auto s = w.indent_scope();
                for (auto& field : typ->fields) {
                    if (auto f = ast::as<ast::Field>(field)) {
                        write_field(ast::cast_to<ast::Field>(field));
                    }
                }
            }
            w.writeln("}");
        }

        void write_if(const std::shared_ptr<ast::If>& if_) {
            auto cond = str.to_string(if_->cond);
            w.write("if (", cond, ") {");
            {
                auto s = w.indent_scope();
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
                        write_single_node(if_->els);
                    }
                    w.writeln("}");
                }
            }
        }

        void write_resize_check(std::string_view len, std::string_view field_name) {
            w.writeln("if (w.offset + (", len, ") > w.view.byteLength) {");
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
            w.writeln("if (r.offset + (", len, ") > r.view.byteLength) {");
            {
                auto s = w.indent_scope();
                w.writeln("throw new Error('out of buffer at ", field_name, "');");
            }
            w.writeln("}");
        }

        void write_field_encode(const std::shared_ptr<ast::Field>& field) {
            auto typ = field->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (field->bit_alignment != field->eventual_bit_alignment) {
            }
            if (auto ity = ast::as<ast::IntType>(typ)) {
                if (ity->is_common_supported) {
                    auto bit = *ity->bit_size;
                    auto sign = ity->is_signed ? "Int" : "Uint";
                    write_resize_check(brgen::nums(bit / 8), field->ident->ident);
                    auto endian = ity->endian == ast::Endian::little ? "true" : "false";
                    auto big = bit > 32 ? "Big" : "";
                    auto ident = str.to_string(field->ident);
                    w.write("w.view.set", big, sign, brgen::nums(bit), "(w.offset, ", ident);
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
                    write_resize_check(brgen::concat(len, " * ", brgen::nums(bit / 8)), field->ident->ident);
                    w.writeln("for (let i = 0; i < ", len, "; i++) {");
                    {
                        auto s = w.indent_scope();
                        auto ident = str.to_string(field->ident);
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
            w.writeln("throw new Error('unsupported type for ", field->ident->ident, "');");
        }

        void write_field_decode(const std::shared_ptr<ast::Field>& field) {
            auto typ = field->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (auto ity = ast::as<ast::IntType>(typ)) {
                if (ity->is_common_supported) {
                    auto bit = *ity->bit_size;
                    auto sign = ity->is_signed ? "Int" : "Uint";
                    read_input_size_check(brgen::nums(bit / 8), field->ident->ident);
                    auto endian = ity->endian == ast::Endian::little ? "true" : "false";
                    auto ident = str.to_string(field->ident);
                    w.write(ident, " = r.view.get", sign, brgen::nums(bit), "(r.offset");
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
                    auto ident = str.to_string(field->ident);
                    auto total = brgen::concat(len, " * ", brgen::nums(bit / 8));
                    read_input_size_check(total, field->ident->ident);
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
                            w.write(ident, "[i] = r.view.get", sign, brgen::nums(bit), "(r.offset + i * ", brgen::nums(bit / 8));
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
            w.writeln("throw new Error('unsupported type for ", field->ident->ident, "');");
        }

        void write_field_code(const std::shared_ptr<ast::Field>& field) {
            if (encode) {
                write_field_encode(field);
            }
            else {
                write_field_decode(field);
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
            else if (auto field = ast::as<ast::Field>(node)) {
                write_field_code(ast::cast_to<ast::Field>(node));
            }
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
            w.write("export interface ", fmt->ident->ident, " ");
            write_struct_type(fmt->body->struct_type);
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
        g.generate(p);
        return g.w.out();
    }
}  // namespace json2ts
