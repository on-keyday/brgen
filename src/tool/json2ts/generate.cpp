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
            if (auto ident = ast::as<ast::IdentType>(type)) {
                return ident->ident->ident;
            }
            return "any";
        }

        void write_field(const std::shared_ptr<ast::Field>& field) {
            auto typ = field->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            auto type = get_type(typ);
            w.writeln(field->ident->ident, ": ", type, ";");
        }

        void write_struct_type(const std::shared_ptr<ast::StructType>& typ) {
            w.writeln("{");
            for (auto& field : typ->fields) {
                if (auto f = ast::as<ast::Field>(field)) {
                    write_field(ast::cast_to<ast::Field>(field));
                }
            }
            w.writeln("}");
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
            w.writeln("export interface ", fmt->ident->ident, " ");
            {
                auto s = w.indent_scope();
                write_struct_type(fmt->body->struct_type);
            }
        }

        void generate(const std::shared_ptr<ast::Program>& p) {
            for (auto& elem : p->elements) {
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
        g.generate(p);
        return g.w.out();
    }
}  // namespace json2ts
