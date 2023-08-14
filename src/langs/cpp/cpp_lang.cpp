/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"

namespace brgen::cpp_lang {
    void write_expr(writer::TreeWriter& w, ast::Expr* expr);
    void write_block(writer::TreeWriter& w, ast::node_list& elements, bool last_should_return);

    void write_temporary_func(writer::TreeWriter& parent) {
        writer::TreeWriter w{"tmp", parent.lookup("global_def")};
        w.out().writeln("int temporary_func", nums(0), "() {");
        w.out().writeln("}");
        w.out().line();
    }

    void write_binary(writer::TreeWriter& w, ast::Binary* b) {
        auto op = b->op;
        bool paren = op == ast::BinaryOp::bit_and || op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
        if (paren) {
            w.out().write("(");
        }
        write_expr(w, b->left.get());
        w.out().write(" ", *ast::bin_op_str(op), " ");
        write_expr(w, b->right.get());
        if (paren) {
            w.out().write(")");
        }
    }

    void write_block_scope(writer::TreeWriter& parent, ast::IndentScope* block, bool last_should_be_return) {
        writer::TreeWriter w{"block", &parent};
        w.out().writeln("{");
        {
            auto scope = w.out().indent_scope();
            write_block(w, block->elements, last_should_be_return);
        }
        w.out().writeln("}");
    }

    void write_if_stmt(writer::TreeWriter& w, ast::If* if_, bool last_should_be_return) {
        w.out().write("if(");
        write_expr(w, if_->cond.get());
        w.out().write(") ");
        write_block_scope(w, if_->block.get(), last_should_be_return);
        if (if_->els) {
            if (auto elif = ast::as<ast::If>(if_->els)) {
                w.out().write("else ");
                write_if_stmt(w, elif, last_should_be_return);
            }
            else if (auto els = ast::as<ast::IndentScope>(if_->els)) {
                w.out().write("else ");
                write_block_scope(w, els, last_should_be_return);
            }
        }
    }

    void write_expr(writer::TreeWriter& w, ast::Expr* expr) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            write_binary(w, b);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            if (ident->usage == ast::IdentUsage::reference) {
                w.out().write(ident->ident);
            }
            else if (ident->usage == ast::IdentUsage::define_const) {
                w.out().write("const int ", ident->ident);
            }
            else {
                w.out().write("int ", ident->ident);
            }
        }
        else if (auto num = ast::as<ast::IntLiteral>(expr)) {
            w.out().write("0x", nums(*num->parse_as<std::uint64_t>(), 16));
        }
        else if (auto call = ast::as<ast::Call>(expr)) {
        }
        else if (auto paren = ast::as<ast::Paren>(expr)) {
            w.out().write("(");
            write_expr(w, paren->expr.get());
            w.out().write(")");
        }
        else if (auto if_ = ast::as<ast::If>(expr)) {
            if (if_->expr_type->type == ast::ObjectType::void_type) {
                write_if_stmt(w, if_, false);
            }
            else {
                w.out().writeln("[&]{");
                {
                    auto scope = w.out().indent_scope();
                    write_if_stmt(w, if_, true);
                }
                w.out().write("}()");
            }
        }
        else if (auto cond = ast::as<ast::Cond>(expr)) {
            w.out().write("(");
            write_expr(w, cond->cond.get());
            w.out().write(")?(");
            write_expr(w, cond->then.get());
            w.out().write("):(");
            write_expr(w, cond->els.get());
            w.out().write(")");
        }
        else if (auto unary = ast::as<ast::Unary>(expr)) {
            w.out().write(ast::unary_op[int(unary->op)], " ");
            write_expr(w, unary->target.get());
        }
    }

    void write_block(writer::TreeWriter& w, ast::node_list& elements, bool last_should_be_return) {
        auto scope = w.out().indent_scope();
        for (auto it = elements.begin(); it != elements.end(); it++) {
            auto& element = *it;
            if (last_should_be_return && it == --elements.end()) {
                w.out().write("return ");
            }
            if (auto a = ast::as_Expr(element)) {
                writer::TreeWriter child{"child", &w};
                write_expr(child, a);
                child.out().writeln(";");
            }
        }
    }

    void entry(writer::TreeWriter& w, std::shared_ptr<ast::Program>& p) {
        writer::TreeWriter global_dec{"global_dec", &w};
        writer::TreeWriter global_def{"global_def", &global_dec};
        writer::TreeWriter main_{"main", &global_def};
        main_.out().writeln("int main() {");
        write_block(main_, p->elements, false);
        main_.out().writeln("}");
    }

}  // namespace brgen::cpp_lang
