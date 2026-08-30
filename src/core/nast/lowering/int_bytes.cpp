/*license*/
#include "int_bytes.hpp"

#include <string>

namespace brgen::nast::lowering {

    struct Build {
            Context& c;

            Node<Type> int_type(std::size_t bits, bool is_signed, lexer::Loc loc) {
                auto t = c.a.make<IntType>(loc);
                t->bit_size = bits;
                t->is_signed = is_signed;
                t->endian = Endian::unspec;
                return t;
            }

            Node<Expr> lit(std::uint64_t v, lexer::Loc loc) {
                auto n = c.a.make<IntLiteral>(loc);
                n->value = std::to_string(v);
                n->type = int_type(64, false, loc);
                return n;
            }

            Node<Expr> bin(BinaryOp op, Node<Expr> l, Node<Expr> r, Node<Type> type, lexer::Loc loc) {
                auto n = c.a.make<Binary>(loc);
                n->op = op;
                n->left = l;
                n->right = r;
                n->type = type;
                return n;
            }

            // unparse は優先順位ではなく Paren ノードで括弧を出す。合成した
            // 二項はそのままだと綴りが誤読される。
            Node<Expr> paren(Node<Expr> e, lexer::Loc loc) {
                if (!e || !e.as_any<Binary>()) {
                    return e;
                }
                auto p = c.a.make<Paren>(loc);
                p->expr = e;
                p->type = e.ref(c.a)->type;
                return p;
            }

            // `u16(x)` の形。Cast は TypeLiteral を callee にした Call を包む
            // (parse_call_or_cast と同じ組み方)。
            Node<Expr> cast(Node<Type> to, Node<Expr> value, lexer::Loc loc) {
                auto tl = c.a.make<TypeLiteral>(loc);
                tl->literal = to;
                tl->type = c.a.make<MetaType>(loc);
                auto args = c.a.make<Arguments>(loc);
                auto arg = c.a.make<Argument>(loc);
                arg->value = value;
                args->arguments.push_back(arg);
                auto call = c.a.make<Call>(loc);
                call->callee = tl;
                call->arguments = args;
                call->type = to;
                auto cst = c.a.make<Cast>(loc);
                cst->base = call;
                cst->arguments = args;
                cst->type = to;
                return cst;
            }

            // bytes[offset + i]。offset が無ければ bytes[i]。
            Node<Expr> at(Node<Expr> bytes, Node<Expr> offset, std::size_t i, lexer::Loc loc) {
                Node<Expr> index = lit(i, loc);
                if (offset) {
                    index = i ? bin(BinaryOp::add, offset, lit(i, loc), int_type(64, false, loc), loc)
                              : offset;
                }
                auto n = c.a.make<Index>(loc);
            n->base = bytes;
            n->index = index;
            n->type = int_type(8, false, loc);
            return n;
        }
    };

    namespace {
        // 何バイトか。バイト境界に乗らない幅はここでは扱わない
        // (ビット単位の読み書きは畳み込みの段の話)。
        std::optional<std::size_t> byte_width(Arena& a, Node<Type> type, Ref<IntType>& out) {
            auto it = type.as_any<IntType>();
            if (!it) {
                return std::nullopt;
            }
            out = it.ref(a);
            if (out->bit_size == 0 || out->bit_size % 8 != 0) {
                return std::nullopt;
            }
            return out->bit_size / 8;
        }
    }  // namespace

    Node<Expr> combine_int(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Type> type,
                           Endian order) {
        auto& a = c.a;
        if (!bytes || !type) {
            return nullref;
        }
        Ref<IntType> info;
        auto n = byte_width(a, type, info);
        if (!n) {
            return nullref;
        }
        auto loc = type.ref(a).loc();
        Build b{c};

        // 合成は符号なしで行う。符号つきの型はそのまま OR すると上位バイトの
        // 符号拡張が混ざるので、組み終えてから落とす。
        auto raw = b.int_type(info->bit_size, false, loc);
        auto effective = order != Endian::unspec ? order : info->endian;
        if (effective == Endian::native) {
            return nullref;  // ここでは決まらない。下の注記を見ること。
        }
        bool big = effective != Endian::little;

        Node<Expr> acc;
        for (std::size_t i = 0; i < *n; i++) {
            auto byte_index = big ? i : *n - 1 - i;
            auto shift = (*n - 1 - i) * 8;
            Node<Expr> term = b.cast(raw, b.at(bytes, offset, byte_index, loc), loc);
            if (shift) {
                term = b.bin(BinaryOp::left_logical_shift, term, b.lit(shift, loc), raw, loc);
            }
            acc = acc ? b.bin(BinaryOp::bit_or, b.paren(acc, loc), b.paren(term, loc), raw, loc) : term;
        }
        if (info->is_signed) {
            acc = b.cast(type, b.paren(acc, loc), loc);
        }
        return acc;
    }

    Node<Body> split_int(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Expr> value,
                         Node<Type> type, Endian order) {
        auto& a = c.a;
        if (!bytes || !value || !type) {
            return nullref;
        }
        Ref<IntType> info;
        auto n = byte_width(a, type, info);
        if (!n) {
            return nullref;
        }
        auto loc = type.ref(a).loc();
        Build b{c};
        auto byte = b.int_type(8, false, loc);
        auto raw = b.int_type(info->bit_size, false, loc);
        auto effective = order != Endian::unspec ? order : info->endian;
        if (effective == Endian::native) {
            return nullref;
        }
        bool big = effective != Endian::little;

        auto body = a.make<Body>(loc);
        for (std::size_t i = 0; i < *n; i++) {
            auto shift = big ? (*n - 1 - i) * 8 : i * 8;
            Node<Expr> src = value;
            if (info->is_signed) {
                src = b.cast(raw, src, loc);  // 右シフトを算術にしない
            }
            if (shift) {
                src = b.bin(BinaryOp::right_logical_shift, src, b.lit(shift, loc), raw, loc);
            }
            auto assign = a.make<Assign>(loc);
            assign->assignee = b.at(bytes, offset, i, loc);
            assign->value = b.cast(byte, b.paren(src, loc), loc);
            assign->op = BinaryOp::assign;
            body->statements.push_back(assign);
        }
        return body;
    }

    namespace {
        // 判定そのもの。中身の綴りはバックエンドが埋める。
        Node<Expr> is_little_endian(Context& c, Node<SpecifyOrder> dynamic_order, lexer::Loc loc) {
            auto n = c.a.make<IsLittleEndian>(loc);
            n->order = dynamic_order;
            n->type = c.a.make<BoolType>(loc);
            return n;
        }
    }  // namespace

    Node<Expr> combine_int_either(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Type> type,
                                  Node<SpecifyOrder> dynamic_order) {
        auto& a = c.a;
        if (!type) {
            return nullref;
        }
        auto little = combine_int(c, bytes, offset, type, Endian::little);
        auto big = combine_int(c, bytes, offset, type, Endian::big);
        if (!little || !big) {
            return nullref;
        }
        auto loc = type.ref(a).loc();
        Build b{c};
        auto n = a.make<Cond>(loc);
        n->cond = is_little_endian(c, dynamic_order, loc);
        n->then = b.paren(little, loc);
        n->els = b.paren(big, loc);
        n->type = type;
        return n;
    }

    Node<Body> split_int_either(Context& c, Node<Expr> bytes, Node<Expr> offset, Node<Expr> value,
                                Node<Type> type, Node<SpecifyOrder> dynamic_order) {
        auto& a = c.a;
        if (!type) {
            return nullref;
        }
        auto little = split_int(c, bytes, offset, value, type, Endian::little);
        auto big = split_int(c, bytes, offset, value, type, Endian::big);
        if (!little || !big) {
            return nullref;
        }
        auto loc = type.ref(a).loc();

        auto then_branch = a.make<ConditionalStatement>(loc);
        then_branch->condition = is_little_endian(c, dynamic_order, loc);
        then_branch->body = little;
        // 既定の分岐は条件なしの BodyStatement (parse.cpp の else と同じ形)。
        auto else_branch = a.make<BodyStatement>(loc);
        else_branch->body = big;

        auto if_ = a.make<If>(loc);
        if_->blocks.push_back(then_branch);
        if_->blocks.push_back(else_branch);

        auto body = a.make<Body>(loc);
        body->statements.push_back(if_);
        return body;
    }

}  // namespace brgen::nast::lowering
