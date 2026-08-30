/*license*/
#include "field_io.hpp"
#include "int_bytes.hpp"
#include "../node/build.h"
#include "../node/util.h"

#include <format>

namespace brgen::nast::lowering {

    namespace {

        // 配列の個数。定数なら畳んだ値、式ならその式。`[..]` は決まらない。
        Node<Expr> element_count(Context& c, Builder b, Node<ArrayType> arr) {
            auto r = arr.ref(c.a);
            if (!r->length || r->length.as_any<Range>()) {
                return nullref;  // 末尾まで。位置の管理が要るので呼ぶ側の領分
            }
            if (auto n = const_uint(c.a, c.tables, r->length)) {
                return b.lit(*n);
            }
            return r->length;  // 元の木のノードを指す (複製しない)
        }

        Node<Body> lower_one(Context& c, Node<Field> f, Node<Expr> target, Node<Type> type,
                             Node<Expr> bytes, Node<Expr> offset, bool decode);

        // 配列。要素を回す。要素幅が固定でないと位置が組めないので断る。
        Node<Body> lower_array(Context& c, Node<Field> f, Node<ArrayType> arr, Node<Expr> target,
                               Node<Expr> bytes, Node<Expr> offset, bool decode) {
            auto& a = c.a;
            Builder b{a, arr.ref(a).loc()};
            auto elem_type = arr.ref(a)->element_type;
            auto width = byte_width(c, elem_type);
            if (!width) {
                return nullref;
            }
            auto count = element_count(c, b, arr);
            if (!count) {
                return nullref;
            }

            // 添字。名前は由来のノード番号から作る。
            auto index_name = std::format("i{}", arr.id());
            auto index = b.ref(index_name, b.int_type(64));

            // 要素の位置は offset + i * 幅。
            Node<Expr> elem_offset = b.bin(BinaryOp::mul, index, b.lit(*width), b.int_type(64));
            if (offset) {
                elem_offset = b.bin(BinaryOp::add, offset, elem_offset, b.int_type(64));
            }

            auto elem_target = b.index(target, index, elem_type);
            auto inner = lower_one(c, f, elem_target, elem_type, bytes, elem_offset, decode);
            if (!inner) {
                return nullref;
            }
            return b.body(b.count_loop(index_name, count, inner));
        }

        Node<Body> lower_one(Context& c, Node<Field> f, Node<Expr> target, Node<Type> type,
                             Node<Expr> bytes, Node<Expr> offset, bool decode) {
            auto& a = c.a;
            if (!type) {
                return nullref;
            }
            auto stripped = strip_wrappers(a, type);
            Builder b{a, stripped.ref(a).loc()};

            if (auto arr = stripped.as_any<ArrayType>()) {
                return lower_array(c, f, arr, target, bytes, offset, decode);
            }

            // enum は下地の整数として読み書きし、値を型の側へ移す。
            Node<Type> io_type = stripped;
            bool is_enum = false;
            bool is_float = false;
            if (auto e = stripped.as_any<EnumType>()) {
                auto base = e.ref(a)->base;
                if (!base) {
                    return nullref;
                }
                io_type = strip_wrappers(a, base.ref(a)->base_type);
                is_enum = true;
            }
            else if (auto fl = stripped.as_any<FloatType>()) {
                // 浮動小数は同じ幅の整数として並べ、値はビットの読み替えで移す。
                // `<u32>(f)` (値の変換) とは別物なので BitCast で分ける —
                // 取り違えると 1.0 が 1065353216 になる。
                auto bits = fl.ref(a)->bit_size;
                if (bits == 0 || bits % 8 != 0) {
                    return nullref;
                }
                io_type = b.int_type(bits);
                is_float = true;
            }
            if (!io_type || !io_type.as_any<IntType>()) {
                return nullref;  // 入れ子 format などはまだ
            }

            // その field に効いているバイト順。実行時に決まるなら代入を指す。
            auto* fe = c.tables.table<FieldEndian>().get(f);
            auto undecided = fe && (fe->dynamic || fe->endian == Endian::native);
            auto dyn = fe ? fe->dynamic : nullref;
            auto order = fe && !fe->dynamic ? fe->endian : Endian::unspec;

            if (decode) {
                auto value = undecided ? combine_int_either(c, bytes, offset, io_type, dyn)
                                       : combine_int(c, bytes, offset, io_type, order);
                if (!value) {
                    return nullref;
                }
                if (is_enum) {
                    value = b.cast(stripped, value);
                }
                else if (is_float) {
                    auto bc = a.make<BitCast>(b.loc);
                    bc->target = b.paren(value);
                    bc->type = stripped;
                    value = bc;
                }
                return b.body(b.assign(target, value));
            }
            // 書く側。enum は下地へ上げ、浮動小数はビットを整数として見る。
            Node<Expr> value = target;
            if (is_enum) {
                value = b.cast(io_type, target);
            }
            else if (is_float) {
                auto bc = a.make<BitCast>(b.loc);
                bc->target = target;
                bc->type = io_type;
                value = bc;
            }
            return undecided ? split_int_either(c, bytes, offset, value, io_type, dyn)
                             : split_int(c, bytes, offset, value, io_type, order);
        }

    }  // namespace

    Node<Body> lower_field_decode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset) {
        if (!f) {
            return nullref;
        }
        return lower_one(c, f, target, f.ref(c.a)->type, bytes, offset, true);
    }

    Node<Body> lower_field_encode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset) {
        if (!f) {
            return nullref;
        }
        return lower_one(c, f, target, f.ref(c.a)->type, bytes, offset, false);
    }

}  // namespace brgen::nast::lowering
