/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"

namespace brgen::cpp_lang {
    void write_expr(writer::TreeWriter& w, ast::Expr* expr);
    void write_block(writer::TreeWriter& w, ast::node_list& elements);

    void write_temporary_func(writer::TreeWriter& parent) {
        writer::TreeWriter w{"tmp", parent.lookup("global_def")};
        w.out().writeln("int temporary_func", nums(0), "() {");
        w.out().writeln("}");
        w.out().line();
    }

    void write_binary(writer::TreeWriter& w, ast::Binary* b) {
        if (b->op == ast::BinaryOp::assign &&
            (b->left->type == ast::ObjectType::ident || b->left->type == ast::ObjectType::tmp_var) &&
            b->right->type == ast::ObjectType::if_) {
            w.out().writeln();
            write_temporary_func(w);
        }
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
    }

    void write_block(writer::TreeWriter& w, ast::node_list& elements) {
        auto scope = w.out().indent_scope();
        for (auto& element : elements) {
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
        write_block(main_, p->elements);
        main_.out().writeln("}");
    }

}  // namespace brgen::cpp_lang
