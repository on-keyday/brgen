/*license*/
#pragma once
#include "../node/nodes.h"
#include "../node/access.h"
#include "../node/traverse.h"
#include "../parse/stream.h"
#include <map>
#include <string>
#include <unordered_map>
#include <utility>

namespace brgen::nast::bind {
    struct Binder {
        Arena& a;
        LocationError& err;
        SideTables& tables;

        void bind(Node<Module> mod) {
            visit_all(a, mod, [&]<class T>(Node<T> n) {
                if (auto bound = n.template as_any<Format>()) {
                    bind_body(bound);
                }
                else if (auto generic = n.template as_any<GenericFormat>()) {
                    // generic format の body も同じ表に集める。typer が
                    // Foo[Plain] のメンバをここから引く。
                    bind_body(generic);
                }
            });
        }

        void bind(Node<Statement> stmt,
                  std::vector<Node<Field>>& fields,
                  std::vector<Node<Function>>& functions,
                  std::vector<Node<Assert>>& asserts,
                  std::vector<Node<Enum>>& enums,
                  std::vector<Node<Format>>& formats,
                  Node<Function>* encode_custom = nullptr,
                  Node<Function>* decode_custom = nullptr,
                  FormatKind* encode_kind = nullptr,
                  FormatKind* decode_kind = nullptr) {
            if (auto fld = stmt.as<Field>()) {
                fields.push_back(fld);
            }
            else if (auto enm = stmt.as<Enum>()) {
                enums.push_back(enm);
            }
            else if (auto fmt = stmt.as<Format>()) {
                formats.push_back(fmt);
            }
            else if (auto fn = stmt.as<Function>()) {
                if (encode_custom && decode_custom && encode_kind && decode_kind) {
                    if (auto name = fn.field<"name.identifier.optional">(a);
                        name == "encode" || name == "decode") {
                        if (name == "encode") {
                            *encode_custom = fn;
                            *encode_kind = FormatKind::custom;
                        }
                        else {
                            *decode_custom = fn;
                            *decode_kind = FormatKind::custom;
                        }
                        return;
                    }
                }
                functions.push_back(fn);
            }
            else if (auto asrt = stmt.as<Assert>()) {
                asserts.push_back(asrt);
            }
            else if (auto c = stmt.as<ConditionalExpr>()) {
                auto& blocks = c.ref(a)->blocks;
                auto loc = c.ref(a).loc();

                // 分岐 i の条件。既定の分岐 (`.. =>` / `else`) は null になる。
                auto cond_of = [&](std::size_t i) -> Node<Expr> {
                    if (auto cs = blocks[i].as<ConditionalStatement>()) {
                        return cs.ref(a)->condition;
                    }
                    return nullref;
                };

                // 分岐ごとに宣言されたフィールドを、分岐の順に集める。
                std::vector<std::vector<Node<Field>>> per_branch(blocks.size());
                for (std::size_t i = 0; i < blocks.size(); i++) {
                    InnerStruct inner;
                    auto statements = blocks[i].field<"body.statements">(a);
                    for (auto& s : *statements) {
                        bind(s, inner.fields, functions, inner.asserts, enums, formats);
                    }
                    per_branch[i] = inner.fields;
                    tables.table<InnerStruct>().set(blocks[i], std::move(inner));
                }

                auto type = a.make<StructUnionType>(loc);
                type->base = c;
                for (std::size_t i = 0; i < blocks.size(); i++) {
                    auto sc = a.make<StructUnionCandidate>(loc);
                    sc->cond = cond_of(i);
                    sc->inner_struct = blocks[i];
                    type->candidates.push_back(sc);
                }
                auto field = a.make<Field>(loc);
                field->type = type;
                fields.push_back(field);

                // 同じ名前が複数の分岐で宣言されうる。名前ごとに 1 つの Field を作り、
                // 型を UnionType にして分岐との対応を candidates に持たせる。
                // 参照の解決先を 1 つに定めるためのもので、これが無いと
                // 「どの分岐の Field を指すか」に答えが無くなる。
                std::vector<std::string> order;
                std::map<std::string, std::vector<std::pair<std::size_t, Node<Field>>>> by_name;
                for (std::size_t i = 0; i < per_branch.size(); i++) {
                    for (auto& f : per_branch[i]) {
                        auto n = f.field<"name.identifier">(a);
                        if (!n || n->empty()) {
                            continue;  // 無名フィールドは名前を持ち込まない
                        }
                        if (!by_name.contains(*n)) {
                            order.push_back(*n);
                        }
                        by_name[*n].push_back({i, f});
                    }
                }

                UnionFields synthesized;
                for (auto& name : order) {
                    auto& entries = by_name[name];
                    auto union_type = a.make<UnionType>(loc);
                    union_type->base_type = type;
                    if (auto m = c.as<Match>()) {
                        union_type->cond = m.ref(a)->condition;
                    }
                    // candidates は分岐と順序を揃える。名前が現れない分岐には
                    // cond だけ持つ候補を入れる。これが無いと、条件が重なる形
                    // (if x <= 3 / elif x <= 5) で後の分岐の値が前の分岐の入力にも
                    // 返ってしまう。末尾の不在は利用側の最終 fallback が拾うので入れない。
                    std::size_t cand_i = 0;
                    for (auto& [branch, fld] : entries) {
                        for (; cand_i < branch; cand_i++) {
                            auto pad = a.make<UnionCandidate>(loc);
                            pad->cond = cond_of(cand_i);
                            union_type->candidates.push_back(pad);
                        }
                        auto cand = a.make<UnionCandidate>(fld.ref(a).loc());
                        cand->cond = cond_of(branch);
                        cand->field = fld;
                        union_type->candidates.push_back(cand);
                        cand_i++;
                    }
                    // 位置は最初に宣言された分岐に合わせる。match の行より、
                    // その名前が最初に現れた場所のほうが定義位置として有用。
                    auto decl_loc = entries.front().second.ref(a).loc();
                    auto union_field = a.make<Field>(decl_loc);
                    union_field->name = a.make<Ident>(decl_loc, name);
                    union_field->type = union_type;
                    fields.push_back(union_field);
                    synthesized.fields.push_back(union_field);
                }
                tables.table<UnionFields>().set(c, std::move(synthesized));
            }
        }

        void bind_body(Node<NamedBodyStatement> n) {
            FormatState state;
            state.encode_kind = FormatKind::as_is;
            state.decode_kind = FormatKind::as_is;
            for (auto& stmt : n.ref(a)->body.ref(a)->statements) {
                bind(stmt, state.fields, state.functions, state.asserts,
                     state.nested_enums, state.nested_formats,
                     &state.encode_custom, &state.decode_custom,
                     &state.encode_kind, &state.decode_kind);
            }
            tables.table<FormatState>().set(n, std::move(state));
        }
    };
}  // namespace brgen::nast::bind
