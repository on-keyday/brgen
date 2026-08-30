/*license*/
#include "predicate.hpp"
#include "../node/build.h"
#include "../node/util.h"

namespace brgen::nast::lowering {

    // 綴り grater_or_eq は元実装から引き継いだもの。直すと生成物と元 AST に波及する。

    Node<Expr> branch_predicate(Context& c, Node<Expr> subject, Node<Expr> pattern) {
        auto& a = c.a;
        if (!pattern) {
            return nullref;
        }
        Builder b{a, pattern.ref(a).loc()};
        auto bool_type = b.bool_type();

        if (!subject) {
            return pattern;  // 条件なし match。パターンがそのまま述語。
        }

        // `1,2 =>` は OrCond。葉ごとに述語を作って || で繋ぐ。葉が範囲でも
        // よいので、再帰で組む。
        if (auto orc = pattern.as_any<OrCond>()) {
            Node<Expr> acc;
            for (auto& leaf : orc.ref(a)->conds) {
                auto one = branch_predicate(c, subject, leaf);
                acc = b.join(BinaryOp::logical_or, acc, one, bool_type);
            }
            return acc;
        }

        if (auto range = pattern.as_any<Range>()) {
            auto d = range.ref(a);
            if (!d->start && !d->end) {
                return nullref;  // `..` = 既定。述語にならない (呼ぶ側が else にする)
            }
            Node<Expr> acc;
            if (d->start) {
                acc = b.join(BinaryOp::logical_and, acc, b.bin(BinaryOp::grater_or_eq, subject, d->start, bool_type), bool_type);
            }
            if (d->end) {
                // `..` は右開き、`..=` は右閉じ。
                auto op = d->op == BinaryOp::range_inclusive ? BinaryOp::less_or_eq : BinaryOp::less;
                acc = b.join(BinaryOp::logical_and, acc, b.bin(op, subject, d->end, bool_type), bool_type);
            }
            return acc;
        }

        return b.bin(BinaryOp::equal, subject, pattern, bool_type);
    }

    Node<Expr> lower_range_compare(Context& c, Node<Binary> cmp) {
        auto& a = c.a;
        if (!cmp) {
            return nullref;
        }
        if (auto* got = c.tables.table<LoweredRangeCompare>().get(cmp)) {
            return got->expr;
        }
        auto d = cmp.ref(a);
        if (d->op != BinaryOp::equal && d->op != BinaryOp::not_equal) {
            return nullref;
        }
        Builder b{a, cmp.ref(a).loc()};
        auto bool_type = b.bool_type();
        // 範囲はどちらの側にも書ける。括弧は綴りの都合なので剥がす。
        auto lhs = strip_paren(a, d->left);
        auto rhs = strip_paren(a, d->right);
        auto range = rhs.as_any<Range>();
        auto subject = lhs;
        if (!range) {
            range = lhs.as_any<Range>();
            subject = rhs;
        }
        if (!range || !subject) {
            return nullref;  // 範囲との比較ではない
        }

        auto expanded = branch_predicate(c, subject, range);
        if (!expanded) {
            return nullref;  // 両端とも無い (`..`)。比較になっていない。
        }
        if (d->op == BinaryOp::not_equal) {
            // Builder::not_ が括弧を入れる。unparse は優先順位ではなく Paren
            // ノードで括弧を出すので、包まないと `!(a >= 20 && ...)` が
            // `!a >= 20 && ...` と綴られる。
            expanded = b.not_(expanded, bool_type);
        }
        LoweredRangeCompare lowered;
        lowered.expr = expanded;
        c.tables.table<LoweredRangeCompare>().set(cmp, std::move(lowered));
        return expanded;
    }

}  // namespace brgen::nast::lowering
