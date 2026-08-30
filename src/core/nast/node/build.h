/*license*/
#pragma once
#include "nodes.h"

#include <string>
#include <string_view>

// ノードを組み立てる小物。`util.h` が「形を問う」側なのに対して、こちらは
// 「形を作る」側。
//
// lowering の規則も、解析が合成する式も、作るものは同じ顔ぶれになる —
// 整数リテラル・二項・括弧・cast・代入・添字。段ごとに書き直していたので
// ここへ集めた。
//
// **括弧を自分で入れる。** unparse は優先順位ではなく Paren ノードを見て
// 括弧を出すので、合成した二項や三項を裸で入れ子にすると綴りが誤読される
// (`8 * (len - 8)` が `8 * len - 8` になる)。`paren()` を通す。
//
// loc は由来のものを持たせる (docs/exit_and_reversibility.md の復元性規則 2)。
// 途中で別の由来に移るときは `at(loc)` で持ち替える。

namespace brgen::nast {

    struct Builder {
        Arena& a;
        lexer::Loc loc;

        // 由来を持ち替えた同じ組み立て器。
        Builder at(lexer::Loc other) const {
            return Builder{a, other};
        }

        Node<Type> int_type(std::size_t bits, bool is_signed = false) const {
            auto t = a.make<IntType>(loc);
            t->bit_size = bits;
            t->is_signed = is_signed;
            t->endian = Endian::unspec;
            return t;
        }

        Node<Type> bool_type() const {
            return a.make<BoolType>(loc);
        }

        Node<Type> void_type() const {
            return a.make<VoidType>(loc);
        }

        Node<Expr> lit(std::uint64_t v) const {
            auto n = a.make<IntLiteral>(loc);
            n->value = std::to_string(v);
            n->type = int_type(64);
            return n;
        }

        // 中身が二項か三項なら括弧で包む。それ以外はそのまま。
        Node<Expr> paren(Node<Expr> e) const {
            if (!e || (!e.template as_any<Binary>() && !e.template as_any<Cond>())) {
                return e;
            }
            auto p = a.make<Paren>(loc);
            p->expr = e;
            p->type = e.ref(a)->type;
            return p;
        }

        // 被演算子は括弧を通す。
        Node<Expr> bin(BinaryOp op, Node<Expr> l, Node<Expr> r, Node<Type> type) const {
            if (!l || !r) {
                return nullref;
            }
            auto n = a.make<Binary>(loc);
            n->op = op;
            n->left = paren(l);
            n->right = paren(r);
            n->type = type;
            return n;
        }

        Node<Expr> cond(Node<Expr> c, Node<Expr> then, Node<Expr> els, Node<Type> type) const {
            if (!c || !then || !els) {
                return nullref;
            }
            auto n = a.make<Cond>(loc);
            n->cond = paren(c);
            n->then = paren(then);
            n->els = paren(els);
            n->type = type;
            return n;
        }

        Node<Expr> not_(Node<Expr> e, Node<Type> type) const {
            if (!e) {
                return nullref;
            }
            auto n = a.make<Unary>(loc);
            n->op = UnaryOp::not_;
            n->target = paren(e);
            n->type = type;
            return n;
        }

        // `<T>(x)`。Cast は TypeLiteral を callee にした Call を包む形
        // (parse.cpp の parse_call_or_cast と同じ組み方)。
        Node<Expr> cast(Node<Type> to, Node<Expr> value) const {
            if (!to || !value) {
                return nullref;
            }
            auto tl = a.make<TypeLiteral>(loc);
            tl->literal = to;
            tl->type = a.make<MetaType>(loc);
            auto args = a.make<Arguments>(loc);
            auto arg = a.make<Argument>(loc);
            arg->value = value;
            args->arguments.push_back(arg);
            auto call = a.make<Call>(loc);
            call->callee = tl;
            call->arguments = args;
            call->type = to;
            auto n = a.make<Cast>(loc);
            n->base = call;
            n->arguments = args;
            n->type = to;
            return n;
        }

        // 名前をそのまま指す参照。解決先が分かっているなら呼ぶ側で
        // Resolution 表に入れる。
        Node<Expr> ref(std::string_view name, Node<Type> type = nullref) const {
            auto id = a.make<Ident>(loc);
            id->identifier = std::string(name);
            auto n = a.make<Reference>(loc);
            n->name = id;
            n->type = type;
            return n;
        }

        Node<Expr> index(Node<Expr> base, Node<Expr> idx, Node<Type> type = nullref) const {
            if (!base || !idx) {
                return nullref;
            }
            auto n = a.make<Index>(loc);
            n->base = base;
            n->index = idx;
            n->type = type;
            return n;
        }

        Node<Statement> assign(Node<Expr> to, Node<Expr> value) const {
            if (!to || !value) {
                return nullref;
            }
            auto n = a.make<Assign>(loc);
            n->assignee = to;
            n->value = value;
            n->op = BinaryOp::assign;
            return n;
        }

        Node<Body> body(Node<Statement> one = nullref) const {
            auto b = a.make<Body>(loc);
            if (one) {
                b->statements.push_back(one);
            }
            return b;
        }

        // `for <name> = 0; <name> < count; <name> = <name> + 1: <inner>`
        // 添字の名前は呼ぶ側が決める (由来のノード番号から作ることが多い)。
        Node<Statement> count_loop(std::string_view index_name, Node<Expr> count,
                                   Node<Body> inner) const {
            if (!count || !inner) {
                return nullref;
            }
            auto id = a.make<Ident>(loc);
            id->identifier = std::string(index_name);
            auto index = a.make<Reference>(loc);
            index->name = id;
            index->type = int_type(64);

            auto init = a.make<VariableDefinition>(loc);
            init->name = id;
            init->value = lit(0);
            init->op = BinaryOp::define_assign;

            auto loop = a.make<Loop>(loc);
            loop->init = init;
            loop->condition = bin(BinaryOp::less, index, count, bool_type());
            loop->step = assign(index, bin(BinaryOp::add, index, lit(1), int_type(64)));
            loop->body = inner;
            return loop;
        }
    };

}  // namespace brgen::nast
