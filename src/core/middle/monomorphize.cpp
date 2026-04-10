/*license*/
#include "monomorphize.h"

#include <core/ast/node/deep_copy.h>
#include <core/ast/traverse.h>

#include <map>
#include <unordered_set>
#include <vector>

namespace brgen::middle {

    namespace {

        using NodeMap = std::map<std::shared_ptr<ast::Node>, std::shared_ptr<ast::Node>>;
        using ScopeMap = std::map<std::shared_ptr<ast::Scope>, std::shared_ptr<ast::Scope>>;
        using ParamMap = std::map<std::shared_ptr<ast::Ident>, std::shared_ptr<ast::Type>>;
        using GenericIdentMap = std::map<std::shared_ptr<ast::GenericType>, std::shared_ptr<ast::IdentType>>;

        // Collect every Node strong-reachable from `root`. ast::traverse only
        // descends into shared_ptr / vector<shared_ptr> fields, never weak_ptr,
        // so the result is exactly the "inside" of the subtree rooted at `root`.
        void collect_inside(const std::shared_ptr<ast::Node>& root,
                            std::unordered_set<const ast::Node*>& out) {
            if (!root) return;
            if (!out.insert(root.get()).second) return;
            ast::traverse(root, [&](auto&& sub) {
                collect_inside(sub, out);
            });
        }

        struct MonoSubst {
            const std::unordered_set<const ast::Node*>* inside;
            const ParamMap* param_map;

            template <class T>
            std::shared_ptr<T> operator()(const std::shared_ptr<T>& node) const {
                if (!node) return nullptr;

                if constexpr (std::is_base_of_v<ast::Type, T>) {
                    if (auto it = ast::as<ast::IdentType>(node); it && it->ident) {
                        auto base = it->ident->base.lock();
                        if (ast::as<ast::Ident>(base)) {
                            auto def = ast::cast_to<ast::Ident>(base);
                            if (auto f = param_map->find(def); f != param_map->end()) {
                                if (ast::as<T>(f->second)) {
                                    return ast::cast_to<T>(f->second);
                                }
                            }
                        }
                    }
                }

                if constexpr (std::is_base_of_v<ast::Node, T>) {
                    if (!inside->contains(node.get())) {
                        return node;  // boundary: reuse the original subtree
                    }
                }
                return nullptr;
            }
        };

        std::shared_ptr<ast::Format> resolve_target_format(const std::shared_ptr<ast::GenericType>& gt, LocationError* warnings) {
            assert(gt);
            if (!gt->base_type) {
                if (warnings) warnings->warning(gt->loc, "monomorphize: generic type has no base_type; skipped");
                return nullptr;
            }
            auto base = gt->base_type->base.lock();
            auto st = ast::as<ast::StructType>(base);
            if (!st) {
                if (warnings) warnings->warning(gt->loc, "monomorphize: generic type base does not resolve to a struct type; skipped");
                return nullptr;
            }
            auto fmt_base = st->base.lock();
            if (!ast::as<ast::Format>(fmt_base)) {
                if (warnings) warnings->warning(gt->loc, "monomorphize: generic type target is not a format; skipped");
                return nullptr;
            }
            return ast::cast_to<ast::Format>(fmt_base);
        }

        void seed_scope_boundary(const std::shared_ptr<ast::Format>& fmt, ScopeMap& sm) {
            assert(fmt && fmt->body && fmt->body->scope);
            auto body_scope = fmt->body->scope;
            for (auto s = body_scope->prev.lock(); s; s = s->prev.lock()) {
                sm[s] = s;
            }
        }

        std::shared_ptr<ast::Format> instantiate(
            const std::shared_ptr<ast::Format>& target,
            const std::shared_ptr<ast::GenericType>& gt,
            LocationError* warnings) {
            assert(target);
            auto const& type_arguments = gt->type_arguments;
            if (target->type_parameters.size() != type_arguments.size()) {
                if (warnings) {
                    warnings->warning(gt->loc, "monomorphize: type argument count mismatch (expected ",
                                      nums(target->type_parameters.size()), ", got ", nums(type_arguments.size()), "); skipped")
                        .warning(target->loc, "monomorphize: target format is declared here");
                }
                return nullptr;
            }

            ParamMap param_map;
            for (size_t i = 0; i < target->type_parameters.size(); ++i) {
                auto& tp = target->type_parameters[i];
                assert(tp && tp->ident);
                param_map[tp->ident] = type_arguments[i];
            }

            std::unordered_set<const ast::Node*> inside;
            collect_inside(target, inside);

            NodeMap nm;
            ScopeMap sm;
            seed_scope_boundary(target, sm);

            MonoSubst subst{.inside = &inside, .param_map = &param_map};
            auto clone = ast::deep_copy(target, nm, sm, subst);
            assert(clone);
            clone->type_parameters.clear();
            clone->generic_base = target;
            clone->generic_arguments = type_arguments;
            for (auto& ta : type_arguments) {
                if (ast::as<ast::IdentType>(ta)) {
                    clone->depends.push_back(ast::cast_to<ast::IdentType>(ta));
                }
            }
            return clone;
        }

        // Build the replacement IdentType for a freshly instantiated clone.
        // It borrows the reference ident from the GenericType's base_type so
        // scope/name info survives, and its `base` is rebound to the cloned
        // Format's body struct_type.
        std::shared_ptr<ast::IdentType> build_clone_ident(
            const std::shared_ptr<ast::GenericType>& gt,
            const std::shared_ptr<ast::Format>& clone_fmt) {
            assert(gt && gt->base_type && clone_fmt && clone_fmt->body && clone_fmt->body->struct_type);
            std::shared_ptr<ast::Type> base_type = clone_fmt->body->struct_type;
            auto new_it = std::make_shared<ast::IdentType>(gt->loc, gt->base_type->ident, std::move(base_type));
            new_it->bit_size = clone_fmt->body->struct_type->bit_size;
            new_it->bit_alignment = clone_fmt->body->struct_type->bit_alignment;
            new_it->non_dynamic_allocation = clone_fmt->body->struct_type->non_dynamic_allocation;
            return new_it;
        }

        // Replace every GenericType shared_ptr field under `root` with the
        // pre-computed IdentType in `mapping`.
        void rewrite_generic_types(const std::shared_ptr<ast::Node>& root,
                                   const GenericIdentMap& mapping) {
            if (!root) return;
            ast::traverse(root, [&](auto& sub) {
                using U = std::decay_t<decltype(sub)>;
                using Elem = typename U::element_type;
                // Only fields typed as shared_ptr<Type> or shared_ptr<Node>
                // can hold either a GenericType or an IdentType, so limit the
                // replacement to those slots.
                if constexpr (std::is_same_v<Elem, ast::Type> ||
                              std::is_same_v<Elem, ast::Node>) {
                    if (ast::as<ast::GenericType>(sub)) {
                        auto gt = ast::cast_to<ast::GenericType>(sub);
                        auto it = mapping.find(gt);
                        if (it != mapping.end()) {
                            sub = ast::cast_to<Elem>(it->second);
                            return;  // replaced; don't descend into the old subtree
                        }
                    }
                }
                rewrite_generic_types(sub, mapping);
            });
        }

        // Post-order rewrite of references that still point at the original
        // (generic) Format after instantiation:
        //
        // * MemberAccess whose target type unwraps to a clone struct_type:
        //     re-lookup the member by name in the clone and rewrite `base`,
        //     `expr_type` and `member->expr_type` so downstream / LSP see
        //     the substituted types instead of the raw type-parameter ones.
        //     MemberAccess::base is a weak_ptr<Ident> and is not visited by
        //     ast::traverse, so it must be handled here explicitly.
        //
        // * Index: typing set `expr_type` to the element_type read from the
        //     raw ArrayType at the time. If its expr's expr_type has since
        //     been rewritten to a clone-substituted ArrayType, re-derive the
        //     element type from the updated source.
        //
        // Post-order: descend into children first so Index sees the updated
        // expr->expr_type produced by the MemberAccess rewrite below it.
        void rewrite_clone_refs(const std::shared_ptr<ast::Node>& root,
                                const std::unordered_set<const ast::StructType*>& clone_structs,
                                std::unordered_set<const ast::Node*>& visited) {
            if (!root) return;
            if (!visited.insert(root.get()).second) return;
            ast::traverse(root, [&](auto&& sub) {
                rewrite_clone_refs(sub, clone_structs, visited);
            });
            if (auto ma = ast::as<ast::MemberAccess>(root)) {
                if (ma->target && ma->target->expr_type && ma->member) {
                    auto t = ma->target->expr_type;
                    std::shared_ptr<ast::StructType> st;
                    if (auto it = ast::as<ast::IdentType>(t)) {
                        auto b = it->base.lock();
                        if (ast::as<ast::StructType>(b)) {
                            st = ast::cast_to<ast::StructType>(b);
                        }
                    }
                    else if (ast::as<ast::StructType>(t)) {
                        st = ast::cast_to<ast::StructType>(t);
                    }
                    if (st && clone_structs.contains(st.get())) {
                        if (auto found = st->lookup(ma->member->ident)) {
                            if (found->ident) {
                                ma->base = found->ident;
                            }
                            if (auto field = ast::as<ast::Field>(found)) {
                                ma->expr_type = field->field_type;
                                ma->member->expr_type = field->field_type;
                            }
                        }
                    }
                }
                return;
            }
            if (auto idx = ast::as<ast::Index>(root)) {
                if (idx->expr && idx->expr->expr_type) {
                    if (auto arr = ast::as<ast::ArrayType>(idx->expr->expr_type)) {
                        idx->expr_type = arr->element_type;
                    }
                }
                return;
            }
        }

        // Walk every Format under `root` and replace depends entries that
        // point to a rewritten GenericType's base_type IdentType with the new
        // clone IdentType. Format.depends is vector<weak_ptr<IdentType>> and
        // is not followed by ast::traverse, so we handle it here explicitly.
        void rewrite_format_depends(const std::shared_ptr<ast::Node>& root,
                                    const std::map<ast::IdentType*, std::shared_ptr<ast::IdentType>>& ident_map,
                                    std::unordered_set<const ast::Node*>& visited) {
            if (!root) return;
            if (!visited.insert(root.get()).second) return;
            if (auto fmt = ast::as<ast::Format>(root)) {
                for (auto& w : fmt->depends) {
                    auto locked = w.lock();
                    if (!locked) continue;
                    auto f = ident_map.find(locked.get());
                    if (f != ident_map.end()) {
                        w = f->second;
                    }
                }
            }
            ast::traverse(root, [&](auto&& sub) {
                rewrite_format_depends(sub, ident_map, visited);
            });
        }

        void collect_generic_types(const std::shared_ptr<ast::Node>& root,
                                   std::vector<std::shared_ptr<ast::GenericType>>& out,
                                   std::unordered_set<const ast::Node*>& visited) {
            if (!root) return;
            if (!visited.insert(root.get()).second) return;
            if (auto gt = ast::as<ast::GenericType>(root)) {
                out.push_back(ast::cast_to<ast::GenericType>(root));
            }
            ast::traverse(root, [&](auto&& sub) {
                collect_generic_types(sub, out, visited);
            });
        }

    }  // namespace

    void monomorphize(const std::shared_ptr<ast::Program>& program, LocationError* warnings) {
        if (!program) return;

        std::vector<std::shared_ptr<ast::GenericType>> generics;
        {
            std::unordered_set<const ast::Node*> visited;
            collect_generic_types(program, generics, visited);
        }
        if (generics.empty()) return;

        GenericIdentMap gt_to_new_ident;
        std::map<ast::IdentType*, std::shared_ptr<ast::IdentType>> old_ident_to_new;
        std::vector<std::shared_ptr<ast::Format>> new_formats;

        for (auto& gt : generics) {
            auto target = resolve_target_format(gt, warnings);
            if (!target) continue;
            auto clone = instantiate(target, gt, warnings);
            if (!clone) continue;
            auto new_it = build_clone_ident(gt, clone);
            gt_to_new_ident[gt] = new_it;
            old_ident_to_new[gt->base_type.get()] = new_it;
            new_formats.push_back(clone);
        }

        {
            std::unordered_set<const ast::Node*> visited;
            rewrite_format_depends(program, old_ident_to_new, visited);
        }

        rewrite_generic_types(program, gt_to_new_ident);

        {
            std::unordered_set<const ast::StructType*> clone_structs;
            for (auto& f : new_formats) {
                if (f->body && f->body->struct_type) {
                    clone_structs.insert(f->body->struct_type.get());
                }
            }
            std::unordered_set<const ast::Node*> visited;
            rewrite_clone_refs(program, clone_structs, visited);
        }

        for (auto& f : new_formats) {
            program->elements.push_back(f);
        }
    }

}  // namespace brgen::middle
