/*license*/
#include "scope_resolver.hpp"

#include "../traverse.h"

namespace brgen::nast::bind {

    namespace {

        // 前方参照でき、外側のスコープからも見える宣言。
        // 元の is_type_ident (scope.h:46) に当たるが、Ident::usage ではなく
        // 宣言している文の種類で判定する。usage は解析結果なので nast のノードには無い。
        bool is_type_decl(NodeType t) {
            return t == NodeType::Format || t == NodeType::GenericFormat ||
                   t == NodeType::State || t == NodeType::Enum ||
                   t == NodeType::TypeParameter;
        }

        // 本体が「型の壁」になる文。外を見るときに型だけへ絞られる。
        // 元は Scope::owner の種類で見ている (scope.h:75-83)。fn は壁ではない。
        bool is_type_barrier(NodeType t) {
            return t == NodeType::Format || t == NodeType::GenericFormat ||
                   t == NodeType::State || t == NodeType::Enum;
        }

    }  // namespace

    void ScopeResolver::declare(Env& env, Node<Ident> name, Node<Statement> node, bool is_type,
                                std::size_t position) {
        auto* d = a.get<Ident>(name);
        if (!d || d->identifier.empty()) {
            return;  // 無名フィールド (:" " など) は名前を持ち込まない
        }
        env.decls.push_back(Decl{d->identifier, node, position, is_type});
    }

    void ScopeResolver::collect(Env& env, const std::vector<Node<Statement>>& stmts,
                                std::size_t base) {
        for (std::size_t i = 0; i < stmts.size(); i++) {
            auto s = stmts[i];
            // 名前を持ち込む文 = NamedStatement の派生。例外は無い。
            if (auto named = s.as_any<NamedStatement>()) {
                declare(env, a.get<NamedStatement>(named)->name, s, is_type_decl(s.type()),
                        base + i + 1);
                if (auto typed = s.as_any<NamedTypeStatement>()) {
                    declare_inline_formats(env, a.get<NamedTypeStatement>(typed)->type,
                                           base + i + 1);
                }
                continue;
            }
            // if / match は分岐の中で宣言された名前を囲むブロックへ持ち込む。
            // 実体は binder が合成した Field (UnionFields)。分岐ごとの Field ではなく
            // こちらを指すことで、参照の解決先が 1 つに定まる。
            if (auto cond = s.as_any<ConditionalExpr>()) {
                if (auto* uf = tables.table<UnionFields>().get(cond)) {
                    for (auto& f : uf->fields) {
                        declare(env, a.get<Field>(f)->name, f, false, base + i + 1);
                    }
                }
            }
        }
    }

    // 名前つきインライン format (`named :[len]format Item:`) は、型の中に居ながら
    // 囲むスコープへ名前を持ち込む。型の包み (配列 / optional など) を剥がして探す。
    // 見つけた format の本体へは降りない。中の入れ子まで外へ出てしまうため。
    void ScopeResolver::declare_inline_formats(Env& env, Node<Type> ty, std::size_t position) {
        if (!ty) {
            return;
        }
        if (auto inl = ty.as_any<InlineStructType>()) {
            auto fmt = a.get<InlineStructType>(inl)->inlined_format;
            if (fmt) {
                declare(env, a.get<Format>(fmt)->name, fmt, true, position);
            }
            return;
        }
        if (auto arr = ty.as_any<ArrayType>()) {
            declare_inline_formats(env, a.get<ArrayType>(arr)->element_type, position);
            return;
        }
        if (auto opt = ty.as_any<OptionalType>()) {
            declare_inline_formats(env, a.get<OptionalType>(opt)->base_type, position);
            return;
        }
        if (auto wrap = ty.as_any<WrapperType>()) {
            declare_inline_formats(env, a.get<WrapperType>(wrap)->base, position);
        }
    }

    Node<Statement> ScopeResolver::lookup(const Env& env, std::string_view name,
                                          std::size_t position) const {
        bool only_type = false;
        for (const Env* e = &env; e; e = e->parent) {
            // 逆順に見る。後の宣言が勝つ (shadowing)。
            for (auto it = e->decls.rbegin(); it != e->decls.rend(); ++it) {
                if (!it->is_type) {
                    if (only_type || position <= it->position) {
                        continue;  // 型でないものは後方参照のみ
                    }
                }
                if (it->name == name) {
                    return it->node;
                }
            }
            if (e->type_barrier) {
                only_type = true;  // format / state / enum を出た。以降は型だけ
            }
            position = e->position_in_parent;
        }
        // global は最後に、型限定なしで前方も含めて舐める (typing.cpp:881 の fallback)。
        if (global_) {
            for (auto& d : global_->decls) {
                if (d.name == name) {
                    return d.node;
                }
            }
        }
        return nullref;
    }

    void ScopeResolver::resolve_name(Env& env, Node<Ident> name, std::size_t position) {
        auto* d = a.get<Ident>(name);
        if (!d || d->identifier.empty()) {
            return;
        }
        auto found = lookup(env, d->identifier, position);
        if (!found) {
            unresolved++;
            return;
        }
        tables.table<Resolution>().set(name, Resolution{.target = found});
        resolved++;
    }

    void ScopeResolver::walk_children(Env& env, NodeAny n, std::size_t position) {
        traverse(a, n, [&](auto child) {
            walk(env, child, position);
        });
    }

    void ScopeResolver::run_block(Env& env, Node<Body> body, std::size_t base) {
        auto* d = a.get<Body>(body);
        if (!d) {
            return;
        }
        collect(env, d->statements, base);
        for (std::size_t i = 0; i < d->statements.size(); i++) {
            walk(env, d->statements[i], base + i + 1);
        }
    }

    void ScopeResolver::walk(Env& env, NodeAny n, std::size_t position) {
        if (!n) {
            return;
        }

        // 型の参照。IdentType にしか無いので先に見る。
        if (auto id = n.as_any<IdentType>()) {
            resolve_name(env, a.get<IdentType>(id)->ident, position);
            walk_children(env, n, position);
            return;
        }

        if (auto ref = n.as_any<Reference>()) {
            resolve_name(env, a.get<Reference>(ref)->name, position);
            walk_children(env, n, position);
            return;
        }

        // 本体がスコープになる文。
        if (auto named_body = n.as_any<NamedBodyStatement>()) {
            auto* d = a.get<NamedBodyStatement>(named_body);
            Env inner{&env, position, is_type_barrier(n.type()), {}};
            // fn の引数は本体スコープの先頭に入る (parse.cpp:1686 が
            // parse_indent_block へ渡している。format の型パラメータも同じ扱い)。
            // 型パラメータは本体スコープの先頭に入る (parse.cpp が
            // parse_indent_block へ渡すのと同じ扱い)。
            if (auto gen = n.as_any<GenericFormat>()) {
                for (auto& tp : a.get<GenericFormat>(gen)->type_parameters) {
                    declare(inner, a.get<TypeParameter>(tp)->name, tp, true, 0);
                }
            }
            if (auto fn = n.as_any<Function>()) {
                auto* f = a.get<Function>(fn);
                for (auto& p : f->parameters) {
                    declare(inner, a.get<Parameter>(p)->name, p, false, 0);
                }
                // 引数の型と戻り値の型は外側で解決する
                for (auto& p : f->parameters) {
                    walk(env, a.get<Parameter>(p)->type, position);
                }
                walk(env, f->return_type, position);
            }
            run_block(inner, d->body, 0);
            return;
        }

        if (auto enm = n.as_any<Enum>()) {
            auto* d = a.get<Enum>(enm);
            walk(env, d->base_type, position);  // 基底型は外側
            Env inner{&env, position, true, {}};
            for (std::size_t i = 0; i < d->members.size(); i++) {
                declare(inner, a.get<EnumMember>(d->members[i])->name, d->members[i], false, i + 1);
            }
            for (std::size_t i = 0; i < d->members.size(); i++) {
                walk_children(inner, d->members[i], i + 1);
            }
            return;
        }

        // if / match。条件は自分のスコープ、各分岐の本体はその内側。
        if (auto cond = n.as_any<ConditionalExpr>()) {
            Env inner{&env, position, false, {}};
            if (auto m = n.as_any<Match>()) {
                walk(inner, a.get<Match>(m)->condition, position);
            }
            for (auto& block : a.get<ConditionalExpr>(cond)->blocks) {
                auto* b = a.get<BodyStatement>(block);
                if (auto cs = block.template as_any<ConditionalStatement>()) {
                    walk(inner, a.get<ConditionalStatement>(cs)->condition, position);
                }
                Env body_env{&inner, position, false, {}};
                run_block(body_env, b->body, 0);
            }
            return;
        }

        // for。init の宣言が cond / step / 本体から見える。
        if (auto loop = n.as_any<Loop>()) {
            auto* d = a.get<Loop>(loop);
            Env inner{&env, position, false, {}};
            if (d->init) {
                if (auto named = d->init.template as_any<NamedStatement>()) {
                    declare(inner, a.get<NamedStatement>(named)->name, d->init, false, 0);
                }
                walk(inner, d->init, 1);
            }
            walk(inner, d->condition, 1);
            walk(inner, d->step, 1);
            Env body_env{&inner, 1, false, {}};
            run_block(body_env, d->body, 0);
            return;
        }

        // for x in expr。x はループの中だけ、expr は外側で解決する。
        if (auto rloop = n.as_any<RangeLoop>()) {
            auto* d = a.get<RangeLoop>(rloop);
            walk(env, d->container, position);
            Env inner{&env, position, false, {}};
            declare(inner, d->bind_variable, rloop, false, 0);
            run_block(inner, d->body, 0);
            return;
        }

        walk_children(env, n, position);
    }

    void ScopeResolver::resolve(Node<Module> mod) {
        auto* d = a.get<Module>(mod);
        if (!d) {
            return;
        }
        Env global{nullptr, 0, false, {}};
        global_ = &global;
        collect(global, d->statements, 0);
        for (std::size_t i = 0; i < d->statements.size(); i++) {
            walk(global, d->statements[i], i + 1);
        }
        global_ = nullptr;
    }

}  // namespace brgen::nast::bind
