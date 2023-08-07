/*license*/
#include "core/writer/writer.h"
#include "core/ast/ast.h"

namespace c_lang {

    void write_expr(writer::TreeWriter& w, ast::Expr* expr) {
        if (auto b = ast::as<ast::Binary>(expr)) {
            auto op = b->op;
            bool paren = op == ast::BinaryOp::left_shift || op == ast::BinaryOp::right_shift;
            if (paren) {
                w.code().write("(");
            }
            write_expr(w, b->left.get());
            w.code().write(" ", ast::bin_op_str(op), " ");
            write_expr(w, b->right.get());
            if (paren) {
                w.code().write(")");
            }
        }
    }

    void entry(writer::TreeWriter& w, std::shared_ptr<ast::Program>& p) {
        w.code().write("int main() {");
        auto scope = w.code().indent_scope();
        for (auto& p : p->program) {
            if (auto a = ast::as_Expr(p)) {
                writer::TreeWriter child{w};
                write_expr(child, a);
            }
        }
        w.code().write("}");
    }

}  // namespace c_lang
