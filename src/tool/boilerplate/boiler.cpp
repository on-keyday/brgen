/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/sort.h>
#include <core/ast/tool/stringer.h>

namespace boiler {
    namespace ast = brgen::ast;
    struct Generator {
        ast::tool::Stringer str;
        virtual void on_metadata(const std::shared_ptr<ast::Metadata>& m) {}
        virtual void on_enum(const std::shared_ptr<ast::Enum>& e) {}

        virtual void on_format_begin(const std::shared_ptr<ast::Format>& f) {}
        virtual void on_format_end(const std::shared_ptr<ast::Format>& f) {}

        virtual void on_struct_type_begin(const std::shared_ptr<ast::StructType>& s) {}
        virtual void on_struct_type_end(const std::shared_ptr<ast::StructType>& s) {}

        virtual void on_field_begin(const std::shared_ptr<ast::Field>& f) {}
        virtual void on_field_end(const std::shared_ptr<ast::Field>& f) {}

        virtual void on_struct_union_type_begin(const std::shared_ptr<ast::StructUnionType>& s) {}
        virtual void on_struct_union_type_end(const std::shared_ptr<ast::StructUnionType>& s) {}

        virtual std::string get_type(const std::shared_ptr<ast::Type>& t) {
            return "";
        }

        virtual void on_field(const std::shared_ptr<ast::Field>& f) {}

        virtual void on_define_constant(const std::shared_ptr<ast::Binary>& b) {}

        void generate_struct_union_type(const std::shared_ptr<ast::StructUnionType>& s) {
            on_struct_union_type_begin(s);
            for (const auto& m : s->structs) {
                generate_struct_type(m);
            }
            on_struct_union_type_end(s);
        }

        void generate_field(const std::shared_ptr<ast::Field>& f) {
            if (!f->ident) {
                f->ident = std::make_shared<ast::Ident>();
                f->ident->ident = "hidden_field";
                f->ident->base = f;
            }
            if (ast::as<ast::StructUnionType>(f->field_type)) {
                generate_struct_union_type(ast::cast_to<ast::StructUnionType>(f->field_type));
            }
            else {
                on_field(f);
            }
        }

        void generate_struct_type(const std::shared_ptr<ast::StructType>& s) {
            on_struct_type_begin(s);
            for (const auto& m : s->fields) {
                if (ast::as<ast::Field>(m)) {
                    generate_field(ast::cast_to<ast::Field>(m));
                }
            }
            on_struct_type_end(s);
        }

        void generate_format(const std::shared_ptr<ast::Format>& f) {
            on_format_begin(f);
            generate_struct_type(f->body->struct_type);
        }

        void generate(const std::shared_ptr<ast::Program>& p) {
            for (const auto& m : p->elements) {
                if (ast::as<ast::Metadata>(m)) {
                    on_metadata(ast::cast_to<ast::Metadata>(m));
                }
            }
            for (const auto& m : p->elements) {
                if (auto b = ast::as<ast::Binary>(m);
                    b && b->op == ast::BinaryOp::const_assign &&
                    b->right->constant_level == ast::ConstantLevel::constant) {
                    on_define_constant(ast::cast_to<ast::Binary>(m));
                }
            }
            for (const auto& m : p->elements) {
                if (ast::as<ast::Enum>(m)) {
                    on_enum(ast::cast_to<ast::Enum>(m));
                }
            }
            ast::tool::FormatSorter sorter;
            auto sorted = sorter.topological_sort(p);
            for (const auto& m : sorted) {
                generate_format(m);
            }
        }
    };
}  // namespace boiler