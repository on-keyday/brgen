/*license*/
#pragma once
#include <variant>
#include "../ast.h"
#include "../../common/error.h"
#include "ident.h"

namespace brgen::ast::tool {

    enum class EResultType {
        boolean,
        integer,
        string,
        ident,
    };

    constexpr const char* eval_result_type_str[]{
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

    struct LocError {
        lexer::Loc loc;
        std::string msg;
    };

    // template <class T>
    // using result = expected<T, LocError>;

    using EResult = expected<EvalResult, LocError>;

    inline EResult add_string(const std::string& a, const std::string& b, lexer::Loc a_loc = {}, lexer::Loc b_loc = {}) {
        auto left = unescape(a);
        if (!left) {
            return unexpect(LocError{a_loc, "cannot unescape left string"});
        }
        auto right = unescape(b);
        if (!right) {
            return unexpect(LocError{b_loc, "cannot unescape right string"});
        }
        return make_result<EResultType::string>("\"" + escape(*left + *right) + "\"");
    }

    EResult compare_string(const std::string& a, const std::string& b, lexer::Loc a_loc, lexer::Loc b_loc, auto&& compare) {
        auto left = unescape(a);
        if (!left) {
            return unexpect(LocError{a_loc, "cannot unescape left string"});
        }
        auto right = unescape(b);
        if (!right) {
            return unexpect(LocError{b_loc, "cannot unescape right string"});
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
            auto left = eval_expr(bin->left);
            if (!left) {
                return left;
            }
            if (bin->op == ast::BinaryOp::logical_and) {
                if (left->type() == EResultType::boolean && !left->get<EResultType::boolean>()) {
                    return left;
                }
                auto right = eval_expr(bin->right);
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
                auto right = eval_expr(bin->right);
                if (!right) {
                    return right;
                }
                if (right->type() == EResultType::boolean && right->get<EResultType::boolean>()) {
                    return right;
                }
                return make_result<EResultType::boolean>(false);
            }
            auto right = eval_expr(bin->right);
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
                    return unexpect(LocError{bin->loc, "cannot add"});
                }
                case ast::BinaryOp::sub: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() - right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot sub"});
                }
                case ast::BinaryOp::mul: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() * right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot mul"});
                }
                case ast::BinaryOp::div: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        auto r = right->get<EResultType::integer>();
                        if (r == 0) {
                            return unexpect(LocError{bin->loc, "div by zero"});
                        }
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() / r);
                    }
                    return unexpect(LocError{bin->loc, "cannot mul"});
                }
                case ast::BinaryOp::mod: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        auto r = right->get<EResultType::integer>();
                        if (r == 0) {
                            return unexpect(LocError{bin->loc, "mod by zero"});
                        }
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() % r);
                    }
                    return unexpect(LocError{bin->loc, "cannot mul"});
                }
                case ast::BinaryOp::bit_and: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() & right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot bit_and"});
                }
                case ast::BinaryOp::bit_or: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() | right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot bit_or"});
                }
                case ast::BinaryOp::bit_xor: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() ^ right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot bit_xor"});
                }
                case ast::BinaryOp::left_logical_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(std::uint64_t(left->get<EResultType::integer>()) << right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot left_logical_shift"});
                }
                case ast::BinaryOp::right_logical_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(std::uint64_t(left->get<EResultType::integer>()) >> right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot right_logical_shift"});
                }
                case ast::BinaryOp::left_arithmetic_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() << right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot left_arithmetic_shift"});
                }
                case ast::BinaryOp::right_arithmetic_shift: {
                    if (left->type() == EResultType::integer && right->type() == EResultType::integer) {
                        return make_result<EResultType::integer>(left->get<EResultType::integer>() << right->get<EResultType::integer>());
                    }
                    return unexpect(LocError{bin->loc, "cannot left_arithmetic_shift"});
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
                    return unexpect(LocError{bin->loc, "cannot equal"});
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
                    return unexpect(LocError{bin->loc, "cannot not_equal"});
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
                    return unexpect(LocError{bin->loc, "cannot less"});
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
                    return unexpect(LocError{bin->loc, "cannot grater"});
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
                    return unexpect(LocError{bin->loc, "cannot grater"});
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
                    return unexpect(LocError{bin->loc, "cannot grater"});
                }
                default: {
                    return unexpect(LocError{bin->loc, "not supported"});
                }
            }
        }

        EResult resolve_ident(const std::shared_ptr<ast::Ident>& ident) {
            auto it = ident_map.find(ident->ident);
            if (it != ident_map.end()) {
                return it->second;
            }
            if (ident->constant_level == ast::ConstantLevel::constant) {
                auto [base, via_member] = *lookup_base(ident);
                if (auto b = ast::as<ast::Binary>(base->base.lock()); b && b->op == ast::BinaryOp::const_assign) {
                    return eval(b->right);
                }
                if (auto d = ast::as<ast::EnumMember>(base->base.lock()); d) {
                    return eval(d->value);
                }
            }
            return unexpect(LocError{ident->loc, "cannot resolve ident"});
        }

        EResult eval_expr(const std::shared_ptr<ast::Expr>& expr) {
            if (auto identity = ast::as<ast::Identity>(expr)) {
                return eval_expr(identity->expr);
            }
            if (auto b = ast::as<ast::Binary>(expr)) {
                return eval_binary(b);
            }
            if (auto c = ast::as<ast::Unary>(expr)) {
                auto res = eval_expr(c->expr);
                if (!res) {
                    return res;
                }
                switch (c->op) {
                    case ast::UnaryOp::minus_sign: {
                        if (res->type() == EResultType::integer) {
                            return make_result<EResultType::integer>(-res->get<EResultType::integer>());
                        }
                        return unexpect(LocError{c->loc, "cannot minus"});
                    }
                    case ast::UnaryOp::not_: {
                        if (res->type() == EResultType::integer) {
                            return make_result<EResultType::integer>(~std::uint64_t(res->get<EResultType::integer>()));
                        }
                        if (res->type() == EResultType::boolean) {
                            return make_result<EResultType::boolean>(!res->get<EResultType::boolean>());
                        }
                        return unexpect(LocError{c->loc, "cannot not"});
                    }
                    default: {
                        return unexpect(LocError{c->loc, "not supported"});
                    }
                }
            }
            if (auto p = ast::as<ast::Paren>(expr)) {
                return eval_expr(p->expr);
            }
            if (auto i = ast::as<ast::Ident>(expr)) {
                if (ident_mode == EvalIdentMode::resolve_ident) {
                    return resolve_ident(ast::cast_to<ast::Ident>(expr));
                }
                if (ident_mode == EvalIdentMode::no_ident) {
                    return unexpect(LocError{i->loc, "cannot use ident"});
                }
                return make_result<EResultType::ident>(i->ident);
            }
            if (auto i = ast::as<ast::IntLiteral>(expr)) {
                auto val = i->parse_as<std::uint64_t>();
                if (!val) {
                    return unexpect(LocError{i->loc, "cannot parse integer in 64 bit"});
                }
                return make_result<EResultType::integer>(*val);
            }
            if (auto i = ast::as<ast::StrLiteral>(expr)) {
                return make_result<EResultType::string>(i->value);
            }
            if (auto i = ast::as<ast::BoolLiteral>(expr)) {
                return make_result<EResultType::boolean>(i->value);
            }
            if (auto cond = ast::as<ast::Cond>(expr)) {
                auto c = eval_expr(cond->cond);
                if (!c) {
                    return c;
                }
                if (c->type() != EResultType::boolean) {
                    return unexpect(LocError{cond->loc, "cond must be boolean"});
                }
                if (c->get<EResultType::boolean>()) {
                    return eval_expr(cond->then);
                }
                else {
                    return eval_expr(cond->els);
                }
            }
            if (auto cast_ = ast::as<ast::Cast>(expr)) {
                if (cast_->arguments.size() != 1) {
                    if (ast::as<ast::IntType>(cast_->expr_type)) {
                        return make_result<EResultType::integer>(0);
                    }
                    return unexpect(LocError{cast_->loc, "cast must have 1 argument"});
                }
                return eval_expr(cast_->arguments[0]);
            }
            if (auto ch = ast::as<ast::CharLiteral>(expr)) {
                return make_result<EResultType::integer>(ch->code);
            }
            if (auto io_op = ast::as<ast::IOOperation>(expr)) {
                if (io_op->method == ast::IOMethod::config_endian_big ||
                    io_op->method == ast::IOMethod::config_bit_order_msb) {
                    return make_result<EResultType::integer>(0);
                }
                if (io_op->method == ast::IOMethod::config_endian_little ||
                    io_op->method == ast::IOMethod::config_bit_order_lsb) {
                    return make_result<EResultType::integer>(1);
                }
                if (io_op->method == ast::IOMethod::config_endian_native) {
                    return make_result<EResultType::integer>(2);
                }
            }
            return unexpect(LocError{expr->loc, "not supported"});
        }

       public:
        EResult eval(const std::shared_ptr<Node>& n) {
            if (auto expr = ast::as<Expr>(n)) {
                return eval_expr(ast::cast_to<Expr>(n));
            }
            return unexpect(LocError{n ? n->loc : lexer::Loc{}, "not an expression"});
        }

        template <EResultType t, bool unescape_str = true>
        EResult eval_as(const std::shared_ptr<Node>& n) {
            auto r = eval(n);
            if (!r) {
                return r;
            }
            if (r->type() != t) {
                return unexpect(LocError{n->loc, std::string("must be ") + eval_result_type_str[int(t)]});
            }
            if constexpr (t == EResultType::string && unescape_str) {
                auto unesc = unescape(r->get<EResultType::string>());
                if (!unesc) {
                    return unexpect(LocError{n->loc, "must be string"});
                }
                return make_result<EResultType::string>(std::move(*unesc));
            }
            else {
                return r;
            }
        }
    };

    inline std::string type_to_string(const std::shared_ptr<ast::Type>& typ) {
        if (auto t = ast::as<ast::IntType>(typ)) {
            return std::string(t->is_signed ? "s" : "u") +
                   (t->endian == Endian::big      ? "b"
                    : t->endian == Endian::little ? "l"
                                                  : "") +
                   nums(*t->bit_size);
        }
        if (auto a = ast::as<ast::ArrayType>(typ)) {
            if (a->length_value) {
                return concat("[", nums(*a->length_value), "]", type_to_string(a->element_type), "");
            }
            return concat("[]", type_to_string(a->element_type), "");
        }
        if (auto fn = ast::as<ast::FunctionType>(typ)) {
            std::string args;
            for (auto& arg : fn->parameters) {
                if (!args.empty()) {
                    args += ", ";
                }
                args += type_to_string(arg);
            }
            return concat("fn (", args, ") -> ", type_to_string(fn->return_type));
        }
        if (auto s = ast::as<ast::IdentType>(typ)) {
            return s->ident->ident;
        }
        if (auto enum_type = ast::as<ast::EnumType>(typ)) {
            auto enum_ = enum_type->base.lock();
            if (enum_->base_type) {
                return enum_->ident->ident + "(" + type_to_string(enum_->base_type) + ")";
            }
            return enum_->ident->ident;
        }
        if (auto struct_type = ast::as<ast::StructType>(typ)) {
            if (auto member = ast::as<ast::Member>(struct_type->base.lock())) {
                if (auto fmt = ast::as<ast::Format>(member)) {
                    std::string cast_to;
                    for (auto& c : fmt->cast_fns) {
                        if (auto fn = c.lock()) {
                            if (!cast_to.empty()) {
                                cast_to += ",";
                            }
                            cast_to += type_to_string(fn->return_type);
                        }
                    }
                    if (cast_to.size()) {
                        return fmt->ident->ident + " cast(" + cast_to + ")";
                    }
                }
                return member->ident->ident;
            }
            return "(anonymous struct at " + nums(struct_type->loc.line) + ":" + nums(struct_type->loc.col) + ")";
        }
        if (auto struct_union_type = ast::as<ast::StructUnionType>(typ)) {
            return "(anonymous union of structs at " + nums(struct_union_type->loc.line) + ":" + nums(struct_union_type->loc.col) + ")";
        }
        if (auto union_type = ast::as<ast::UnionType>(typ)) {
            auto s = "(anonymous union at " + nums(union_type->loc.line) + ":" + nums(union_type->loc.col);
            if (union_type->common_type) {
                s += " with common type " + type_to_string(union_type->common_type);
            }
            s += ")";
            return s;
        }
        if (auto range = ast::as<ast::RangeType>(typ)) {
            if (!range->base_type) {
                return "range<any>";
            }
            return concat("range_", range->range.lock()->op == ast::BinaryOp::range_inclusive ? "inclusive" : "exclusive",
                          "<", type_to_string(range->base_type), ">");
        }
        if (auto bool_type = ast::as<ast::BoolType>(typ)) {
            return "bool";
        }
        if (auto void_type = ast::as<ast::VoidType>(typ)) {
            return "void";
        }
        if (auto int_literal = ast::as<ast::IntLiteralType>(typ)) {
            return "(int literal at " + nums(int_literal->loc.line) + ":" + nums(int_literal->loc.col) + " size " + nums(*int_literal->bit_size) + ")";
        }
        if (auto str_literal = ast::as<ast::StrLiteralType>(typ)) {
            return "(string literal at " + nums(str_literal->loc.line) + ":" + nums(str_literal->loc.col) + ")";
        }
        if (auto float_type = ast::as<ast::FloatType>(typ)) {
            return "f" +
                   std::string(float_type->endian == Endian::big      ? "b"
                               : float_type->endian == Endian::little ? "l"
                                                                      : "") +
                   nums(*float_type->bit_size);
        }
        return "(unknown type)";
    }

}  // namespace brgen::ast::tool
