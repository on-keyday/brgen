/*license*/
#pragma once
#include <string>
#include "../ast.h"
#include "../traverse.h"

namespace brgen::ast::tool {

    struct Stringer {
        std::string buffer;
        std::unordered_map<BinaryOp, std::string> bin_op_map;

        void write(auto&&... args) {
            utils::strutil::appends(buffer, args...);
        }

        void write_bin_op(BinaryOp op) {
            if (auto it = bin_op_map.find(op); it != bin_op_map.end()) {
                write(it->second);
            }
            else {
                write(*bin_op_str(op));
            }
        }

        void to_string(const std::shared_ptr<Expr>& expr) {
            if (!expr) {
                return;
            }
            if (auto e = ast::as<ast::Binary>(expr)) {
                write("(");
                to_string(e->left);
                write(" ");
                write_bin_op(e->op);
                write(" ");
                to_string(e->right);
                write(")");
            }
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                write(lit->value);
            }
            if (auto id = ast::as<ast::Ident>(expr)) {
                write(id->ident);
            }
        }
    };
}  // namespace brgen::ast::tool
