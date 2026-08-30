/*license*/
#include "stream_io.hpp"
#include "../node/build.h"

#include <optional>

#include <format>
#include <string>

namespace brgen::nast::lowering {

    namespace {

        // stream.member(args...)。input.get() / output.put(x) を組む。
        Node<Expr> stream_call(Builder b, Node<Expr> stream, std::string_view member, Node<Expr> arg,
                               Node<Type> result) {
            auto name = b.a.make<Ident>(b.loc);
            name->identifier = std::string(member);
            auto ma = b.a.make<MemberAccess>(b.loc);
            ma->base = stream;
            ma->member = name;
            auto ft = b.a.make<FunctionType>(b.loc);
            ft->return_type = result;
            ma->type = ft;

            auto args = b.a.make<Arguments>(b.loc);
            if (arg) {
                auto one = b.a.make<Argument>(b.loc);
                one->value = arg;
                args->arguments.push_back(one);
            }
            auto n = b.a.make<Call>(b.loc);
            n->callee = ma;
            n->arguments = args;
            n->type = result;
            return n;
        }

        // buffer[offset + i]
        Node<Expr> at(Builder b, Node<Expr> buffer, Node<Expr> offset, Node<Expr> i) {
            auto idx = offset ? b.bin(BinaryOp::add, offset, i, b.int_type(64)) : i;
            return b.index(buffer, idx, b.int_type(8));
        }

        // 定数回なら並べ、そうでなければ回す。並べるほうを選べるのは、
        // 展開したほうが位置の計算が消えて読みやすいから。
        Node<Body> repeat(Context& c, Builder b, Node<Expr> count, auto&& one) {
            auto body = b.a.make<Body>(b.loc);
            std::optional<std::uint64_t> n;
            if (auto* v = c.tables.template table<ConstantValue>().get(count);
                v && v->kind == EvalKind::integer && !v->is_negative) {
                n = v->integer;
            }
            else if (auto lit_node = count.as_any<IntLiteral>()) {
                // 畳んだ値が無くても綴りが数ならそのまま並べる。
                auto& text = lit_node.ref(b.a)->value;
                std::uint64_t acc = 0;
                bool ok = !text.empty();
                for (auto ch : text) {
                    if (ch < '0' || ch > '9') {
                        ok = false;
                        break;
                    }
                    acc = acc * 10 + std::uint64_t(ch - '0');
                }
                if (ok) {
                    n = acc;
                }
            }
            if (n) {
                for (std::uint64_t i = 0; i < *n; i++) {
                    body->statements.push_back(one(b.lit(i)));
                }
                return body;
            }
            auto index_name = std::format("b{}", count.id());
            auto index = b.ref(index_name, b.int_type(64));
            auto inner = b.body(one(index));
            body->statements.push_back(b.count_loop(index_name, count, inner));
            return body;
        }

        Node<Expr> stream_literal(Context& c, SpecialLiteralKind kind, lexer::Loc loc) {
            auto n = c.a.make<SpecialLiteral>(loc);
            n->kind = kind;
            auto t = c.a.make<StreamType>(loc);
            t->kind = kind;
            n->type = t;
            return n;
        }

    }  // namespace

    Node<Expr> input_stream(Context& c, lexer::Loc loc) {
        return stream_literal(c, SpecialLiteralKind::input_, loc);
    }

    Node<Expr> output_stream(Context& c, lexer::Loc loc) {
        return stream_literal(c, SpecialLiteralKind::output_, loc);
    }

    Node<Body> read_bytes(Context& c, Node<Expr> stream, Node<Expr> buffer, Node<Expr> offset,
                          Node<Expr> count) {
        if (!stream || !buffer || !count) {
            return nullref;
        }
        Builder b{c.a, count.ref(c.a).loc()};
        return repeat(c, b, count, [&](Node<Expr> index) {
            return b.assign(at(b, buffer, offset, index),
                            stream_call(b, stream, "get", nullref, b.int_type(8)));
        });
    }

    Node<Body> write_bytes(Context& c, Node<Expr> stream, Node<Expr> buffer, Node<Expr> offset,
                           Node<Expr> count) {
        if (!stream || !buffer || !count) {
            return nullref;
        }
        Builder b{c.a, count.ref(c.a).loc()};
        return repeat(c, b, count, [&](Node<Expr> index) -> Node<Statement> {
            return stream_call(b, stream, "put", at(b, buffer, offset, index), b.void_type());
        });
    }

}  // namespace brgen::nast::lowering
