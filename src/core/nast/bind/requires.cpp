/*license*/
#include "requires.hpp"

#include "../traverse.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace brgen::nast::bind {

    namespace {

        // 片方向ぶんの要求。表の Requirements は decode / encode の 2 組を
        // 平らに持つが、推論の中では方向ごとにこれで持つ。Function は方向を
        // 持たないので dec 側を正準として使う (呼ばれた方向で要る)。
        struct Caps {
            bool peek = false;
            bool backward = false;
            bool remain = false;
            bool offset = false;
            std::vector<Node<StateVariable>> state_read;
            std::vector<Node<StateVariable>> state_write;
        };

        bool add_state(std::vector<Node<StateVariable>>& v, Node<StateVariable> s) {
            if (std::find(v.begin(), v.end(), s) == v.end()) {
                v.push_back(s);
                return true;
            }
            return false;
        }

        bool merge_state(Caps& into, const Caps& from) {
            bool changed = false;
            for (auto& s : from.state_read) {
                changed = add_state(into.state_read, s) || changed;
            }
            for (auto& s : from.state_write) {
                changed = add_state(into.state_write, s) || changed;
            }
            return changed;
        }

        bool merge_caps(Caps& into, const Caps& from, bool discharge_remain) {
            bool changed = (from.peek && !into.peek) || (from.backward && !into.backward) ||
                           (from.offset && !into.offset) ||
                           (!discharge_remain && from.remain && !into.remain);
            into.peek = into.peek || from.peek;
            into.backward = into.backward || from.backward;
            if (!discharge_remain) {
                into.remain = into.remain || from.remain;
            }
            into.offset = into.offset || from.offset;
            return changed;
        }

        // 依存辺。取り込み方は文書 (docs/requires_direction.md) の段階 1 の規則。
        enum class EdgeKind {
            field,          // field :Format。方向を保って伝播 (dec→dec / enc→enc)
            inline_read,    // input.get(Format)。復号がこの場で走る。dec→dec のみ
            fn_from_format, // as_is body からの fn 呼び出し。能力は dec へ、state は両方向へ
            fn_from_fn,     // fn からの fn 呼び出し。正準 (dec) どうし
            encode_custom,  // encode を差し替えた fn。正準を enc へ
            decode_custom,  // decode を差し替えた fn。正準を dec へ
        };

        struct Edge {
            std::uint32_t target = 0;
            EdgeKind kind = EdgeKind::field;
            // field の引数で input = input.subrange(len) と長さを確立している。
            // 呼び先の remain (dec 側) はその場で満たされて伝播しない。
            bool discharge_remain = false;
        };

        struct Local {
            Caps dec;
            Caps enc;
            std::vector<Edge> edges;
        };

    }  // namespace

    namespace {

        struct Collector {
            Arena& a;
            SideTables& tables;
            Typer& typer;
            Node<Statement> owner;
            Local& out;
            // Format の as_is body か、Function の本体か。stream 能力の直接使用は
            // どちらでも dec へ入れる (fn は dec が正準)。state は as_is body の
            // 文が両方向で実行されるため、Format のときだけ両側に入れる。
            bool owner_is_format = false;

            // 代入の左辺の根にある参照。読みとして数えないため覚えておく。
            std::set<std::uint32_t> assign_lhs_refs;

            Node<StateVariable> state_target(Node<Ident> name) {
                if (auto* r = tables.table<Resolution>().get(name)) {
                    if (auto sv = r->target.as_any<StateVariable>()) {
                        return sv;
                    }
                }
                return nullref;
            }

            // 代入の左辺 (sstate.isA = .. / arr[i] = ..) の根の参照を剥がす。
            Node<Ident> lhs_root_name(Node<Expr> e) {
                for (;;) {
                    if (auto ma = e.as_any<MemberAccess>()) {
                        e = ma.ref(a)->base;
                        continue;
                    }
                    if (auto idx = e.as_any<Index>()) {
                        e = idx.ref(a)->base;
                        continue;
                    }
                    if (auto p = e.as_any<Paren>()) {
                        e = p.ref(a)->expr;
                        continue;
                    }
                    break;
                }
                if (auto ref = e.as_any<Reference>()) {
                    return ref.ref(a)->name;
                }
                return nullref;
            }

            // field の型の先にいる format。配列と包みを剥がして struct の持ち主を見る。
            Node<Format> format_of_type(Node<Type> t) {
                for (;;) {
                    if (auto arr = t.as_any<ArrayType>()) {
                        t = arr.ref(a)->element_type;
                        continue;
                    }
                    break;
                }
                auto st = typer.as_struct(t);
                if (!st) {
                    return nullref;
                }
                return st.ref(a)->base.as_any<Format>();
            }

            void note_state_write(Node<StateVariable> sv) {
                add_state(out.dec.state_write, sv);
                if (owner_is_format) {
                    add_state(out.enc.state_write, sv);
                }
            }

            void note_state_read(Node<StateVariable> sv) {
                add_state(out.dec.state_read, sv);
                if (owner_is_format) {
                    add_state(out.enc.state_read, sv);
                }
            }

            void note_write(Node<Expr> lhs) {
                if (auto name = lhs_root_name(lhs)) {
                    if (auto sv = state_target(name)) {
                        note_state_write(sv);
                        assign_lhs_refs.insert(name.id());
                    }
                }
            }

            // input = input.subrange(len) の形で長さを確立したストリームを渡して
            // いるか。渡していれば呼び先の remain はここで満たされる。
            bool rebinds_input_with_length(Node<Arguments> args) {
                auto d = args.ref(a);
                if (!d) {
                    return false;
                }
                for (auto& arg : d->arguments) {
                    auto na = arg.as_any<NamedArgument>();
                    if (!na) {
                        continue;
                    }
                    auto nd = na.ref(a);
                    auto sp = nd->name.as_any<SpecialLiteral>();
                    if (!sp || sp.ref(a)->kind != SpecialLiteralKind::input_) {
                        continue;
                    }
                    if (auto stream = typer.type_of_expr(nd->value).as_any<StreamType>()) {
                        if (stream.ref(a)->length) {
                            return true;
                        }
                    }
                }
                return false;
            }

            void member_access(Node<MemberAccess> ma) {
                auto d = ma.ref(a);
                auto stream = typer.type_of_expr(d->base).as_any<StreamType>();
                if (!stream || stream.ref(a)->kind != SpecialLiteralKind::input_) {
                    return;
                }
                auto member = d->member.ref(a);
                if (!member) {
                    return;
                }
                // 入力能力はどれも dec 側。as_is body の encode がこれらを
                // 要求しないのは段階 1 の宣言 (docs/requires_direction.md)。
                auto& name = member->identifier;
                if (name == "peek") {
                    out.dec.peek = true;
                }
                else if (name == "backward") {
                    out.dec.backward = true;
                }
                else if (name == "remain" || name == "scope_length") {
                    // 長さを確立したストリーム (subrange の値) 相手ならその場で
                    // 満たされている。要求は外に出ない。
                    if (!stream.ref(a)->length) {
                        out.dec.remain = true;
                    }
                }
                else if (name == "offset" || name == "bit_offset") {
                    out.dec.offset = true;
                }
            }

            void call(Node<Call> c) {
                auto d = c.ref(a);
                // input.get(Format) / input.peek(Format): その format の復号が
                // この場のストリームで走る。dec 側だけへ取り込む。
                if (auto ma = d->callee.as_any<MemberAccess>()) {
                    auto md = ma.ref(a);
                    if (typer.type_of_expr(md->base).as_any<StreamType>()) {
                        if (auto args = d->arguments.ref(a); args && !args->arguments.empty()) {
                            auto v = args->arguments.front().ref(a)->value;
                            if (auto ref = v.as_any<Reference>()) {
                                if (auto* r = tables.table<Resolution>().get(ref.ref(a)->name)) {
                                    if (auto fmt = r->target.as_any<Format>()) {
                                        out.edges.push_back({fmt.id(), EdgeKind::inline_read, false});
                                    }
                                }
                            }
                        }
                        return;
                    }
                }
                // fn 呼び出し。解決先の関数の要求を取り込む。
                Node<Ident> callee_name;
                if (auto ref = d->callee.as_any<Reference>()) {
                    callee_name = ref.ref(a)->name;
                }
                else if (auto ma = d->callee.as_any<MemberAccess>()) {
                    callee_name = ma.ref(a)->member;
                }
                if (callee_name) {
                    if (auto* r = tables.table<Resolution>().get(callee_name)) {
                        if (auto fn = r->target.as_any<Function>()) {
                            out.edges.push_back({fn.id(),
                                                 owner_is_format ? EdgeKind::fn_from_format : EdgeKind::fn_from_fn,
                                                 false});
                        }
                    }
                }
            }

            void run() {
                visit_all(a, owner, [&](NodeAny n) {
                    if (n.id() != owner.id() &&
                        (n.as_any<Format>() || n.as_any<Function>() || n.as_any<Enum>() || n.as_any<State>())) {
                        // 入れ子の owner は自分の項を持つ。参照されたときに辺で届く。
                        return false;
                    }
                    // 代入は文の位置では Assign 文、式の位置では Binary。どちらも
                    // 左辺の根が state 変数なら書き込み。
                    if (auto as = n.as_any<Assign>()) {
                        note_write(as.ref(a)->assignee);
                    }
                    else if (auto bin = n.as_any<Binary>()) {
                        auto d = bin.ref(a);
                        if (is_assign_op(d->op)) {
                            note_write(d->left);
                        }
                    }
                    else if (auto ref = n.as_any<Reference>()) {
                        auto name = ref.ref(a)->name;
                        if (!assign_lhs_refs.contains(name.id())) {
                            if (auto sv = state_target(name)) {
                                note_state_read(sv);
                            }
                        }
                    }
                    else if (auto ma = n.as_any<MemberAccess>()) {
                        member_access(ma);
                    }
                    else if (auto c = n.as_any<Call>()) {
                        call(c);
                    }
                    else if (auto f = n.as_any<Field>()) {
                        auto d = f.ref(a);
                        if (auto fmt = format_of_type(d->type)) {
                            out.edges.push_back({fmt.id(), EdgeKind::field, rebinds_input_with_length(d->arguments)});
                        }
                    }
                    return true;
                });
                // encode / decode を fn で差し替えている format は、その fn が
                // その方向の codec 本体。要求もその方向だけに取り込む。
                if (auto fmt = owner.as_any<Format>()) {
                    if (auto* st = tables.table<FormatState>().get(fmt)) {
                        if (st->encode_custom) {
                            out.edges.push_back({st->encode_custom.id(), EdgeKind::encode_custom, false});
                        }
                        if (st->decode_custom) {
                            out.edges.push_back({st->decode_custom.id(), EdgeKind::decode_custom, false});
                        }
                    }
                }
            }
        };

    }  // namespace

    void RequiresInference::run(const std::vector<Node<Module>>& modules) {
        // 1. owner (Format / Function) を集めて、それぞれの局所要求と依存辺を出す。
        std::map<std::uint32_t, Node<Statement>> owners;
        for (auto& mod : modules) {
            visit_all(a, mod, [&](NodeAny n) {
                if (auto fmt = n.as_any<Format>()) {
                    owners.emplace(fmt.id(), fmt);
                }
                else if (auto fn = n.as_any<Function>()) {
                    owners.emplace(fn.id(), fn);
                }
                return true;
            });
        }
        std::map<std::uint32_t, Local> locals;
        for (auto& [id, owner] : owners) {
            Collector c{a, tables, typer, owner, locals[id]};
            c.owner_is_format = bool(owner.as_any<Format>());
            c.run();
        }

        // 2. 不動点。要求は単調に増えるだけなので、変化が無くなるまで回す。
        //    再帰 format の循環もこれで収束する。辺の種類ごとの取り込み方が
        //    段階 1 の規則そのもの。
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& [id, local] : locals) {
                for (auto& e : local.edges) {
                    auto it = locals.find(e.target);
                    if (it == locals.end()) {
                        continue;
                    }
                    auto& dep = it->second;
                    switch (e.kind) {
                        case EdgeKind::field:
                            // 子 format の decode の都合は親の decode に、
                            // 子の encode の都合 (custom encode fn 由来) は親の encode に。
                            changed = merge_caps(local.dec, dep.dec, e.discharge_remain) || changed;
                            changed = merge_state(local.dec, dep.dec) || changed;
                            changed = merge_caps(local.enc, dep.enc, false) || changed;
                            changed = merge_state(local.enc, dep.enc) || changed;
                            break;
                        case EdgeKind::inline_read:
                            changed = merge_caps(local.dec, dep.dec, false) || changed;
                            changed = merge_state(local.dec, dep.dec) || changed;
                            break;
                        case EdgeKind::fn_from_format:
                            // fn の文は両方向で実行されるが、入力能力は decode の
                            // 性質なので dec だけへ。state は両方向へ。
                            changed = merge_caps(local.dec, dep.dec, false) || changed;
                            changed = merge_state(local.dec, dep.dec) || changed;
                            changed = merge_state(local.enc, dep.dec) || changed;
                            break;
                        case EdgeKind::fn_from_fn:
                            changed = merge_caps(local.dec, dep.dec, false) || changed;
                            changed = merge_state(local.dec, dep.dec) || changed;
                            break;
                        case EdgeKind::encode_custom:
                            changed = merge_caps(local.enc, dep.dec, false) || changed;
                            changed = merge_state(local.enc, dep.dec) || changed;
                            break;
                        case EdgeKind::decode_custom:
                            changed = merge_caps(local.dec, dep.dec, false) || changed;
                            changed = merge_state(local.dec, dep.dec) || changed;
                            break;
                    }
                }
            }
        }

        // 3. 表へ。空でも「計測済み」の印として置く。Function は方向を持たない
        //    ので、正準 (dec) を両側に写す — 呼ばれた方向でその能力が要る。
        for (auto& [id, owner] : owners) {
            auto& local = locals[id];
            if (owner.as_any<Function>()) {
                local.enc = local.dec;
            }
            auto fill = [](Caps& c) {
                std::sort(c.state_read.begin(), c.state_read.end());
                std::sort(c.state_write.begin(), c.state_write.end());
            };
            fill(local.dec);
            fill(local.enc);
            Requirements req;
            req.decode_peek = local.dec.peek;
            req.decode_backward = local.dec.backward;
            req.decode_remain = local.dec.remain;
            req.decode_offset = local.dec.offset;
            req.decode_state_read = std::move(local.dec.state_read);
            req.decode_state_write = std::move(local.dec.state_write);
            req.encode_peek = local.enc.peek;
            req.encode_backward = local.enc.backward;
            req.encode_remain = local.enc.remain;
            req.encode_offset = local.enc.offset;
            req.encode_state_read = std::move(local.enc.state_read);
            req.encode_state_write = std::move(local.enc.state_write);
            tables.table<Requirements>().set(owner, std::move(req));
            inferred++;
        }
    }

}  // namespace brgen::nast::bind
