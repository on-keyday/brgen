/*license*/
#pragma once

#include <algorithm>
#include <unordered_set>
#include "ebm/extended_binary_module.hpp"
#include "ebmcodegen/stub/util.hpp"
#include "ebmgen/access.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/mapping.hpp"
namespace ebmcodegen::util {

    ebmgen::expected<std::vector<ebm::StatementRef>> sorted_struct(auto&& ctx) {
        ebmgen::MappingTable& module_ = get_visitor(ctx).module_;

        // 構造体IDをキーとして、その参照と依存先リストを保持する構造
        struct Node {
            ebm::StatementRef ref;
            std::vector<std::uint64_t> deps;
        };
        std::unordered_map<std::uint64_t, Node> struct_graph;
        std::vector<std::uint64_t> order_of_appearance;

        auto do_handle = [&](ebm::StatementRef stmt_ref, const ebm::StructDecl* struct_ptr) -> ebmgen::expected<void> {
            uint64_t id = get_id(stmt_ref);
            order_of_appearance.push_back(id);

            std::vector<std::uint64_t> deps;
            auto res = handle_fields(ctx, struct_ptr->fields, true, [&](ebm::StatementRef field_ref, auto&) -> ebmgen::expected<void> {
                MAYBE(field_decl, ebmgen::access_field<"body.field_decl">(module_, field_ref));
                MAYBE(tree, get_type_tree(ctx, field_decl.field_type));

                const ebm::Type* last_type = tree.back();
                if (last_type->body.kind == ebm::TypeKind::STRUCT) {
                    MAYBE(decl, ebmgen::access_field<"body.id">(module_, last_type));
                    deps.push_back(get_id(decl.id));
                }
                else if (last_type->body.kind == ebm::TypeKind::VARIANT) {
                    for (auto& member_ref : last_type->body.variant_desc()->members.container) {
                        MAYBE(member_decl, ebmgen::access_field<"body.id">(module_, member_ref));
                        deps.push_back(get_id(member_decl.id));
                    }
                }
                return {};
            });

            if (!res) return ebmgen::unexpect_error(std::move(res.error()));
            struct_graph[id] = Node{stmt_ref, std::move(deps)};
            return {};
        };

        // 1. 全ての構造体をスキャンし、依存関係を一度だけ解析してキャッシュする
        for (auto& s : module_.module().statements) {
            auto* struct_ptr = s.body.struct_decl();
            if (!struct_ptr) continue;
            MAYBE_VOID(ok, do_handle(s.id, struct_ptr));
        }
        // aliasの場合も
        for (auto& alias : module_.module().aliases) {
            if (alias.hint != ebm::AliasHint::STATEMENT) {
                continue;
            }
            auto ref = from_any_ref<ebm::StatementRef>(alias.from);
            auto struct_decl = ebmgen::access_field<"body.struct_decl">(module_, ref);
            if (!struct_decl) {
                continue;
            }
            MAYBE_VOID(ok, do_handle(ref, struct_decl));
        }

        // 2. トポロジカルソートの実行
        std::vector<ebm::StatementRef> result;
        std::unordered_set<std::uint64_t> visited;

        auto visit = [&](auto self, uint64_t id) -> void {
            if (visited.contains(id)) return;

            auto it = struct_graph.find(id);
            if (it != struct_graph.end()) {
                // 依存先に先に訪れる
                for (uint64_t dep_id : it->second.deps) {
                    self(self, dep_id);
                }
                // 全ての依存先を処理し終えたら、自分を追加
                result.push_back(it->second.ref);
            }
            visited.insert(id);
        };

        for (uint64_t id : order_of_appearance) {
            visit(visit, id);
        }

        return result;
    }
}  // namespace ebmcodegen::util