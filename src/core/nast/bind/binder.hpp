/*license*/
#pragma once
#include "../nodes.h"
#include "../access.h"
#include "../traverse.h"
#include "../stream.h"
#include <unordered_map>

namespace brgen::nast::bind {
    struct Binder {
        Arena& a;
        LocationError& err;
        SideTables& tables;

        void bind(Node<Module> mod) {
            visit_all(a, mod, [&]<class T>(Node<T> n) {
                if (auto bound = n.template as_any<Format>()) {
                    bind(bound);
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
                for (auto& block : c.ref(a)->blocks) {
                    InnerStruct inner;
                    auto statements = block.field<"body.statements">(a);
                    for (auto& s : *statements) {
                        bind(s, inner.fields, functions, inner.asserts, enums, formats);
                    }
                    tables.table<InnerStruct>().set(block, std::move(inner));
                }
                auto type = a.make<StructUnionType>(c.ref(a).loc());
                type->base = c;
                auto field = a.make<Field>(c.ref(a).loc());
                field->type = type;
                fields.push_back(field);
            }
        }

        void bind(Node<Format> n) {
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
