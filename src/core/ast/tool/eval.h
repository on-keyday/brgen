/*license*/
#pragma once
#include <variant>
#include "../ast.h"

namespace brgen::ast::tool {

    enum class EResultType {
        boolean,
        integer,
        string,
        ident,
    };

    struct EvalResult {
       private:
        std::variant<bool, std::int64_t, std::string, std::string> value;

       public:
        EResultType type() const {
            return static_cast<EResultType>(value.index());
        }

        template <EResultType t>
        void emplace(auto&&... args) {
            value.template emplace<int(t)>(std::forward<decltype(args)>(args)...);
        }

        template <EResultType t>
        auto& get() {
            return std::get<int(t)>(value);
        }
    };

    template <EResultType t>
    EvalResult make_result(auto&&... args) {
        EvalResult r;
        r.emplace<t>(std::forward<decltype(args)>(args)...);
        return r;
    }

    struct EvalError {
        lexer::Loc loc;
        std::string msg;
    };

    using EResult = expected<EvalResult, EvalError>;

    EResult add_string(const std::string& a, const std::string& b, lexer::Loc a_loc = {}, lexer::Loc b_loc = {}) {
        auto left = unescape(a);
        if (!left) {
            return unexpect(EvalError{a_loc, "cannot unescape left string"});
        }
        auto right = unescape(b);
        if (!right) {
            return unexpect(EvalError{b_loc, "cannot unescape right string"});
        }
        return make_result<EResultType::string>("\"" + escape(*left + *right) + "\"");
    }

    struct Evaluator {
       private:
        EResult eval_binary(ast::Binary* bin) {
            auto left = eval_expr(bin->left.get());
            if (!left) {
                return left;
            }
            auto right = eval_expr(bin->right.get());
            if (!right) {
                return right;
            }
            switch (bin->op) {
                case ast::BinaryOp::add: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return add_string(left->get<EResultType::string>(), right->get<EResultType::string>(), bin->left->loc, bin->right->loc);
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() + right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot add"});
                }
                case ast::BinaryOp::sub: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() - right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot sub"});
                }
                case ast::BinaryOp::mul: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() * right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot mul"});
                }
                case ast::BinaryOp::div: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        auto r = right->get<EResultType::integer>();
                        if (r == 0) {
                            return unexpect(EvalError{bin->loc, "div by zero"});
                        }
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() / r);
                    }
                    return unexpect(EvalError{bin->loc, "cannot mul"});
                }
                case ast::BinaryOp::mod: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        auto r = right->get<EResultType::integer>();
                        if (r == 0) {
                            return unexpect(EvalError{bin->loc, "mod by zero"});
                        }
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() % r);
                    }
                    return unexpect(EvalError{bin->loc, "cannot mul"});
                }
                case ast::BinaryOp::bit_and: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() & right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot bit_and"});
                }
                case ast::BinaryOp::bit_or: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() | right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot bit_or"});
                }
                case ast::BinaryOp::bit_xor: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() ^ right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot bit_xor"});
                }
                case ast::BinaryOp::left_logical_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() << right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot left_logical_shift"});
                }
            }
        }

        EResult eval_expr(ast::Expr* expr) {
            if (auto b = ast::as<ast::Binary>(expr)) {
                return eval_binary(b);
            }
            if (auto c = ast::as<ast::Unary>(expr)) {
            }
        }

       public:
        EResult eval(const std::shared_ptr<Node>& n) {
            if (auto expr = ast::as<Expr>(n)) {
                return eval_expr(expr);
            }
            return unexpect(EvalError{n->loc, "not an expression"});
        }
    };

}  // namespace brgen::ast::tool