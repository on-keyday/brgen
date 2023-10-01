/*license*/
#pragma once
#include <variant>
#include "../ast.h"
#include "../../common/error.h"

namespace brgen::ast::tool {

    enum class EResultType {
        boolean,
        integer,
        string,
        ident,
    };

    constexpr const char* eresult_type_str[]{
        "boolean",
        "integer",
        "string",
        "ident",
        nullptr,
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

        template <EResultType t>
        const auto& get() const {
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

    EResult compare_string(const std::string& a, const std::string& b, lexer::Loc a_loc, lexer::Loc b_loc, auto&& compare) {
        auto left = unescape(a);
        if (!left) {
            return unexpect(EvalError{a_loc, "cannot unescape left string"});
        }
        auto right = unescape(b);
        if (!right) {
            return unexpect(EvalError{b_loc, "cannot unescape right string"});
        }
        return make_result<EResultType::boolean>(compare(*left, *right));
    }

    enum class EvalIdentMode {
        resolve_ident,
        raw_ident,
        no_ident,
    };

    struct Evaluator {
        EvalIdentMode ident_mode = EvalIdentMode::raw_ident;
        std::map<std::string, EResult> ident_map;

       private:
        EResult eval_binary(ast::Binary* bin) {
            auto left = eval_expr(bin->left.get());
            if (!left) {
                return left;
            }
            if (bin->op == ast::BinaryOp::logical_and) {
                if (left->type() == EResultType::boolean && !left->get<EResultType::boolean>()) {
                    return left;
                }
                auto right = eval_expr(bin->right.get());
                if (!right) {
                    return right;
                }
                if (right->type() == EResultType::boolean && !right->get<EResultType::boolean>()) {
                    return right;
                }
                return make_result<EResultType::boolean>(true);
            }
            else if (bin->op == ast::BinaryOp::logical_or) {
                if (left->type() == EResultType::boolean && left->get<EResultType::boolean>()) {
                    return left;
                }
                auto right = eval_expr(bin->right.get());
                if (!right) {
                    return right;
                }
                if (right->type() == EResultType::boolean && right->get<EResultType::boolean>()) {
                    return right;
                }
                return make_result<EResultType::boolean>(false);
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
                        return make_result<EResultType::integer>(std::uint64_t(left->get<EResultType::integer>()) << right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot left_logical_shift"});
                }
                case ast::BinaryOp::right_logical_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(std::uint64_t(left->get<EResultType::integer>()) >> right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot right_logical_shift"});
                }
                case ast::BinaryOp::left_arithmetic_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() << right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot left_arithmetic_shift"});
                }
                case ast::BinaryOp::right_arithmetic_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() << right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot left_arithmetic_shift"});
                }
                case ast::BinaryOp::equal: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return compare_string(left->get<EResultType::string>(), right->get<EResultType::string>(),
                                              bin->left->loc, bin->right->loc,
                                              [](auto&& a, auto&& b) { return a == b; });
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::boolean>(left->get<EResultType::integer>() == right->get<EResultType::integer>());
                    }
                    if (left->type() == EResultType::boolean && right->type() == EResultType::boolean) {
                        return make_result<EResultType::boolean>(left->get<EResultType::boolean>() == right->get<EResultType::boolean>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot equal"});
                }
                case ast::BinaryOp::not_equal: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return compare_string(left->get<EResultType::string>(), right->get<EResultType::string>(),
                                              bin->left->loc, bin->right->loc,
                                              [](auto&& a, auto&& b) { return a != b; });
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::boolean>(left->get<EResultType::integer>() != right->get<EResultType::integer>());
                    }
                    if (left->type() == EResultType::boolean && right->type() == EResultType::boolean) {
                        return make_result<EResultType::boolean>(left->get<EResultType::boolean>() != right->get<EResultType::boolean>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot not_equal"});
                }
                case ast::BinaryOp::less: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return compare_string(left->get<EResultType::string>(), right->get<EResultType::string>(),
                                              bin->left->loc, bin->right->loc,
                                              [](auto&& a, auto&& b) { return a < b; });
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::boolean>(left->get<EResultType::integer>() < right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot less"});
                }
                case ast::BinaryOp::less_or_eq: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return compare_string(left->get<EResultType::string>(), right->get<EResultType::string>(),
                                              bin->left->loc, bin->right->loc,
                                              [](auto&& a, auto&& b) { return a <= b; });
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::boolean>(left->get<EResultType::integer>() <= right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot grater"});
                }
                case ast::BinaryOp::grater: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return compare_string(left->get<EResultType::string>(), right->get<EResultType::string>(),
                                              bin->left->loc, bin->right->loc,
                                              [](auto&& a, auto&& b) { return a > b; });
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::boolean>(left->get<EResultType::integer>() > right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot grater"});
                }
                case ast::BinaryOp::grater_or_eq: {
                    if (left->type() == EResultType::string && right->type() == EResultType::string) {
                        return compare_string(left->get<EResultType::string>(), right->get<EResultType::string>(),
                                              bin->left->loc, bin->right->loc,
                                              [](auto&& a, auto&& b) { return a >= b; });
                    }
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::boolean>(left->get<EResultType::integer>() >= right->get<EResultType::integer>());
                    }
                    return unexpect(EvalError{bin->loc, "cannot grater"});
                }
                default: {
                    return unexpect(EvalError{bin->loc, "not supported"});
                }
            }
        }

        EResult eval_expr(ast::Expr* expr) {
            if (auto b = ast::as<ast::Binary>(expr)) {
                return eval_binary(b);
            }
            if (auto c = ast::as<ast::Unary>(expr)) {
                auto res = eval_expr(c->expr.get());
                if (!res) {
                    return res;
                }
                switch (c->op) {
                    case ast::UnaryOp::minus_sign: {
                        if (res->type() == EResultType::integer) {
                            return make_result<EResultType::integer>(-res->get<EResultType::integer>());
                        }
                        return unexpect(EvalError{c->loc, "cannot minus"});
                    }
                    case ast::UnaryOp::not_: {
                        if (res->type() == EResultType::integer) {
                            return make_result<EResultType::integer>(~std::uint64_t(res->get<EResultType::integer>()));
                        }
                        if (res->type() == EResultType::boolean) {
                            return make_result<EResultType::boolean>(!res->get<EResultType::boolean>());
                        }
                        return unexpect(EvalError{c->loc, "cannot not"});
                    }
                    default: {
                        return unexpect(EvalError{c->loc, "not supported"});
                    }
                }
            }
            if (auto p = ast::as<ast::Paren>(expr)) {
                return eval_expr(p->expr.get());
            }
            if (auto i = ast::as<ast::Ident>(expr)) {
                if (ident_mode == EvalIdentMode::resolve_ident) {
                    auto it = ident_map.find(i->ident);
                    if (it == ident_map.end()) {
                        return unexpect(EvalError{i->loc, "cannot resolve ident"});
                    }
                    return it->second;
                }
                if (ident_mode == EvalIdentMode::no_ident) {
                    return unexpect(EvalError{i->loc, "cannot use ident"});
                }
                return make_result<EResultType::ident>(i->ident);
            }
            if (auto i = ast::as<ast::IntLiteral>(expr)) {
                auto val = i->parse_as<std::uint64_t>();
                if (!val) {
                    return unexpect(EvalError{i->loc, "cannot parse integer in 64 bit"});
                }
                return make_result<EResultType::integer>(*val);
            }
            if (auto i = ast::as<ast::StrLiteral>(expr)) {
                return make_result<EResultType::string>(i->value);
            }
            if (auto i = ast::as<ast::BoolLiteral>(expr)) {
                return make_result<EResultType::boolean>(i->value);
            }
            return unexpect(EvalError{expr->loc, "not supported"});
        }

       public:
        EResult eval(const std::shared_ptr<Node>& n) {
            if (auto expr = ast::as<Expr>(n)) {
                return eval_expr(expr);
            }
            return unexpect(EvalError{n ? n->loc : lexer::Loc{}, "not an expression"});
        }

        template <EResultType t, bool unescape_str = true>
        EResult eval_as(const std::shared_ptr<Node>& n) {
            auto r = eval(n);
            if (!r) {
                return r;
            }
            if (r->type() != t) {
                return unexpect(EvalError{n->loc, std::string("must be ") + eresult_type_str[int(t)]});
            }
            if constexpr (t == EResultType::string && unescape_str) {
                auto unesc = unescape(r->get<EResultType::string>());
                if (!unesc) {
                    return unexpect(EvalError{n->loc, "must be string"});
                }
                return make_result<EResultType::string>(std::move(*unesc));
            }
            else {
                return r;
            }
        }
    };

    constexpr auto to_loc_error() {
        return [](auto&& err) {
            return error(err.loc, err.msg);
        };
    }
}  // namespace brgen::ast::tool