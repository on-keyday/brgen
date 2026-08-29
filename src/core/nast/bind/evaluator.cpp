/*license*/
#include "evaluator.hpp"

#include "../node/traverse.h"

#include <number/prefix.h>
#include <optional>

namespace brgen::nast::bind {

    namespace {

        using I128 = __int128;
        using U128 = unsigned __int128;

        constexpr std::uint64_t U64_MAX = ~std::uint64_t(0);

        // 整数の中間値。std::optional<__int128> に入れると、格納域が 16 byte
        // 境界に来ない配置になり、-O0 の比較 (movaps) がアライメント違反で
        // 落ちることがある (実測)。アラインの保証される平の構造体で持つ。
        struct IntVal {
            bool ok = false;
            I128 v = 0;
        };

        IntVal as_int(const std::optional<ConstantValue>& v) {
            if (!v || v->kind != EvalKind::integer) {
                return {};
            }
            auto x = I128(v->integer);
            return {true, v->is_negative ? -x : x};
        }

        std::optional<bool> as_bool(const std::optional<ConstantValue>& v) {
            if (!v || v->kind != EvalKind::boolean) {
                return std::nullopt;
            }
            return v->boolean;
        }

        std::optional<ConstantValue> make_int(I128 x) {
            ConstantValue v;
            v.kind = EvalKind::integer;
            if (x < 0) {
                v.is_negative = true;
                x = -x;
            }
            if (U128(x) > U128(U64_MAX)) {
                // 絶対値が u64 に収まらない。畳まない。
                return std::nullopt;
            }
            v.integer = std::uint64_t(x);
            return v;
        }

        ConstantValue make_bool(bool b) {
            ConstantValue v;
            v.kind = EvalKind::boolean;
            v.boolean = b;
            return v;
        }

    }  // namespace

    // 表の実体は伸びる vector なので、再帰の途中で挿入が起きると get の指す先が
    // 動く。実装の中は値で受け渡し、pointer を持ち越さない。
    std::optional<ConstantValue> Evaluator::value_of(Node<Expr> e) {
        if (auto* p = eval(e)) {
            return *p;
        }
        return std::nullopt;
    }

    std::optional<ConstantValue> Evaluator::value_of_decl(Node<Statement> decl) {
        if (auto vd = decl.as_any<VariableDefinition>()) {
            auto d = vd.ref(a);
            // := は後から代入され得るので ::= だけ。in_assign もここには来ない
            // (来ても value は container で、束縛の値ではない)。
            if (d->op == BinaryOp::const_assign) {
                return value_of(d->value);
            }
            return std::nullopt;
        }
        if (auto em = decl.as_any<EnumMember>()) {
            return value_of(em.ref(a)->value);
        }
        return std::nullopt;
    }

    std::optional<ConstantValue> Evaluator::compute_binary(Node<Binary> bin) {
        auto d = bin.ref(a);
        auto l = value_of(d->left);
        auto r = value_of(d->right);
        if (!l || !r) {
            return std::nullopt;
        }
        auto li = as_int(l);
        auto ri = as_int(r);
        auto lb = as_bool(l);
        auto rb = as_bool(r);
        auto both_int = li.ok && ri.ok;
        switch (d->op) {
            case BinaryOp::add:
            case BinaryOp::sub:
            case BinaryOp::mul: {
                if (!both_int) {
                    return std::nullopt;
                }
                I128 res = 0;
                bool overflow = d->op == BinaryOp::add   ? __builtin_add_overflow(li.v, ri.v, &res)
                                : d->op == BinaryOp::sub ? __builtin_sub_overflow(li.v, ri.v, &res)
                                                         : __builtin_mul_overflow(li.v, ri.v, &res);
                return overflow ? std::nullopt : make_int(res);
            }
            case BinaryOp::div:
            case BinaryOp::mod: {
                if (!both_int || ri.v == 0) {
                    return std::nullopt;
                }
                // 128bit の除算は Windows だと compiler-rt (__divti3) が要るので、
                // 絶対値を u64 で割って符号を後から付ける。C の切り捨てと同じ。
                bool lneg = li.v < 0;
                bool rneg = ri.v < 0;
                auto lm = std::uint64_t(lneg ? -li.v : li.v);
                auto rm = std::uint64_t(rneg ? -ri.v : ri.v);
                if (d->op == BinaryOp::div) {
                    auto q = I128(lm / rm);
                    return make_int(lneg != rneg ? -q : q);
                }
                auto m = I128(lm % rm);
                return make_int(lneg ? -m : m);
            }
            case BinaryOp::left_logical_shift:
            case BinaryOp::left_arithmetic_shift: {
                if (!both_int || li.v < 0 || ri.v < 0 || ri.v > 63) {
                    return std::nullopt;
                }
                auto shifted = U128(li.v) << int(ri.v);
                if (shifted > U128(U64_MAX)) {
                    return std::nullopt;
                }
                return make_int(I128(shifted));
            }
            case BinaryOp::right_logical_shift:
            case BinaryOp::right_arithmetic_shift: {
                // 負の値の右シフトは論理と算術で答えが割れるので畳まない。
                if (!both_int || li.v < 0 || ri.v < 0 || ri.v > 63) {
                    return std::nullopt;
                }
                return make_int(li.v >> int(ri.v));
            }
            case BinaryOp::bit_and:
            case BinaryOp::bit_or:
            case BinaryOp::bit_xor: {
                if (!both_int || li.v < 0 || ri.v < 0) {
                    return std::nullopt;
                }
                auto lu = std::uint64_t(li.v);
                auto ru = std::uint64_t(ri.v);
                auto res = d->op == BinaryOp::bit_and  ? (lu & ru)
                           : d->op == BinaryOp::bit_or ? (lu | ru)
                                                       : (lu ^ ru);
                return make_int(I128(res));
            }
            case BinaryOp::equal:
            case BinaryOp::not_equal: {
                std::optional<bool> eq;
                if (both_int) {
                    eq = li.v == ri.v;
                }
                else if (lb && rb) {
                    eq = *lb == *rb;
                }
                else if (l->kind == EvalKind::string && r->kind == EvalKind::string) {
                    eq = l->string == r->string;
                }
                if (!eq) {
                    return std::nullopt;
                }
                return make_bool(d->op == BinaryOp::equal ? *eq : !*eq);
            }
            case BinaryOp::less:
                return both_int ? std::optional(make_bool(li.v < ri.v)) : std::nullopt;
            case BinaryOp::less_or_eq:
                return both_int ? std::optional(make_bool(li.v <= ri.v)) : std::nullopt;
            case BinaryOp::grater:
                return both_int ? std::optional(make_bool(li.v > ri.v)) : std::nullopt;
            case BinaryOp::grater_or_eq:
                return both_int ? std::optional(make_bool(li.v >= ri.v)) : std::nullopt;
            case BinaryOp::logical_and:
                return (lb && rb) ? std::optional(make_bool(*lb && *rb)) : std::nullopt;
            case BinaryOp::logical_or:
                return (lb && rb) ? std::optional(make_bool(*lb || *rb)) : std::nullopt;
            case BinaryOp::comma:
                // 値としては右。enum の (値, 表示名) の組は parse が既に
                // 割っているので、ここに来る comma は式としての comma だけ。
                return r;
            default:
                return std::nullopt;
        }
    }

    std::optional<ConstantValue> Evaluator::compute(Node<Expr> e) {
        if (auto lit = e.as_any<IntLiteral>()) {
            std::uint64_t value = 0;
            if (!futils::number::prefix_integer(lit.ref(a)->value, value)) {
                return std::nullopt;
            }
            ConstantValue v;
            v.kind = EvalKind::integer;
            v.integer = value;
            return v;
        }
        if (auto lit = e.as_any<BoolLiteral>()) {
            return make_bool(lit.ref(a)->value);
        }
        if (auto lit = e.as_any<CharLiteral>()) {
            ConstantValue v;
            v.kind = EvalKind::integer;
            v.integer = lit.ref(a)->code;
            return v;
        }
        if (auto lit = e.as_any<StrLiteral>()) {
            ConstantValue v;
            v.kind = EvalKind::string;
            // binary_value (base64) のまま持つ。復号すると生の制御バイトが
            // 混ざり、as_json の出力が JSON として壊れる (実測)。比較は base64
            // 同士で一貫し、人間向けの復号は表示側の仕事。
            v.string = lit.ref(a)->binary_value;
            return v;
        }
        if (auto p = e.as_any<Paren>()) {
            return value_of(p.ref(a)->expr);
        }
        if (auto id = e.as_any<Identity>()) {
            return value_of(id.ref(a)->expr);
        }
        if (auto un = e.as_any<Unary>()) {
            auto d = un.ref(a);
            if (d->op == UnaryOp::minus_sign) {
                auto x = as_int(value_of(d->target));
                return x.ok ? make_int(-x.v) : std::nullopt;
            }
            auto b = as_bool(value_of(d->target));
            return b ? std::optional(make_bool(!*b)) : std::nullopt;
        }
        if (auto cnd = e.as_any<Cond>()) {
            auto d = cnd.ref(a);
            auto c = as_bool(value_of(d->cond));
            if (!c) {
                return std::nullopt;
            }
            return value_of(*c ? d->then : d->els);
        }
        if (auto ref = e.as_any<Reference>()) {
            if (auto* r = tables.table<Resolution>().get(ref.ref(a)->name)) {
                return value_of_decl(r->target);
            }
            return std::nullopt;
        }
        if (auto ma = e.as_any<MemberAccess>()) {
            if (auto* r = tables.table<Resolution>().get(ma.ref(a)->member)) {
                return value_of_decl(r->target);
            }
            return std::nullopt;
        }
        if (auto bin = e.as_any<Binary>()) {
            return compute_binary(bin);
        }
        return std::nullopt;
    }

    const ConstantValue* Evaluator::eval(Node<Expr> e) {
        if (!e) {
            return nullptr;
        }
        if (auto* v = tables.table<ConstantValue>().get(e)) {
            return v;
        }
        if (!in_progress_.insert(e.id()).second) {
            return nullptr;
        }
        auto result = compute(e);
        in_progress_.erase(e.id());
        if (!result) {
            return nullptr;
        }
        evaluated++;
        tables.table<ConstantValue>().set(e, std::move(*result));
        return tables.table<ConstantValue>().get(e);
    }

    void Evaluator::run(Node<Module> mod) {
        visit_all(a, mod, [&](NodeAny n) {
            if (auto e = n.as_any<Expr>()) {
                eval(e);
            }
            return true;
        });
    }

}  // namespace brgen::nast::bind
