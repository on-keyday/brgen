/*license*/
#include "requires.hpp"

#include "../traverse.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace brgen::nast::bind {

    namespace {

        // 依存辺。dependee の要求を取り込むとき、discharge_remain なら remain を
        // 落とす (呼び出し側が長さを確立したストリームを渡している)。
        struct Edge {
            std::uint32_t target = 0;
            bool discharge_remain = false;
        };

        struct Local {
            Requirements req;
            std::vector<Edge> edges;
        };

        void add_state(std::vector<Node<StateVariable>>& v, Node<StateVariable> s) {
            if (std::find(v.begin(), v.end(), s) == v.end()) {
                v.push_back(s);
            }
        }

        void merge_state(std::vector<Node<StateVariable>>& into, const std::vector<Node<StateVariable>>& from) {
            for (auto& s : from) {
                add_state(into, s);
            }
        }

    }  // namespace

    namespace {

        struct Collector {
            Arena& a;
            SideTables& tables;
            Typer& typer;
            Node<Statement> owner;
            Local& out;

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
                        e = a.get<MemberAccess>(ma)->base;
                        continue;
                    }
                    if (auto idx = e.as_any<Index>()) {
                        e = a.get<Index>(idx)->base;
                        continue;
                    }
                    if (auto p = e.as_any<Paren>()) {
                        e = a.get<Paren>(p)->expr;
                        continue;
                    }
                    break;
                }
                if (auto ref = e.as_any<Reference>()) {
                    return a.get<Reference>(ref)->name;
                }
                return nullref;
            }

            // field の型の先にいる format。配列と包みを剥がして struct の持ち主を見る。
            Node<Format> format_of_type(Node<Type> t) {
                for (;;) {
                    if (auto arr = t.as_any<ArrayType>()) {
                        t = a.get<ArrayType>(arr)->element_type;
                        continue;
                    }
                    break;
                }
                auto st = typer.as_struct(t);
                if (!st) {
                    return nullref;
                }
                return a.get<StructType>(st)->base.as_any<Format>();
            }

            void note_write(Node<Expr> lhs) {
                if (auto name = lhs_root_name(lhs)) {
                    if (auto sv = state_target(name)) {
                        add_state(out.req.state_write, sv);
                        assign_lhs_refs.insert(name.id());
                    }
                }
            }

            // input = input.subrange(len) の形で長さを確立したストリームを渡して
            // いるか。渡していれば呼び先の remain はここで満たされる。
            bool rebinds_input_with_length(Node<Arguments> args) {
                auto* d = a.get<Arguments>(args);
                if (!d) {
                    return false;
                }
                for (auto& arg : d->arguments) {
                    auto na = arg.as_any<NamedArgument>();
                    if (!na) {
                        continue;
                    }
                    auto* nd = a.get<NamedArgument>(na);
                    auto sp = nd->name.as_any<SpecialLiteral>();
                    if (!sp || a.get<SpecialLiteral>(sp)->kind != SpecialLiteralKind::input_) {
                        continue;
                    }
                    if (auto stream = typer.type_of_expr(nd->value).as_any<StreamType>()) {
                        if (a.get<StreamType>(stream)->length) {
                            return true;
                        }
                    }
                }
                return false;
            }

            void member_access(Node<MemberAccess> ma) {
                auto* d = a.get<MemberAccess>(ma);
                auto stream = typer.type_of_expr(d->base).as_any<StreamType>();
                if (!stream || a.get<StreamType>(stream)->kind != SpecialLiteralKind::input_) {
                    return;
                }
                auto* member = a.get<Ident>(d->member);
                if (!member) {
                    return;
                }
                auto& name = member->identifier;
                if (name == "peek") {
                    out.req.peek = true;
                }
                else if (name == "backward") {
                    out.req.backward = true;
                }
                else if (name == "remain" || name == "scope_length") {
                    // 長さを確立したストリーム (subrange の値) 相手ならその場で
                    // 満たされている。要求は外に出ない。
                    if (!a.get<StreamType>(stream)->length) {
                        out.req.remain = true;
                    }
                }
                else if (name == "offset" || name == "bit_offset") {
                    out.req.offset = true;
                }
            }

            void call(Node<Call> c) {
                auto* d = a.get<Call>(c);
                // input.get(Format) / input.peek(Format): その format の復号が
                // この場のストリームで走る。要求は素通しで取り込む。
                if (auto ma = d->callee.as_any<MemberAccess>()) {
                    auto* md = a.get<MemberAccess>(ma);
                    if (typer.type_of_expr(md->base).as_any<StreamType>()) {
                        if (auto* args = a.get<Arguments>(d->arguments); args && !args->arguments.empty()) {
                            auto v = a.get<Argument>(args->arguments.front())->value;
                            if (auto ref = v.as_any<Reference>()) {
                                if (auto* r = tables.table<Resolution>().get(a.get<Reference>(ref)->name)) {
                                    if (auto fmt = r->target.as_any<Format>()) {
                                        out.edges.push_back({fmt.id(), false});
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
                    callee_name = a.get<Reference>(ref)->name;
                }
                else if (auto ma = d->callee.as_any<MemberAccess>()) {
                    callee_name = a.get<MemberAccess>(ma)->member;
                }
                if (callee_name) {
                    if (auto* r = tables.table<Resolution>().get(callee_name)) {
                        if (auto fn = r->target.as_any<Function>()) {
                            out.edges.push_back({fn.id(), false});
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
                        note_write(a.get<Assign>(as)->assignee);
                    }
                    else if (auto bin = n.as_any<Binary>()) {
                        auto* d = a.get<Binary>(bin);
                        if (is_assign_op(d->op)) {
                            note_write(d->left);
                        }
                    }
                    else if (auto ref = n.as_any<Reference>()) {
                        auto name = a.get<Reference>(ref)->name;
                        if (!assign_lhs_refs.contains(name.id())) {
                            if (auto sv = state_target(name)) {
                                add_state(out.req.state_read, sv);
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
                        auto* d = a.get<Field>(f);
                        if (auto fmt = format_of_type(d->type)) {
                            out.edges.push_back({fmt.id(), rebinds_input_with_length(d->arguments)});
                        }
                    }
                    return true;
                });
                // encode / decode を fn で差し替えている format は、その fn が
                // codec 本体。要求も format のものとして取り込む。
                if (auto fmt = owner.as_any<Format>()) {
                    if (auto* st = tables.table<FormatState>().get(fmt)) {
                        if (st->encode_custom) {
                            out.edges.push_back({st->encode_custom.id(), false});
                        }
                        if (st->decode_custom) {
                            out.edges.push_back({st->decode_custom.id(), false});
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
            c.run();
        }

        // 2. 不動点。要求は単調に増えるだけなので、変化が無くなるまで回す。
        //    再帰 format の循環もこれで収束する。
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& [id, local] : locals) {
                for (auto& e : local.edges) {
                    auto it = locals.find(e.target);
                    if (it == locals.end()) {
                        continue;
                    }
                    auto& dep = it->second.req;
                    auto& req = local.req;
                    auto before_peek = req.peek;
                    auto before_backward = req.backward;
                    auto before_remain = req.remain;
                    auto before_offset = req.offset;
                    auto before_reads = req.state_read.size();
                    auto before_writes = req.state_write.size();
                    req.peek = req.peek || dep.peek;
                    req.backward = req.backward || dep.backward;
                    if (!e.discharge_remain) {
                        req.remain = req.remain || dep.remain;
                    }
                    req.offset = req.offset || dep.offset;
                    merge_state(req.state_read, dep.state_read);
                    merge_state(req.state_write, dep.state_write);
                    changed = changed || before_peek != req.peek || before_backward != req.backward ||
                              before_remain != req.remain || before_offset != req.offset ||
                              before_reads != req.state_read.size() || before_writes != req.state_write.size();
                }
            }
        }

        // 3. 表へ。空でも「計測済み」の印として置く。
        for (auto& [id, owner] : owners) {
            auto& req = locals[id].req;
            std::sort(req.state_read.begin(), req.state_read.end());
            std::sort(req.state_write.begin(), req.state_write.end());
            tables.table<Requirements>().set(owner, std::move(req));
            inferred++;
        }
    }

}  // namespace brgen::nast::bind
