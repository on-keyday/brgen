/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"

namespace brgen::c_lang {
    void write_expr(writer::TreeWriter& w, ast::Expr* expr, ast::Expr* parent);
    void write_block(writer::TreeWriter& w, ast::node_list& elements);

    void write_temporary_func(writer::TreeWriter& parent) {
        writer::TreeWriter w{"tmp", parent.lookup("global_def")};
        w.code().writeln("int temporary_func", nums(0), "() {");
        w.code().writeln("}");
        w.code().line();
    }

    void write_binary(writer::TreeWriter& w, ast::Binary* b, ast::Expr* parent) {
        if (b->op == ast::BinaryOp::assign &&
            (b->left->type == ast::ObjectType::ident || b->left->type == ast::ObjectType::tmp_var) &&
            b->right->type == ast::ObjectType::if_) {
            w.code().writeln();
            write_temporary_func(w);
        }
        auto op = b->op;
        bool paren = op == ast::BinaryOp::bit_and || op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
        if (paren) {
            w.code().write("(");
        }
        write_expr(w, b->left.get(), b);
        w.code().write(" ", *ast::bin_op_str(op), " ");
        write_expr(w, b->right.get(), b);
        if (paren) {
            w.code().write(")");
        }
    }

    void write_expr(writer::TreeWriter& w, ast::Expr* expr, ast::Expr* parent) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            write_binary(w, b, parent);
        }
        else if (auto ident = ast::as<ast::Ident>(expr)) {
            if (parent && parent->object_type == ast::ObjectType::binary &&
                ast::as<ast::Binary>(parent)->op == ast::BinaryOp::assign) {
                w.code().write("int ", ident->ident);
            }
            else {
                w.code().write(ident->ident);
            }
        }
        else if (auto num = ast::as<ast::IntLiteral>(expr)) {
            w.code().write("0x", nums(*num->parse_as<std::uint64_t>(), 16));
        }
        else if (auto tmp = ast::as<ast::TmpVar>(expr)) {
            if (parent && parent->object_type == ast::ObjectType::binary &&
                ast::as<ast::Binary>(parent)->op == ast::BinaryOp::assign) {
                w.code().write("int tmp", nums(tmp->tmp_index));
            }
            else {
                w.code().write("tmp", nums(tmp->tmp_index));
            }
        }
        else if (auto call = ast::as<ast::Call>(expr)) {
        }
    }

    void write_block(writer::TreeWriter& w, ast::node_list& elements) {
        auto scope = w.code().indent_scope();
        for (auto& element : elements) {
            if (auto a = ast::as_Expr(element)) {
                writer::TreeWriter child{"child", &w};
                write_expr(child, a, nullptr);
                child.code().writeln(";");
            }
        }
    }

    void entry(writer::TreeWriter& w, std::shared_ptr<ast::Program>& p) {
        writer::TreeWriter global_dec{"global_dec", &w};
        writer::TreeWriter global_def{"global_def", &global_dec};
        writer::TreeWriter main_{"main", &global_def};
        main_.code().writeln("int main() {");
        write_block(main_, p->elements);
        main_.code().writeln("}");
    }

}  // namespace brgen::c_lang
