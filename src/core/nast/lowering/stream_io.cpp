/*license*/
#include "stream_io.hpp"

#include <format>
#include <string>

namespace brgen::nast::lowering {

    namespace {

        struct Build {
            Context& c;

            Node<Type> uint_type(std::size_t bits, lexer::Loc loc) {
                auto t = c.a.make<IntType>(loc);
                t->bit_size = bits;
                t->is_signed = false;
                t->endian = Endian::unspec;
                return t;
            }

            Node<Expr> lit(std::uint64_t v, lexer::Loc loc) {
                auto n = c.a.make<IntLiteral>(loc);
                n->value = std::to_string(v);
                n->type = uint_type(64, loc);
                return n;
            }

            Node<Expr> bin(BinaryOp op, Node<Expr> l, Node<Expr> r, lexer::Loc loc) {
                auto n = c.a.make<Binary>(loc);
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
                auto p = c.a.make<Paren>(loc);
                p->expr = e;
                p->type = e.ref(c.a)->type;
                return p;
            }

            // stream.member(args...)。input.get() / output.put(x) を組む。
            Node<Expr> call(Node<Expr> stream, std::string_view member, Node<Expr> arg,
                            Node<Type> result, lexer::Loc loc) {
                auto name = c.a.make<Ident>(loc);
                name->identifier = std::string(member);
                auto ma = c.a.make<MemberAccess>(loc);
                ma->base = stream;
                ma->member = name;
                auto ft = c.a.make<FunctionType>(loc);
                ft->return_type = result;
                ma->type = ft;

                auto args = c.a.make<Arguments>(loc);
                if (arg) {
                    auto one = c.a.make<Argument>(loc);
                    one->value = arg;
                    args->arguments.push_back(one);
                }
                auto n = c.a.make<Call>(loc);
                n->callee = ma;
                n->arguments = args;
                n->type = result;
                return n;
            }

            // buffer[offset + i]
            Node<Expr> at(Node<Expr> buffer, Node<Expr> offset, Node<Expr> index, lexer::Loc loc) {
                Node<Expr> idx = index;
                if (offset) {
                    idx = bin(BinaryOp::add, offset, paren(index, loc), loc);
                }
                auto n = c.a.make<Index>(loc);
                n->base = buffer;
                n->index = idx;
                n->type = uint_type(8, loc);
                return n;
            }

            Node<Statement> assign(Node<Expr> to, Node<Expr> value, lexer::Loc loc) {
                auto n = c.a.make<Assign>(loc);
                n->assignee = to;
                n->value = value;
                n->op = BinaryOp::assign;
                return n;
            }

            // 定数回なら並べ、そうでなければ回す。回す形は添字が要るので、
            // 名前を 1 つ作る。
            Node<Body> repeat(Node<Expr> count, lexer::Loc loc,
                              auto&& one) {
                auto body = c.a.make<Body>(loc);
                if (auto* v = c.tables.template table<ConstantValue>().get(count);
                    v && v->kind == EvalKind::integer && !v->is_negative) {
                    for (std::uint64_t i = 0; i < v->integer; i++) {
                        body->statements.push_back(one(lit(i, loc)));
                    }
                    return body;
                }
                if (auto lit_node = count.as_any<IntLiteral>()) {
                    // 畳んだ値が無くても綴りが数ならそのまま並べる。
                    std::uint64_t n = 0;
                    auto& text = lit_node.ref(c.a)->value;
                    auto ok = !text.empty();
                    for (auto ch : text) {
                        if (ch < '0' || ch > '9') {
                            ok = false;
                            break;
                        }
                        n = n * 10 + std::uint64_t(ch - '0');
                    }
                    if (ok) {
                        for (std::uint64_t i = 0; i < n; i++) {
                            body->statements.push_back(one(lit(i, loc)));
                        }
                        return body;
                    }
                }
                auto index_name = c.a.make<Ident>(loc);
                index_name->identifier = std::format("b{}", count.id());
                auto index = c.a.make<Reference>(loc);
                index->name = index_name;
                index->type = uint_type(64, loc);

                auto init = c.a.make<VariableDefinition>(loc);
                init->name = index_name;
                init->value = lit(0, loc);
                init->op = BinaryOp::define_assign;

                auto inner = c.a.make<Body>(loc);
                inner->statements.push_back(one(index));

                auto loop = c.a.make<Loop>(loc);
                loop->init = init;
                loop->condition = bin(BinaryOp::less, index, count, loc);
                loop->step = assign(index, bin(BinaryOp::add, index, lit(1, loc), loc), loc);
                loop->body = inner;
                body->statements.push_back(loop);
                return body;
            }
        };

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
        auto loc = count.ref(c.a).loc();
        Build b{c};
        auto byte = b.uint_type(8, loc);
        return b.repeat(count, loc, [&](Node<Expr> index) {
            return b.assign(b.at(buffer, offset, index, loc),
                            b.call(stream, "get", nullref, byte, loc), loc);
        });
    }

    Node<Body> write_bytes(Context& c, Node<Expr> stream, Node<Expr> buffer, Node<Expr> offset,
                           Node<Expr> count) {
        if (!stream || !buffer || !count) {
            return nullref;
        }
        auto loc = count.ref(c.a).loc();
        Build b{c};
        auto void_type = c.a.make<VoidType>(loc);
        return b.repeat(count, loc, [&](Node<Expr> index) -> Node<Statement> {
            return b.call(stream, "put", b.at(buffer, offset, index, loc), void_type, loc);
        });
    }

}  // namespace brgen::nast::lowering
