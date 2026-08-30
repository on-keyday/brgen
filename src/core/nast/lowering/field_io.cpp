/*license*/
#include "field_io.hpp"
#include "int_bytes.hpp"
#include "../node/util.h"

#include <format>

namespace brgen::nast::lowering {

    namespace {

        struct Build {
            Context& c;
            Node<Field> field;

            Arena& a() {
                return c.a;
            }

            Node<Type> uint_type(std::size_t bits, lexer::Loc loc) {
                auto t = a().make<IntType>(loc);
                t->bit_size = bits;
                t->is_signed = false;
                t->endian = Endian::unspec;
                return t;
            }

            Node<Expr> lit(std::uint64_t v, lexer::Loc loc) {
                auto n = a().make<IntLiteral>(loc);
                n->value = std::to_string(v);
                n->type = uint_type(64, loc);
                return n;
            }

            Node<Expr> bin(BinaryOp op, Node<Expr> l, Node<Expr> r, lexer::Loc loc) {
                auto n = a().make<Binary>(loc);
                n->op = op;
                n->left = l;
                n->right = r;
                n->type = uint_type(64, loc);
                return n;
            }

            Node<Expr> paren(Node<Expr> e, lexer::Loc loc) {
                if (!e || !e.as_any<Binary>()) {
                    return e;
                }
                auto p = a().make<Paren>(loc);
                p->expr = e;
                p->type = e.ref(a())->type;
                return p;
            }

            Node<Expr> cast(Node<Type> to, Node<Expr> value, lexer::Loc loc) {
                auto tl = a().make<TypeLiteral>(loc);
                tl->literal = to;
                tl->type = a().make<MetaType>(loc);
                auto args = a().make<Arguments>(loc);
                auto arg = a().make<Argument>(loc);
                arg->value = value;
                args->arguments.push_back(arg);
                auto call = a().make<Call>(loc);
                call->callee = tl;
                call->arguments = args;
                call->type = to;
                auto cst = a().make<Cast>(loc);
                cst->base = call;
                cst->arguments = args;
                cst->type = to;
                return cst;
            }

            Node<Statement> assign(Node<Expr> to, Node<Expr> value, lexer::Loc loc) {
                auto n = a().make<Assign>(loc);
                n->assignee = to;
                n->value = value;
                n->op = BinaryOp::assign;
                return n;
            }

            Node<Body> body_of(Node<Statement> s, lexer::Loc loc) {
                auto b = a().make<Body>(loc);
                if (s) {
                    b->statements.push_back(s);
                }
                return b;
            }

            // その field に効いているバイト順。実行時に決まるなら代入を返す。
            Node<SpecifyOrder> dynamic_order() {
                auto* fe = c.tables.template table<FieldEndian>().get(field);
                return fe ? fe->dynamic : nullref;
            }

            bool order_is_undecided() {
                auto* fe = c.tables.template table<FieldEndian>().get(field);
                return fe && (fe->dynamic || fe->endian == Endian::native);
            }

            Endian static_order() {
                auto* fe = c.tables.template table<FieldEndian>().get(field);
                return fe && !fe->dynamic ? fe->endian : Endian::unspec;
            }
        };

        // 型が固定幅なら何バイトか。
        std::optional<std::uint64_t> fixed_bytes(Context& c, Node<Type> t) {
            auto* s = c.tables.table<TypeSize>().get(t);
            if (!s || s->kind != SizeKind::fixed || s->bits % 8 != 0) {
                return std::nullopt;
            }
            return s->bits / 8;
        }

        // 配列の個数。定数なら畳んだ値、式ならその式。`[..]` は決まらない。
        Node<Expr> element_count(Context& c, Build& b, Node<ArrayType> arr, lexer::Loc loc) {
            auto r = arr.ref(c.a);
            if (!r->length || r->length.as_any<Range>()) {
                return nullref;  // 末尾まで。位置の管理が要るので呼ぶ側の領分
            }
            if (auto* v = c.tables.table<ConstantValue>().get(r->length);
                v && v->kind == EvalKind::integer && !v->is_negative) {
                return b.lit(v->integer, loc);
            }
            return r->length;  // 元の木のノードを指す (複製しない)
        }

        Node<Body> lower_one(Context& c, Build& b, Node<Expr> target, Node<Type> type,
                             Node<Expr> bytes, Node<Expr> offset, bool decode);

        // 配列。要素を回す。要素幅が固定でないと位置が組めないので断る。
        Node<Body> lower_array(Context& c, Build& b, Node<ArrayType> arr, Node<Expr> target,
                               Node<Expr> bytes, Node<Expr> offset, bool decode) {
            auto& a = c.a;
            auto loc = arr.ref(a).loc();
            auto elem_type = arr.ref(a)->element_type;
            auto width = fixed_bytes(c, elem_type);
            if (!width) {
                return nullref;
            }
            auto count = element_count(c, b, arr, loc);
            if (!count) {
                return nullref;
            }

            // 添字。名前は由来のノード番号から作る。
            auto index_name = a.make<Ident>(loc);
            index_name->identifier = std::format("i{}", arr.id());
            auto index = a.make<Reference>(loc);
            index->name = index_name;
            index->type = b.uint_type(64, loc);

            // 要素の位置は offset + i * 幅。
            Node<Expr> elem_offset = b.bin(BinaryOp::mul, index, b.lit(*width, loc), loc);
            if (offset) {
                elem_offset = b.bin(BinaryOp::add, offset, b.paren(elem_offset, loc), loc);
            }

            auto elem_target = a.make<Index>(loc);
            elem_target->base = target;
            elem_target->index = index;
            elem_target->type = elem_type;

            auto inner = lower_one(c, b, elem_target, elem_type, bytes, elem_offset, decode);
            if (!inner) {
                return nullref;
            }

            // for i = 0; i < count; i = i + 1
            auto init = a.make<VariableDefinition>(loc);
            init->name = index_name;
            init->value = b.lit(0, loc);
            init->op = BinaryOp::define_assign;

            auto step = b.assign(index, b.bin(BinaryOp::add, index, b.lit(1, loc), loc), loc);

            auto loop = a.make<Loop>(loc);
            loop->init = init;
            loop->condition = b.bin(BinaryOp::less, index, count, loc);
            loop->step = step;
            loop->body = inner;
            return b.body_of(loop, loc);
        }

        Node<Body> lower_one(Context& c, Build& b, Node<Expr> target, Node<Type> type,
                             Node<Expr> bytes, Node<Expr> offset, bool decode) {
            auto& a = c.a;
            if (!type) {
                return nullref;
            }
            auto stripped = strip_wrappers(a, type);
            auto loc = stripped.ref(a).loc();

            if (auto arr = stripped.as_any<ArrayType>()) {
                return lower_array(c, b, arr, target, bytes, offset, decode);
            }

            // enum は下地の整数として読み書きし、値を型の側へ移す。
            Node<Type> io_type = stripped;
            bool is_enum = false;
            if (auto e = stripped.as_any<EnumType>()) {
                auto base = e.ref(a)->base;
                if (!base) {
                    return nullref;
                }
                io_type = strip_wrappers(a, base.ref(a)->base_type);
                is_enum = true;
            }
            if (!io_type || !io_type.as_any<IntType>()) {
                return nullref;  // 浮動小数 / 入れ子 format などはまだ
            }

            auto undecided = b.order_is_undecided();
            auto dyn = b.dynamic_order();
            auto order = b.static_order();

            if (decode) {
                auto value = undecided ? combine_int_either(c, bytes, offset, io_type, dyn)
                                       : combine_int(c, bytes, offset, io_type, order);
                if (!value) {
                    return nullref;
                }
                if (is_enum) {
                    value = b.cast(stripped, b.paren(value, loc), loc);
                }
                return b.body_of(b.assign(target, value, loc), loc);
            }
            // 書く側。enum は下地へ上げてから並べる。
            auto value = is_enum ? b.cast(io_type, target, loc) : target;
            return undecided ? split_int_either(c, bytes, offset, value, io_type, dyn)
                             : split_int(c, bytes, offset, value, io_type, order);
        }

    }  // namespace

    Node<Body> lower_field_decode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset) {
        if (!f) {
            return nullref;
        }
        Build b{c, f};
        return lower_one(c, b, target, f.ref(c.a)->type, bytes, offset, true);
    }

    Node<Body> lower_field_encode(Context& c, Node<Field> f, Node<Expr> target, Node<Expr> bytes,
                                  Node<Expr> offset) {
        if (!f) {
            return nullref;
        }
        Build b{c, f};
        return lower_one(c, b, target, f.ref(c.a)->type, bytes, offset, false);
    }

}  // namespace brgen::nast::lowering
