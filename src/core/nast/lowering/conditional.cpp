/*license*/
#include "conditional.hpp"
#include "../node/util.h"

#include <format>

namespace brgen::nast::lowering {

    namespace {
        // tmpN への代入 1 つを body に持つ分岐を作る。condition が null なら
        // 既定の分岐 = 素の BodyStatement。parse.cpp が else をそう作っていて
        // (parse_if の `a.make<BodyStatement>`)、binder の cond_of も
        // 「ConditionalStatement でなければ既定」と読んでいる。条件なしの
        // ConditionalStatement にすると unparse が `elif /*missing*/` を出す。
        Node<BodyStatement> make_branch(Context& c, lexer::Loc loc, Node<Expr> condition,
                                        Node<Ident> temp_name, Node<Expr> value) {
            auto& a = c.a;
            auto assignee = a.make<Reference>(loc);
            assignee->name = temp_name;
            assignee->type = value ? value.ref(a)->type : nullref;

            auto assign = a.make<Assign>(loc);
            assign->assignee = assignee;
            assign->value = value;
            assign->op = BinaryOp::assign;

            auto body = a.make<Body>(loc);
            body->statements.push_back(assign);

            if (!condition) {
                auto branch = a.make<BodyStatement>(loc);
                branch->body = body;
                return branch;
            }
            auto branch = a.make<ConditionalStatement>(loc);
            branch->condition = condition;
            branch->body = body;
            return branch;
        }
    }  // namespace

    LoweredCond* lower_conditional(Context& c, Node<Cond> cond) {
        if (!cond) {
            return nullptr;
        }
        if (auto* got = c.tables.table<LoweredCond>().get(cond)) {
            return got;  // 2 度目は同じノードを返す
        }
        auto& a = c.a;
        auto d = cond.ref(a);
        auto loc = cond.ref(a).loc();

        // 一時変数の型は三項自身の型。typer が then/els の共通型を入れている。
        auto type = d->type;
        if (!type) {
            return nullptr;  // 型が付いていない (壊れた入力)。作らない。
        }

        // 名前は由来のノード番号から。同じ木の中で衝突しない。
        auto temp_name = a.make<Ident>(loc);
        temp_name->identifier = derived_name("tmp", cond);

        auto then_branch = make_branch(c, loc, d->cond, temp_name, d->then);
        auto else_branch = make_branch(c, loc, nullref, temp_name, d->els);

        auto branch = a.make<If>(loc);
        branch->type = type;
        branch->blocks.push_back(then_branch);
        branch->blocks.push_back(else_branch);

        auto value = a.make<Reference>(loc);
        value->name = temp_name;
        value->type = type;

        LoweredCond lowered;
        lowered.temp_name = temp_name;
        lowered.type = type;
        lowered.branch = branch;
        lowered.value = value;
        c.tables.table<LoweredCond>().set(cond, std::move(lowered));
        return c.tables.table<LoweredCond>().get(cond);
    }

}  // namespace brgen::nast::lowering
