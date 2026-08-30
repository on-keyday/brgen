/*license*/
#include "match_to_if.hpp"
#include "predicate.hpp"
#include "../node/util.h"

namespace brgen::nast::lowering {


    Node<If> lower_match(Context& c, Node<Match> match) {
        if (!match) {
            return nullref;
        }
        if (auto* got = c.tables.table<LoweredMatch>().get(match)) {
            return got->branch;  // 2 度目は同じノードを返す
        }
        auto& a = c.a;
        auto d = match.ref(a);
        auto loc = match.ref(a).loc();

        auto if_ = a.make<If>(loc);
        if_->type = d->type;
        for (auto& block : d->blocks) {
            auto cs = block.as_any<ConditionalStatement>();
            // 既定の分岐 (`..`) は条件なしの BodyStatement にする。else の
            // 表し方は parse.cpp の if がそうしている形に合わせる。
            if (!cs || is_default_cond(a, cs.ref(a)->condition)) {
                auto els = a.make<BodyStatement>(block.ref(a).loc());
                els->belong = if_;
                els->body = block.ref(a)->body;
                if_->blocks.push_back(els);
                continue;
            }
            auto branch = a.make<ConditionalStatement>(block.ref(a).loc());
            branch->belong = if_;
            branch->condition = branch_predicate(c, d->condition, cs.ref(a)->condition);
            branch->body = block.ref(a)->body;
            if_->blocks.push_back(branch);
        }

        LoweredMatch lowered;
        lowered.branch = if_;
        c.tables.table<LoweredMatch>().set(match, std::move(lowered));
        return if_;
    }

}  // namespace brgen::nast::lowering
