/*license*/
#include "predicate.hpp"
#include "../node/util.h"

namespace brgen::nast::lowering {

    // 綴り grater_or_eq は元実装から引き継いだもの。直すと生成物と元 AST に波及する。
    struct Builder {
        Context& c;
        Node<Type> bool_type;

        Node<Expr> bin(BinaryOp op, Node<Expr> l, Node<Expr> r, lexer::Loc loc) {
            auto n = c.a.make<Binary>(loc);
            n->op = op;
            n->left = l;
            n->right = r;
            n->type = bool_type;
            return n;
        }

        Node<Expr> and_(Node<Expr> l, Node<Expr> r, lexer::Loc loc) {
            if (!l) {
                return r;
            }
            if (!r) {
                return l;
            }
            return bin(BinaryOp::logical_and, l, r, loc);
        }
    };

    Node<Expr> branch_predicate(Context& c, Node<Expr> subject, Node<Expr> pattern) {
        auto& a = c.a;
        if (!pattern) {
            return nullref;
        }
        auto loc = pattern.ref(a).loc();
        Builder b{c, a.make<BoolType>(loc)};

        if (!subject) {
            return pattern;  // 条件なし match。パターンがそのまま述語。
        }

        // `1,2 =>` は OrCond。葉ごとに述語を作って || で繋ぐ。葉が範囲でも
        // よいので、再帰で組む。
        if (auto orc = pattern.as_any<OrCond>()) {
            Node<Expr> acc;
            for (auto& leaf : orc.ref(a)->conds) {
                auto one = branch_predicate(c, subject, leaf);
                acc = acc ? b.bin(BinaryOp::logical_or, acc, one, loc) : one;
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
                acc = b.and_(acc, b.bin(BinaryOp::grater_or_eq, subject, d->start, loc), loc);
            }
            if (d->end) {
                // `..` は右開き、`..=` は右閉じ。
                auto op = d->op == BinaryOp::range_inclusive ? BinaryOp::less_or_eq : BinaryOp::less;
                acc = b.and_(acc, b.bin(op, subject, d->end, loc), loc);
            }
            return acc;
        }

        return b.bin(BinaryOp::equal, subject, pattern, loc);
    }

}  // namespace brgen::nast::lowering

namespace brgen::nast::lowering {

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
            auto loc = cmp.ref(a).loc();
            // 括弧で包む。unparse は優先順位ではなく Paren ノードで括弧を出す
            // ので、包まないと `!(a >= 20 && ...)` が `!a >= 20 && ...` と
            // 綴られる (木は正しいが綴りが嘘になる)。
            auto paren = a.make<Paren>(loc);
            paren->expr = expanded;
            paren->type = expanded.ref(a)->type;
            auto n = a.make<Unary>(loc);
            n->op = UnaryOp::not_;
            n->target = paren;
            n->type = a.make<BoolType>(loc);
            expanded = n;
        }
        LoweredRangeCompare lowered;
        lowered.expr = expanded;
        c.tables.table<LoweredRangeCompare>().set(cmp, std::move(lowered));
        return expanded;
    }

}  // namespace brgen::nast::lowering
