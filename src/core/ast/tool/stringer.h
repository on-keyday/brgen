/*license*/
#pragma once
#include <string>
#include "../ast.h"
#include "../traverse.h"
#include <functional>
#include <unordered_map>

namespace brgen::ast::tool {

    void extract_ident(const std::shared_ptr<Expr>& expr, auto&& add_ident) {
        if (auto ident = ast::as<ast::Ident>(expr)) {
            add_ident(ast::cast_to<ast::Ident>(expr));
            return;
        }
        auto lookup = [&](auto&& f, const std::shared_ptr<Node>& v) -> void {
            if (ast::as<ast::Type>(v)) {
                return;
            }
            if (ast::as<ast::Ident>(v)) {
                add_ident(ast::cast_to<ast::Ident>(v));
                return;
            }
            traverse(v, [&](auto&& v) {
                f(f, v);
            });
        };
        traverse(expr, [&](auto&& v) -> void {
            lookup(lookup, v);
        });
    }

    struct Stringer {
        std::unordered_map<BinaryOp, std::function<std::string(Stringer& s, const std::shared_ptr<Expr>& left, const std::shared_ptr<Expr>& right)>> bin_op_map;
        std::unordered_map<UnaryOp, std::function<std::string(Stringer& s, const std::shared_ptr<Expr>&)>> unary_op_map;
        std::unordered_map<size_t, std::string> tmp_var_map;
        std::unordered_map<std::string, std::string> ident_map;
        std::function<std::string(std::string cond, std::string then, std::string els)> cond_op;
        std::unordered_map<std::string, std::function<std::string(Stringer& s, const std::string& name, const std::vector<std::shared_ptr<ast::Expr>>&)>> call_handler;
        std::function<std::string(Stringer&, const std::shared_ptr<Type>&)> type_resolver;

        void clear() {
            bin_op_map.clear();
            tmp_var_map.clear();
            ident_map.clear();
            cond_op = nullptr;
            call_handler.clear();
            type_resolver = nullptr;
        }

       private:
        std::string handle_bin_op(const std::shared_ptr<Binary>& bin) {
            auto op = bin->op;
            if (auto it = bin_op_map.find(op); it != bin_op_map.end()) {
                return it->second(*this, bin->left, bin->right);
            }
            else {
                auto left = to_string(bin->left);
                auto right = to_string(bin->right);
                return concat("(", left, " ", *bin_op_str(op), " ", right, ")");
            }
        }

        std::string handle_call_op(const std::shared_ptr<Call>& c) {
            auto callee = to_string(c->callee);
            if (auto found = call_handler.find(callee); found != call_handler.end()) {
                return found->second(*this, callee, c->arguments);
            }
            else {
                std::string args;
                for (auto&& arg : c->arguments) {
                    if (!args.empty()) {
                        args += ", ";
                    }
                    args += to_string(arg);
                }
                return concat(callee, "(", args, ")");
            }
        }

       public:
        std::string to_string(const std::shared_ptr<Expr>& expr) {
            if (!expr) {
                return "";
            }
            if (auto e = ast::as<ast::Binary>(expr)) {
                return handle_bin_op(ast::cast_to<ast::Binary>(expr));
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
            if (auto c = ast::as<ast::Cond>(expr)) {
                if (cond_op) {
                    return cond_op(to_string(c->cond), to_string(c->then), to_string(c->els));
                }
                else {
                    return concat("(", to_string(c->cond), " ? ", to_string(c->then), " : ", to_string(c->els), ")");
                }
            }
            if (auto c = ast::as<ast::Call>(expr)) {
                return handle_call_op(ast::cast_to<ast::Call>(expr));
            }
            if (auto d = ast::as<ast::Unary>(expr)) {
                return concat(unary_op_str[int(d->op)], to_string(d->expr));
            }
            if (auto a = ast::as<ast::Available>(expr)) {
                auto base_ident = ast::as<ast::Ident>(a->target->base.lock());
                if (!base_ident) {
                    return "false";
                }
                if (auto field = ast::as<ast::Field>(base_ident->base.lock())) {
                    if (auto typ = ast::as<ast::UnionType>(field->field_type)) {
                        std::string cond_s = "";
                        if (auto c = typ->cond.lock()) {
                            cond_s = to_string(c);
                        }
                        else {
                            cond_s = "true";
                        }
                        std::string result;
                        for (auto& c : typ->candidates) {
                            auto expr = c->cond.lock();
                            auto s = to_string(expr);
                            result += brgen::concat("(", s, "==", cond_s, ") ? ");
                            if (c->field.lock()) {
                                result += "true";
                            }
                            else {
                                result += "false";
                            }
                            result += " : ";
                        }
                        result += "false";
                        return brgen::concat("(", result, ")");
                    }
                    return "true";
                }
            }
            if (auto cast_ = ast::as<ast::Cast>(expr)) {
                return concat(type_resolver(*this, cast_->expr_type), "(", to_string(cast_->expr), ")");
            }
            if (auto access = ast::as<ast::MemberAccess>(expr)) {
                auto base = to_string(access->target);
                auto field = access->member->ident;
                return concat(base, ".", field);
            }
            return "";
        }

        std::vector<std::shared_ptr<ast::Binary>> collect_defined_ident(const std::shared_ptr<ast::Expr>& expr) {
            std::vector<std::shared_ptr<ast::Binary>> defs;
            auto f = [&](auto&& f, auto&& v) -> void {
                ast::traverse(v, [&](auto&& v) -> void {
                    f(f, v);
                });
                if (auto ident = ast::as<ast::Ident>(v)) {
                    auto base = ast::as<ast::Ident>(ident->base.lock());
                    if (!base) {
                        return;
                    }
                    auto base_base = base->base.lock();
                    if (auto bin = ast::as<ast::Binary>(base_base)) {
                        if (bin->op == BinaryOp::define_assign ||
                            bin->op == BinaryOp::const_assign) {
                            defs.push_back(ast::cast_to<ast::Binary>(base_base));
                        }
                    }
                }
            };
            ast::traverse(expr, [&](auto&& v) -> void {
                f(f, v);
            });
            return defs;
        }
    };
}  // namespace brgen::ast::tool
