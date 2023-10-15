/*license*/
#pragma once
#include <string>
#include "../ast.h"
#include "../traverse.h"
#include <functional>

namespace brgen::ast::tool {

    struct Stringer {
        std::unordered_map<BinaryOp, std::function<std::string(std::string, std::string)>> bin_op_map;
        std::unordered_map<size_t, std::string> tmp_var_map;
        std::unordered_map<std::string, std::string> ident_map;

       private:
        std::string handle_bin_op(BinaryOp op, std::string& left, std::string& right) {
            if (auto it = bin_op_map.find(op); it != bin_op_map.end()) {
                return it->second(std::move(left), std::move(right));
            }
            else {
                return concat("(", left, " ", *bin_op_str(op), " ", right, ")");
            }
        }

       public:
        std::string to_string(const std::shared_ptr<Expr>& expr) {
            if (!expr) {
                return "";
            }
            if (auto e = ast::as<ast::Binary>(expr)) {
                auto left = to_string(e->left);
                auto right = to_string(e->right);
            }
            if (auto lit = ast::as<ast::IntLiteral>(expr)) {
                return lit->value;
            }
            if (auto id = ast::as<ast::Ident>(expr)) {
                if (auto found = ident_map.find(id->ident); found != ident_map.end()) {
                    return found->second;
                }
                else {
                    return id->ident;
                }
            }
            if (auto tmp = ast::as<ast::TmpVar>(expr)) {
                if (auto found = tmp_var_map.find(tmp->tmp_var); found != tmp_var_map.end()) {
                    return found->second;
                }
                else {
                    return concat("tmp", nums(tmp->tmp_var));
                }
            }
            return "";
        }
    };
}  // namespace brgen::ast::tool
