/*license*/
#include "int_bytes.hpp"
#include "../node/build.h"

#include <string>

namespace brgen::nast::lowering {

    namespace {
        // bytes[offset + i]
        Node<Expr> at(Builder b, Node<Expr> bytes, Node<Expr> offset, std::size_t i) {
            Node<Expr> index = b.lit(i);
            if (offset) {
                index = i ? b.bin(BinaryOp::add, offset, b.lit(i), b.int_type(64)) : offset;
            }
            return b.index(bytes, index, b.int_type(8));
        }

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
        Builder b{a, loc};

        // 合成は符号なしで行う。符号つきの型はそのまま OR すると上位バイトの
        // 符号拡張が混ざるので、組み終えてから落とす。
        auto raw = b.int_type(info->bit_size);
        auto effective = order != Endian::unspec ? order : info->endian;
        if (effective == Endian::native) {
            return nullref;  // ここでは決まらない。下の注記を見ること。
        }
        bool big = effective != Endian::little;

        Node<Expr> acc;
        for (std::size_t i = 0; i < *n; i++) {
            auto byte_index = big ? i : *n - 1 - i;
            auto shift = (*n - 1 - i) * 8;
            Node<Expr> term = b.cast(raw, at(b, bytes, offset, byte_index));
            if (shift) {
                term = b.bin(BinaryOp::left_logical_shift, term, b.lit(shift), raw);
            }
            acc = acc ? b.bin(BinaryOp::bit_or, acc, term, raw) : term;
        }
        if (info->is_signed) {
            acc = b.cast(type, acc);
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
        Builder b{a, loc};
        auto byte = b.int_type(8);
        auto raw = b.int_type(info->bit_size);
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
                src = b.cast(raw, src);  // 右シフトを算術にしない
            }
            if (shift) {
                src = b.bin(BinaryOp::right_logical_shift, src, b.lit(shift), raw);
            }
            auto assign = a.make<Assign>(loc);
            assign->assignee = at(b, bytes, offset, i);
            assign->value = b.cast(byte, src);
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
        Builder b{a, loc};
        auto n = a.make<Cond>(loc);
        n->cond = is_little_endian(c, dynamic_order, loc);
        n->then = b.paren(little);
        n->els = b.paren(big);
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
