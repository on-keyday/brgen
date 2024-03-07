/*license*/
#pragma once
#include <ast/tool/stringer.h>
#include <ast/tool/sort.h>
#include <writer/writer.h>

namespace json2ts {
    namespace ast = brgen::ast;
    struct Generator {
        ast::tool::Stringer str;
        brgen::writer::Writer w;

        void write_field(const std::shared_ptr<ast::Field>& field) {
            auto typ = field->field_type;
            if (auto ident = ast::as<ast::IdentType>(typ)) {
                typ = ident->base.lock();
            }
            if (auto i = ast::as<ast::IntType>(typ)) {
                if (i->is_common_supported) {
                    if (i->bit_size <= 32) {
                        w.writeln(field->ident->ident, " : number;");
                    }
                    else {
                        w.writeln(field->ident->ident, " : bigint;");
                    }
                }
            }
        }

        void write_struct_type(const std::shared_ptr<ast::StructType>& typ) {
            for (auto& field : typ->fields) {
                if (auto f = ast::as<ast::Field>(field)) {
                    write_field(ast::cast_to<ast::Field>(field));
                }
            }
        }

        void write_format(const std::shared_ptr<ast::Format>& fmt) {
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
}  // namespace json2ts
