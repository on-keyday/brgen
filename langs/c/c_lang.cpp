/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"
#include "core/ast/translated.h"
#include "core/ast/translated.h"

namespace c_lang {

    void write_expr(writer::TreeWriter& w, ast::Expr* expr, ast::Expr* parent) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            auto op = b->op;
            bool paren = op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
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
        if (auto tmp = ast::as<ast::TmpVar>(expr)) {
            if (parent && parent->object_type == ast::ObjectType::binary &&
                ast::as<ast::Binary>(parent)->op == ast::BinaryOp::assign) {
                w.code().write("int tmp", ast::nums(tmp->tmp_index));
            }
            else {
                w.code().write("tmp", ast::nums(tmp->tmp_index));
            }
        }
    }

    void entry(writer::TreeWriter& w, std::shared_ptr<ast::Program>& p) {
        w.code().write("int main() {");
        auto scope = w.code().indent_scope();
        for (auto& p : p->elements) {
            if (auto a = ast::as_Expr(p)) {
                writer::TreeWriter child{w};
                write_expr(child, a, nullptr);
                child.code().writeln(";");
            }
        }
        w.code().write("}");
    }

}  // namespace c_lang
