/*license*/
#include "stream_io.hpp"
#include "../node/build.h"
#include "../node/util.h"

#include <optional>

#include <format>
#include <string>

namespace brgen::nast::lowering {

    namespace {

        // 定数回なら並べ、そうでなければ回す。並べるほうを選べるのは、
        // 展開したほうが位置の計算が消えて読みやすいから。
        Node<Body> repeat(Context& c, Builder b, Node<Expr> count, auto&& one) {
            auto body = b.a.make<Body>(b.loc);
            auto n = const_uint(c.tables, count);
            if (!n) {
                // 畳んだ値が無くても綴りが数ならそのまま並べる。
                if (auto lit_node = count.as_any<IntLiteral>()) {
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
            return b.assign(b.index_at(buffer, offset, index, b.int_type(8)),
                            b.member_call(stream, "get", nullref, b.int_type(8)));
        });
    }

    Node<Body> write_bytes(Context& c, Node<Expr> stream, Node<Expr> buffer, Node<Expr> offset,
                           Node<Expr> count) {
        if (!stream || !buffer || !count) {
            return nullref;
        }
        Builder b{c.a, count.ref(c.a).loc()};
        return repeat(c, b, count, [&](Node<Expr> index) -> Node<Statement> {
            return b.member_call(stream, "put", b.index_at(buffer, offset, index, b.int_type(8)), b.void_type());
        });
    }

}  // namespace brgen::nast::lowering
