/*license*/
#pragma once
#include <core/ast/ast.h>
#include <core/ast/tool/sort.h>
#include <core/ast/tool/stringer.h>

namespace boiler {
    namespace ast = brgen::ast;
    struct Generator {
        ast::tool::Stringer str;
        bool encode = false;
        bool custom_fn = false;
        virtual void on_metadata(const std::shared_ptr<ast::Metadata>& m) {}
        virtual void on_enum_begin(const std::shared_ptr<ast::Enum>& e) {}
        virtual void on_enum_member(const std::shared_ptr<ast::EnumMember>& e) {}
        virtual void on_enum_end(const std::shared_ptr<ast::Enum>& e) {}

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
        virtual void on_floating_field(const std::shared_ptr<ast::Field>& f) {}

        virtual void on_define(const std::shared_ptr<ast::Binary>& b) {}

        virtual void on_if(const std::shared_ptr<ast::If>& i) {}
        virtual void on_elif(const std::shared_ptr<ast::If>& e) {}
        virtual void on_else(const std::shared_ptr<ast::IndentBlock>& e) {}
        virtual void on_end_if(const std::shared_ptr<ast::If>& e) {}

        virtual void on_match(const std::shared_ptr<ast::Match>& m) {}
        virtual void on_match_branch(const std::shared_ptr<ast::MatchBranch>& m) {}
        virtual void on_end_match(const std::shared_ptr<ast::Match>& m) {}

        virtual void on_encode_field_begin(const std::shared_ptr<ast::Field>& f) {}
        virtual void on_encode_field_end(const std::shared_ptr<ast::Field>& f) {}
        virtual void on_decode_field_begin(const std::shared_ptr<ast::Field>& f) {}
        virtual void on_decode_field_end(const std::shared_ptr<ast::Field>& f) {}

        virtual void on_encode_int(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::IntType>& t, std::string& ctx) {}
        virtual void on_decode_int(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::IntType>& t, std::string& ctx) {}
        virtual void on_encode_float(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::FloatType>& t, std::string& ctx) {}
        virtual void on_decode_float(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::FloatType>& t, std::string& ctx) {}
        virtual void on_encode_enum(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::EnumType>& t, std::string& ctx) {}
        virtual void on_decode_enum(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::EnumType>& t, std::string& ctx) {}
        virtual void on_encode_struct(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::StructType>& t, std::string& ctx) {}
        virtual void on_decode_struct(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::StructType>& t, std::string& ctx) {}
        virtual void on_encode_str_literal(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::StrLiteralType>& t, std::string& ctx) {}
        virtual void on_decode_str_literal(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::StrLiteralType>& t, std::string& ctx) {}

        virtual void on_encode_array_begin(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::ArrayType>& t, std::string& ctx) {}
        virtual void on_encode_array_end(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::ArrayType>& t, std::string& ctx) {}

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

        void generate_if(const std::shared_ptr<ast::If>& i) {
            on_if(i);
            generate_node(i->then);
            auto prev = i.get();
            for (auto i_ = ast::as<ast::If>(i->els); i_; prev = i_, i_ = ast::as<ast::If>(i->els)) {
                on_elif(ast::cast_to<ast::If>(i->els));
                generate_node(i_->then);
            }
            if (auto els = ast::as<ast::IndentBlock>(prev->els)) {
                auto block = ast::cast_to<ast::IndentBlock>(prev->els);
                on_else(block);
                generate_node(block);
            }
            on_end_if(i);
        }

        void generate_match(const std::shared_ptr<ast::Match>& m) {
            on_match(m);
            for (auto& b : m->branch) {
                on_match_branch(b);
                generate_node(b->then);
            }
            on_end_match(m);
        }

        void generate_type_encode(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::Type>& t, std::string& ctx) {
            auto typ = t;
            if (auto ity = ast::as<ast::IdentType>(t)) {
                typ = ity->base.lock();
            }
            if (auto ity = ast::as<ast::IntType>(typ)) {
                on_encode_int(f, ast::cast_to<ast::IntType>(typ), ctx);
            }
            else if (auto fty = ast::as<ast::FloatType>(typ)) {
                on_encode_float(f, ast::cast_to<ast::FloatType>(typ), ctx);
            }
            else if (auto aty = ast::as<ast::ArrayType>(typ)) {
                on_encode_array_begin(f, ast::cast_to<ast::ArrayType>(typ), ctx);
                generate_type_encode(f, aty->element_type, ctx);
                on_encode_array_end(f, ast::cast_to<ast::ArrayType>(typ), ctx);
            }
            else if (auto ety = ast::as<ast::EnumType>(typ)) {
                on_encode_enum(f, ast::cast_to<ast::EnumType>(typ), ctx);
            }
            else if (auto sty = ast::as<ast::StructType>(typ)) {
                on_encode_struct(f, ast::cast_to<ast::StructType>(typ), ctx);
            }
            else if (auto ty = ast::as<ast::StrLiteralType>(typ)) {
                on_encode_str_literal(f, ast::cast_to<ast::StrLiteralType>(typ), ctx);
            }
        }

        void generate_type_decode(const std::shared_ptr<ast::Field>& f, const std::shared_ptr<ast::Type>& t, std::string& ctx) {
            auto typ = t;
            if (auto ity = ast::as<ast::IdentType>(t)) {
                typ = ity->base.lock();
            }
            if (auto ity = ast::as<ast::IntType>(typ)) {
                on_decode_int(f, ast::cast_to<ast::IntType>(typ), ctx);
            }
            else if (auto fty = ast::as<ast::FloatType>(typ)) {
                on_decode_float(f, ast::cast_to<ast::FloatType>(typ), ctx);
            }
            else if (auto aty = ast::as<ast::ArrayType>(typ)) {
                on_encode_array_begin(f, ast::cast_to<ast::ArrayType>(typ), ctx);
                generate_type_decode(f, aty->element_type, ctx);
                on_encode_array_end(f, ast::cast_to<ast::ArrayType>(typ), ctx);
            }
            else if (auto ety = ast::as<ast::EnumType>(typ)) {
                on_decode_enum(f, ast::cast_to<ast::EnumType>(typ), ctx);
            }
            else if (auto sty = ast::as<ast::StructType>(typ)) {
                on_decode_struct(f, ast::cast_to<ast::StructType>(typ), ctx);
            }
            else if (auto ty = ast::as<ast::StrLiteralType>(typ)) {
                on_decode_str_literal(f, ast::cast_to<ast::StrLiteralType>(typ), ctx);
            }
        }

        void generate_field_encode(const std::shared_ptr<ast::Field>& f) {
            on_encode_field_begin(f);
            std::string ctx = f->ident->ident;
            generate_type_encode(f, f->field_type, ctx);
            on_encode_field_end(f);
        }

        void generate_field_decode(const std::shared_ptr<ast::Field>& f) {
            on_decode_field_begin(f);
            std::string ctx = f->ident->ident;
            generate_type_decode(f, f->field_type, ctx);
            on_decode_field_end(f);
        }

        void generate_node(const std::shared_ptr<ast::Node>& n) {
            if (auto block = ast::as<ast::IndentBlock>(n)) {
                for (const auto& m : block->elements) {
                    generate_node(m);
                }
            }
            else if (auto s = ast::as<ast::ScopedStatement>(n)) {
                generate_node(s->statement);
            }
            else if (auto if_ = ast::as<ast::If>(n)) {
                generate_if(ast::cast_to<ast::If>(n));
            }
            else if (auto m = ast::as<ast::Match>(n)) {
                generate_match(ast::cast_to<ast::Match>(n));
            }
            else if (auto b = ast::as<ast::Binary>(n)) {
                if (b->op == ast::BinaryOp::define_assign ||
                    b->op == ast::BinaryOp::const_assign) {
                    on_define(ast::cast_to<ast::Binary>(n));
                }
            }
            else if (auto f = ast::as<ast::Field>(n)) {
                if (custom_fn) {
                    on_floating_field(ast::cast_to<ast::Field>(n));
                }
                if (encode) {
                    generate_field_encode(ast::cast_to<ast::Field>(n));
                }
                else {
                    generate_field_decode(ast::cast_to<ast::Field>(n));
                }
            }
        }

        void generate_format(const std::shared_ptr<ast::Format>& f) {
            on_format_begin(f);
            generate_struct_type(f->body->struct_type);
            encode = true;
            if (auto fn = f->encode_fn.lock()) {
                custom_fn = true;
                generate_node(fn->body);
                custom_fn = false;
            }
            else {
                generate_node(f->body);
            }
            encode = false;
            if (auto fn = f->decode_fn.lock()) {
                custom_fn = true;
                generate_node(fn->body);
                custom_fn = false;
            }
            else {
                generate_node(f->body);
            }
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
                    on_define(ast::cast_to<ast::Binary>(m));
                }
            }
            for (const auto& m : p->elements) {
                if (auto e = ast::as<ast::Enum>(m)) {
                    on_enum_begin(ast::cast_to<ast::Enum>(m));
                    for (const auto& m : e->members) {
                        on_enum_member(ast::cast_to<ast::EnumMember>(m));
                    }
                    on_enum_end(ast::cast_to<ast::Enum>(m));
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
