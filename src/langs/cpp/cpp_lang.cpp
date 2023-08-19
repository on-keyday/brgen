/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"
#include <core/writer/section.h>
#include "cpp_lang.h"

namespace brgen::cpp_lang {
    using writer::SectionPtr;
    void write_expr(const SectionPtr& w, ast::Expr* expr);
    void write_block(const SectionPtr& w, ast::node_list& elements, bool last_should_return);

    void report_error() {}

    void write_binary(const SectionPtr& w, ast::Binary* b) {
        auto op = b->op;
        bool paren = op == ast::BinaryOp::bit_and || op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
        if (paren) {
            w->write("(");
        }
        write_expr(w, b->left.get());
        w->write(" ", *ast::bin_op_str(op), " ");
        write_expr(w, b->right.get());
        if (paren) {
            w->write(")");
        }
    }

    void write_block_scope(const SectionPtr& w, ast::IndentScope* block, bool last_should_be_return) {
        auto b = w->add_section("block", true);
        b->head().writeln("{");
        b->foot().writeln("}");
        write_block(b, block->elements, last_should_be_return);
    }

    void write_if_stmt(const SectionPtr& w, ast::If* if_, bool last_should_be_return) {
        auto if_stmt = w->add_section(".");
        auto cond = if_stmt->add_section("cond");
        cond->head().write("if(");
        cond->foot().write(") ");
        write_expr(cond, if_->cond.get());
        write_block_scope(if_stmt, if_->block.get(), last_should_be_return);
        if (if_->els) {
            w->write("else ");
            if (auto elif = ast::as<ast::If>(if_->els)) {
                write_if_stmt(w, elif, last_should_be_return);
            }
            else if (auto els = ast::as<ast::IndentScope>(if_->els)) {
                write_block_scope(w, els, last_should_be_return);
            }
        }
    }

    void write_expr(const SectionPtr& w, ast::Expr* expr) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            write_binary(w, b);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            if (ident->usage == ast::IdentUsage::reference) {
                w->write(ident->ident);
            }
            else if (ident->usage == ast::IdentUsage::define_const) {
                w->write("const int ", ident->ident);
            }
            else {
                w->write("int ", ident->ident);
            }
        }
        else if (auto num = ast::as<ast::IntLiteral>(expr)) {
            w->write("0x", nums(*num->parse_as<std::uint64_t>(), 16));
        }
        else if (auto call = ast::as<ast::Call>(expr)) {
        }
        else if (auto paren = ast::as<ast::Paren>(expr)) {
            w->write("(");
            write_expr(w, paren->expr.get());
            w->write(")");
        }
        else if (auto if_ = ast::as<ast::If>(expr)) {
            if (if_->expr_type->type == ast::ObjectType::void_type) {
                write_if_stmt(w, if_, false);
            }
            else {
                auto lambda = w->add_section(".", true);
                lambda->head().writeln("[&]{");
                lambda->foot().write("}()");
                write_if_stmt(lambda, if_, true);
            }
        }
        else if (auto cond = ast::as<ast::Cond>(expr)) {
            w->write("(");
            write_expr(w, cond->cond.get());
            w->write(")?(");
            write_expr(w, cond->then.get());
            w->write("):(");
            write_expr(w, cond->els.get());
            w->write(")");
        }
        else if (auto unary = ast::as<ast::Unary>(expr)) {
            w->write(ast::unary_op[int(unary->op)], " ");
            write_expr(w, unary->target.get());
        }
    }

    void write_block(Context& w, ast::node_list& elements, bool last_should_be_return) {
        for (auto it = elements.begin(); it != elements.end(); it++) {
            auto& element = *it;
            auto stmt = w.w->add_section(".");
            if (last_should_be_return && it == --elements.end()) {
                stmt->write("return ");
            }
            if (auto a = ast::as_Expr(element)) {
                write_expr(stmt, a);
                stmt->writeln(";");
            }
            if (auto f = ast::as<ast::Field>(element)) {
                if (f->ident) {
                    stmt->writeln("int ", f->ident->ident, ";");
                }
                if (f->field_type->type != ast::ObjectType::int_type) {
                }
            }
        }
    }

    void entry(Context& w, std::shared_ptr<ast::Program>& p) {
        auto root = writer::root();
        root->head().writeln("#include<cstdint>");
        root->head().writeln("#include<cstddef>");
        auto global = root->add_section("global");
        auto dec = global->add_section("dec");
        dec->writeln("struct Input {std::size_t bit_index = 0; const std::uint8_t buffer[1200];};");
        auto def = global->add_section("def");
        auto main_ = root->add_section("main", true);
        main_->head().writeln("int main() {");
        main_->foot().writeln("}");
        main_->writeln("Input input__{},*input=&input__;");
        write_block(main_, p->elements, false);
        w.w = root;
    }

}  // namespace brgen::cpp_lang
