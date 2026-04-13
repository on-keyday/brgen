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

        std::string mangle_name(const std::string& base,
                                const std::vector<std::shared_ptr<ast::Type>>& args);

        std::string type_name(const std::shared_ptr<ast::Type>& t) {
            if (!t) {
                return "unknown";
            }
            if (auto it = ast::as<ast::IntType>(t)) {
                std::string s;
                s += it->is_signed ? 'i' : 'u';
                if (it->endian == ast::Endian::little) {
                    s += 'l';
                }
                else if (it->endian == ast::Endian::big) {
                    s += 'b';
                }
                s += std::to_string(it->bit_size ? *it->bit_size : 0);
                return s;
            }
            if (ast::as<ast::BoolType>(t)) {
                return "bool";
            }
            if (auto ft = ast::as<ast::FloatType>(t)) {
                std::string s = "f";
                if (ft->endian == ast::Endian::little) {
                    s += 'l';
                }
                else if (ft->endian == ast::Endian::big) {
                    s += 'b';
                }
                s += std::to_string(ft->bit_size ? *ft->bit_size : 0);
                return s;
            }
            if (auto id = ast::as<ast::IdentType>(t)) {
                if (id->ident) {
                    return std::string(id->ident->ident);
                }
            }
            if (auto et = ast::as<ast::EnumType>(t)) {
                if (auto m = ast::as<ast::Enum>(et->base.lock())) {
                    if (m->ident) {
                        return std::string(m->ident->ident);
                    }
                }
            }
            if (auto gt = ast::as<ast::GenericType>(t)) {
                if (gt->base_type && gt->base_type->ident) {
                    return mangle_name(std::string(gt->base_type->ident->ident), gt->type_arguments);
                }
            }
            return "T";
        }

        std::string mangle_name(const std::string& base,
                                const std::vector<std::shared_ptr<ast::Type>>& args) {
            std::string result = base;
            for (auto& a : args) {
                result += "_";
                result += type_name(a);
            }
            return result;
        }

        // ast::traverse follows only strong refs, so this yields exactly the
        // subtree owned by `root` (weak back-edges stop the walk).
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

        using InsideSet = std::unordered_set<const ast::Node*>;
        using InsideCache = std::map<const ast::Format*, InsideSet>;

        std::shared_ptr<ast::Format> instantiate(
            const std::shared_ptr<ast::Format>& target,
            const std::shared_ptr<ast::GenericType>& gt,
            InsideCache& inside_cache,
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

            auto [it, inserted] = inside_cache.try_emplace(target.get());
            if (inserted) {
                collect_inside(target, it->second);
            }
            const InsideSet& inside = it->second;

            NodeMap nm;
            ScopeMap sm;
            seed_scope_boundary(target, sm);

            MonoSubst subst{.inside = &inside, .param_map = &param_map};
            auto clone = ast::deep_copy(target, nm, sm, subst);
            assert(clone);
            // Remove type-parameter idents from the cloned scope before clearing,
            // otherwise the scope holds dangling weak_ptrs to destroyed Idents.
            if (clone->body && clone->body->scope) {
                auto& objs = clone->body->scope->objects;
                objs.erase(std::remove_if(objs.begin(), objs.end(), [](const auto& w) {
                    auto id = w.lock();
                    return !id || id->usage == ast::IdentUsage::define_type_parameter;
                }), objs.end());
            }
            clone->type_parameters.clear();
            if (clone->ident) {
                clone->ident->ident = mangle_name(
                    std::string(target->ident->ident), type_arguments);
            }
            clone->generic_base = target;
            clone->generic_arguments = type_arguments;
            for (auto& ta : type_arguments) {
                if (ast::as<ast::IdentType>(ta)) {
                    clone->depends.push_back(ast::cast_to<ast::IdentType>(ta));
                }
            }
            return clone;
        }

        std::shared_ptr<ast::IdentType> build_clone_ident(
            const std::shared_ptr<ast::GenericType>& gt,
            const std::shared_ptr<ast::Format>& clone_fmt) {
            assert(gt && gt->base_type && gt->base_type->ident && clone_fmt && clone_fmt->ident && clone_fmt->body && clone_fmt->body->struct_type);
            // LSP hover chases Ident.base; point it at the clone, not the raw template.
            auto& src_ident = gt->base_type->ident;
            auto new_ident = std::make_shared<ast::Ident>(src_ident->loc, std::string(src_ident->ident));
            new_ident->usage = ast::IdentUsage::reference_type;
            new_ident->scope = src_ident->scope;
            new_ident->base = clone_fmt->ident;
            std::shared_ptr<ast::Type> base_type = clone_fmt->body->struct_type;
            auto new_it = std::make_shared<ast::IdentType>(gt->loc, std::move(new_ident), std::move(base_type));
            new_it->bit_size = clone_fmt->body->struct_type->bit_size;
            new_it->bit_alignment = clone_fmt->body->struct_type->bit_alignment;
            new_it->non_dynamic_allocation = clone_fmt->body->struct_type->non_dynamic_allocation;
            return new_it;
        }

        void rewrite_generic_types(const std::shared_ptr<ast::Node>& root,
                                   const GenericIdentMap& mapping,
                                   std::unordered_set<const ast::Node*>& visited) {
            if (!root) return;
            if (!visited.insert(root.get()).second) return;
            ast::traverse(root, [&](auto& sub) {
                using U = std::decay_t<decltype(sub)>;
                using Elem = typename U::element_type;
                if constexpr (std::is_same_v<Elem, ast::Type> ||
                              std::is_same_v<Elem, ast::Node>) {
                    if (ast::as<ast::GenericType>(sub)) {
                        auto gt = ast::cast_to<ast::GenericType>(sub);
                        auto it = mapping.find(gt);
                        if (it != mapping.end()) {
                            sub = ast::cast_to<Elem>(it->second);
                            return;
                        }
                    }
                }
                rewrite_generic_types(sub, mapping, visited);
            });
        }

        // Post-order so Index sees updated expr->expr_type from child
        // MemberAccess rewrites. MemberAccess::base is a weak_ptr<Ident>
        // that ast::traverse skips, so it's rebound explicitly here.
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

        // Format::depends is vector<weak_ptr<IdentType>> which ast::traverse
        // does not follow, so rebind it here.
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
            if (!root) {
                return;
            }
            if (!visited.insert(root.get()).second) {
                return;
            }
            // Skip raw generic templates: their GenericTypes reference unbound T
            // and alias Format::depends weak_ptrs that monomorphize would corrupt.
            if (auto fmt = ast::as<ast::Format>(root)) {
                if (!fmt->type_parameters.empty()) {
                    return;
                }
            }
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

        GenericIdentMap gt_to_new_ident;
        std::map<ast::IdentType*, std::shared_ptr<ast::IdentType>> old_ident_to_new;
        std::vector<std::shared_ptr<ast::Format>> all_new_formats;
        InsideCache inside_cache;

        // Dedup cache: (target format, type arg pointers) → already-built IdentType.
        using InstKey = std::pair<const ast::Format*, std::vector<const ast::Type*>>;
        std::map<InstKey, std::shared_ptr<ast::IdentType>> inst_cache;

        auto make_key = [](const ast::Format* target, const std::shared_ptr<ast::GenericType>& gt) {
            std::vector<const ast::Type*> arg_ptrs;
            arg_ptrs.reserve(gt->type_arguments.size());
            for (auto& a : gt->type_arguments) {
                arg_ptrs.push_back(a.get());
            }
            return InstKey{target, std::move(arg_ptrs)};
        };

        // Secondary dedup by mangled name: catches structurally equal but
        // pointer-distinct type arguments (e.g. two separate IntType(u8)).
        using NameKey = std::pair<const ast::Format*, std::string>;
        std::map<NameKey, std::shared_ptr<ast::IdentType>> name_cache;

        // Round 1: collect from the whole program.
        std::vector<std::shared_ptr<ast::GenericType>> generics;
        {
            std::unordered_set<const ast::Node*> visited;
            collect_generic_types(program, generics, visited);
        }

        // Fix-point loop: instantiate GenericTypes, then scan newly created
        // clones for further GenericTypes born from substitution (e.g.
        // Bar[Foo[u8]] produces a clone containing Foo[Foo[u8]]).
        constexpr size_t max_rounds = 32;
        for (size_t round = 0; !generics.empty() && round < max_rounds; ++round) {
            std::vector<std::shared_ptr<ast::Format>> round_formats;

            for (auto& gt : generics) {
                auto target = resolve_target_format(gt, warnings);
                if (!target) {
                    continue;
                }

                auto key = make_key(target.get(), gt);
                if (auto cached = inst_cache.find(key); cached != inst_cache.end()) {
                    gt_to_new_ident[gt] = cached->second;
                    old_ident_to_new[gt->base_type.get()] = cached->second;
                    continue;
                }

                auto name_key = NameKey{target.get(), mangle_name("", gt->type_arguments)};
                if (auto cached = name_cache.find(name_key); cached != name_cache.end()) {
                    gt_to_new_ident[gt] = cached->second;
                    old_ident_to_new[gt->base_type.get()] = cached->second;
                    inst_cache[key] = cached->second;
                    continue;
                }

                auto clone = instantiate(target, gt, inside_cache, warnings);
                if (!clone) {
                    continue;
                }
                auto new_it = build_clone_ident(gt, clone);
                gt_to_new_ident[gt] = new_it;
                old_ident_to_new[gt->base_type.get()] = new_it;
                inst_cache[key] = new_it;
                name_cache[name_key] = new_it;
                round_formats.push_back(clone);
            }

            all_new_formats.insert(all_new_formats.end(),
                                   round_formats.begin(), round_formats.end());

            // Scan this round's clones for new GenericTypes.
            generics.clear();
            {
                std::unordered_set<const ast::Node*> visited;
                for (auto& f : round_formats) {
                    collect_generic_types(f, generics, visited);
                }
            }
        }

        if (!generics.empty() && warnings) {
            for (auto& gt : generics) {
                warnings->warning(gt->loc,
                                  "monomorphize: maximum instantiation depth (",
                                  nums(max_rounds), ") exceeded; this GenericType was not resolved");
            }
        }

        if (all_new_formats.empty()) {
            return;
        }

        // Append clones first so rewrite passes cover them too.
        for (auto& f : all_new_formats) {
            program->elements.push_back(f);
        }

        {
            std::unordered_set<const ast::Node*> visited;
            rewrite_format_depends(program, old_ident_to_new, visited);
        }

        {
            std::unordered_set<const ast::Node*> visited;
            rewrite_generic_types(program, gt_to_new_ident, visited);
        }

        {
            std::unordered_set<const ast::StructType*> clone_structs;
            for (auto& f : all_new_formats) {
                if (f->body && f->body->struct_type) {
                    clone_structs.insert(f->body->struct_type.get());
                }
            }
            std::unordered_set<const ast::Node*> visited;
            rewrite_clone_refs(program, clone_structs, visited);
        }
    }

}  // namespace brgen::middle
