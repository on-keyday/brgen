/*license*/
#include "control_flow_graph.hpp"
#include <memory>
#include "code/code_writer.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/mapping.hpp"
#include <optional>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ebmgen {
    struct InternalCFGContext {
        CFGStack& stack;
        RepositoryProxy proxy;
    };

    expected<CFGTuple> analyze_ref(InternalCFGContext& tctx, ebm::StatementRef ref);

    expected<std::shared_ptr<CFGExpression>> analyze_expression(InternalCFGContext& tctx, ebm::ExpressionRef ref) {
        auto expr = std::make_shared<CFGExpression>();
        expr->original_node = ref;
        MAYBE(expr_v, tctx.proxy.get_expression(ref));
        std::vector<std::pair<std::string_view, ebm::ExpressionRef>> children;
        expr_v.body.visit([&](auto&& visitor, const char* name, auto&& value) -> void {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ebm::ExpressionRef>) {
                if (!is_nil(value)) {
                    children.push_back({name, value});
                }
            }
            else if constexpr (std::is_same_v<T, ebm::LoweredExpressionRef> || std::is_same_v<T, ebm::LoweredStatementRef>) {
                // ignore
            }
            else
                VISITOR_RECURSE_CONTAINER(visitor, name, value)
            else VISITOR_RECURSE(visitor, name, value)
        });
        for (auto& child : children) {
            MAYBE(child_expr, analyze_expression(tctx, child.second));
            child_expr->parent = expr;
            child_expr->relation_name = child.first;
            expr->children.push_back(std::move(child_expr));
        }
        if (auto w = expr_v.body.io_statement()) {
            MAYBE(related_cfg, analyze_ref(tctx, *w));
            expr->related_cfg = std::move(related_cfg);
        }
        else if (auto v = expr_v.body.conditional_stmt()) {
            MAYBE(related_cfg, analyze_ref(tctx, *v));
            expr->related_cfg = std::move(related_cfg);
        }
        return expr;
    }

    expected<std::vector<CFGTuple>> analyze_lowered(InternalCFGContext& tctx, ebm::StatementRef ref) {
        MAYBE(stmt, tctx.proxy.get_statement(ref));
        std::vector<CFGTuple> lowered;
        if (auto lw = stmt.body.lowered_io_statements()) {
            for (auto& r : lw->container) {
                MAYBE(result, analyze_ref(tctx, r.io_statement.id));
                lowered.push_back(std::move(result));
            }
        }
        else {
            MAYBE(result, analyze_ref(tctx, ref));
            lowered.push_back(std::move(result));
        }
        return lowered;
    }

    expected<CFGTuple> analyze_ref(InternalCFGContext& tctx, ebm::StatementRef ref) {
        auto root = std::make_shared<CFG>();
        root->original_node = ref;
        auto current = root;
        auto link = [](auto& from, auto& to) {
            from->next.push_back(to);
            to->prev.push_back(from);
        };
        MAYBE(stmt, tctx.proxy.get_statement(ref));
        root->statement_op = stmt.body.kind;
        tctx.stack.cfg_map[get_id(ref)] = root;
        bool brk = false;
        if (auto block = stmt.body.block()) {
            auto join = std::make_shared<CFG>();
            for (auto& ref : block->container) {
                MAYBE(child, analyze_ref(tctx, ref));
                link(current, child.start);
                if (!child.brk) {
                    link(child.end, join);
                    current = std::move(join);
                    join = std::make_shared<CFG>();
                    continue;
                }
                current = child.end;
                brk = true;
                break;
            }
        }
        else if (auto if_stmt = stmt.body.if_statement()) {
            MAYBE(then_block, analyze_ref(tctx, if_stmt->then_block));
            MAYBE(cond, analyze_expression(tctx, if_stmt->condition.cond));
            then_block.start->condition = std::move(cond);
            auto join = std::make_shared<CFG>();
            link(current, then_block.start);
            if (!then_block.brk) {
                link(then_block.end, join);
            }
            if (get_id(if_stmt->else_block)) {
                MAYBE(else_block, analyze_ref(tctx, if_stmt->else_block));
                link(current, else_block.start);
                if (!else_block.brk) {
                    link(else_block.end, join);
                }
                brk = then_block.brk && else_block.brk;
            }
            else {
                link(current, join);
            }
            current = std::move(join);
        }
        else if (auto loop_ = stmt.body.loop()) {
            if (auto cond = loop_->condition()) {
                MAYBE(cond_node, analyze_expression(tctx, cond->cond));
                current->condition = std::move(cond_node);
            }
            auto join = std::make_shared<CFG>();
            tctx.stack.loop_stack.push_back(CFGTuple{current, join});
            MAYBE(body, analyze_ref(tctx, loop_->body));
            tctx.stack.loop_stack.pop_back();
            link(current, body.start);
            if (loop_->loop_type != ebm::LoopType::INFINITE) {
                link(current, join);
            }
            if (!body.brk) {
                link(body.end, current);
            }
            current = std::move(join);
        }
        else if (auto match_ = stmt.body.match_statement()) {
            auto join = std::make_shared<CFG>();
            bool all_break = true;
            for (auto& b : match_->branches.container) {
                MAYBE(branch_stmt, tctx.proxy.get_statement(b));
                MAYBE(branch_ptr, branch_stmt.body.match_branch());
                MAYBE(branch, analyze_ref(tctx, branch_ptr.body));
                MAYBE(cond, analyze_expression(tctx, branch_ptr.condition.cond));
                branch.start->condition = std::move(cond);
                link(current, branch.start);
                if (!branch.brk) {
                    link(branch.end, join);
                }
                all_break = all_break && branch.brk;
            }
            if (!match_->is_exhaustive()) {
                link(current, join);
            }
            brk = all_break;
            current = std::move(join);
        }
        else if (auto cont = stmt.body.continue_()) {
            if (tctx.stack.loop_stack.size() == 0) {
                return unexpect_error("Continue outside of loop");
            }
            link(current, tctx.stack.loop_stack.back().start);
            brk = true;
        }
        else if (auto brk_ = stmt.body.break_()) {
            if (tctx.stack.loop_stack.size() == 0) {
                return unexpect_error("Break outside of loop");
            }
            link(current, tctx.stack.loop_stack.back().end);
            brk = true;
        }
        else if (stmt.body.kind == ebm::StatementKind::RETURN ||
                 stmt.body.kind == ebm::StatementKind::ERROR_RETURN ||
                 stmt.body.kind == ebm::StatementKind::ERROR_REPORT) {
            if (stmt.body.kind != ebm::StatementKind::ERROR_REPORT) {
                if (stmt.body.value()->id.value() != 0) {
                    MAYBE(expr_node, analyze_expression(tctx, *stmt.body.value()));
                    current->condition = std::move(expr_node);
                }
            }
            link(current, tctx.stack.end_of_function);
            brk = true;
        }
        else if (stmt.body.kind == ebm::StatementKind::READ_DATA ||
                 stmt.body.kind == ebm::StatementKind::WRITE_DATA) {
            auto io_ = stmt.body.read_data() ? stmt.body.read_data() : stmt.body.write_data();
            if (auto lw = io_->lowered_statement()) {
                MAYBE(r, analyze_lowered(tctx, lw->io_statement.id));
                current->lowered = std::move(r);
            }
            if (!is_nil(io_->target)) {
                MAYBE(expr_node, analyze_expression(tctx, io_->target));
                current->condition = expr_node;
            }
        }
        else if (auto var_decl = stmt.body.var_decl()) {
            MAYBE(expr_node, analyze_expression(tctx, var_decl->initial_value));
            current->condition = expr_node;
        }
        else if (auto expr = stmt.body.expression()) {
            MAYBE(expr_node, analyze_expression(tctx, *expr));
            current->condition = std::move(expr_node);
        }
        else if (auto assert_ = stmt.body.assert_desc()) {
            MAYBE(expr_node, analyze_expression(tctx, assert_->condition.cond));
            current->condition = std::move(expr_node);
        }
        else if (auto desc = stmt.body.sub_byte_range()) {
            MAYBE(range, analyze_ref(tctx, desc->io_statement));
            link(current, range.start);
            current = range.end;
            brk = range.brk;
        }
        return CFGTuple{root, current, brk};
    }
    struct OptimizeContext {
        std::map<std::shared_ptr<CFG>, std::set<std::shared_ptr<CFG>>> per_roots;
        std::set<std::shared_ptr<CFG>> all_cfg;
        std::set<std::shared_ptr<CFG>>* current_root = nullptr;

        void with_root(std::shared_ptr<CFG>& cfg, auto&& fn) {
            auto tmp = current_root;
            const auto _defer = futils::helper::defer([&] {
                current_root = tmp;
            });
            current_root = &per_roots[cfg];
            fn();
        }
    };

    std::shared_ptr<CFG> optimize_cfg_node(std::shared_ptr<CFG>& cfg, OptimizeContext& ctx);

    std::shared_ptr<CFGExpression> optimize_cfg_expression(std::shared_ptr<CFGExpression>& cfg, OptimizeContext& ctx) {
        for (auto& child : cfg->children) {
            child = optimize_cfg_expression(child, ctx);
        }
        if (cfg->related_cfg) {
            ctx.with_root(cfg->related_cfg->start, [&] {
                cfg->related_cfg->start = optimize_cfg_node(cfg->related_cfg->start, ctx);
            });
        }
        return cfg;
    }

    void unique(std::shared_ptr<CFG>& cfg) {
        std::unordered_set<std::shared_ptr<CFG>> uniq;
        std::erase_if(cfg->next, [&](auto& q) {
            return !uniq.insert(q).second;
        });
        uniq.clear();
        std::erase_if(cfg->prev, [&](auto& q) {
            return !uniq.insert(q.lock()).second;
        });
    }

    // remove <phi> node (not related to original node)
    std::shared_ptr<CFG> optimize_cfg_node(std::shared_ptr<CFG>& cfg, OptimizeContext& ctx) {
        if (ctx.all_cfg.find(cfg) != ctx.all_cfg.end()) {
            return cfg;
        }
        unique(cfg);
        if (cfg->prev.size() && cfg->next.size() == 1 && is_nil(cfg->original_node)) {
            auto rem = cfg;
            for (auto& prev : rem->prev) {
                if (auto p = prev.lock()) {
                    for (auto& n : p->next) {
                        if (n == cfg) {
                            n = rem->next[0];
                        }
                    }
                }
            }
            std::erase_if(rem->next[0]->prev, [&](auto& a) {
                return a.lock() == rem;
            });
            rem->next[0]->prev.insert(rem->next[0]->prev.end(), rem->prev.begin(), rem->prev.end());
            return optimize_cfg_node(rem->next[0], ctx);
        }
        ctx.all_cfg.insert(cfg);
        ctx.current_root->insert(cfg);
        for (auto& n : cfg->next) {
            n = optimize_cfg_node(n, ctx);
        }
        for (auto& v : cfg->lowered) {
            ctx.with_root(v.start, [&] {
                v.start = optimize_cfg_node(v.start, ctx);
            });
        }
        if (cfg->condition) {
            cfg->condition = optimize_cfg_expression(cfg->condition, ctx);
        }
        return cfg;
    }

    expected<void> analyze_dominators(DominatorTree& dom_tree, const std::shared_ptr<CFG>& root, std::set<std::shared_ptr<CFG>>& all_node) {
        dom_tree.roots.push_back(root);
        std::map<std::shared_ptr<CFG>, std::set<std::shared_ptr<CFG>>> doms;
        for (auto& n : all_node) {
            if (n == root) {
                doms[n].insert(n);
            }
            else {
                doms[n] = all_node;
            }
        }
        bool changed = true;
        while (changed) {
            changed = false;
            // 開始ノード以外の全てのノードnについてループ
            for (auto& n : all_node) {
                if (n == root) continue;

                // ===== nの支配集合を再計算 =====

                // 1. nの先行ノード(prev)全ての支配集合の共通部分(intersection)を求める
                std::optional<std::set<std::shared_ptr<CFG>>> intersection;

                for (auto& weak_pred : n->prev) {
                    if (auto pred = weak_pred.lock()) {  // weak_ptrからshared_ptrを取得
                        auto found = doms.find(pred);
                        if (found == doms.end()) {
                            continue;  // 外部ノード
                        }
                        auto& nodes = found->second;
                        if (!intersection.has_value()) {
                            // 最初の先行ノード
                            intersection = nodes;
                        }
                        else {
                            // 2つ目以降は、現在の共通集合と積集合を取る
                            // (C++では std::set_intersection などを利用)
                            std::set<std::shared_ptr<CFG>> temp_result;
                            std::set_intersection(
                                intersection->begin(), intersection->end(),
                                nodes.begin(), nodes.end(),
                                std::inserter(temp_result, temp_result.begin()));
                            intersection = std::move(temp_result);
                        }
                    }
                }

                // 2. 求めた共通部分に、ノードn自身を加える
                std::set<std::shared_ptr<CFG>> new_doms_for_n;
                if (intersection.has_value()) {
                    new_doms_for_n = *intersection;
                }
                new_doms_for_n.insert(n);

                // 3. もし支配集合が変化していたら、更新してフラグを立てる
                if (new_doms_for_n != doms.at(n)) {
                    doms[n] = new_doms_for_n;
                    changed = true;
                }
            }
        }

        // 全てのノードnについてループ
        for (const auto& n : all_node) {
            if (n == root) continue;

            // nの支配ノードからn自身を除いた集合Sを用意
            const auto& n_doms = doms.at(n);

            std::shared_ptr<CFG> best_candidate = nullptr;
            size_t max_size = 0;

            // 集合Sの中で、|doms(d)|が最大になるdを探す
            for (const auto& d : n_doms) {
                if (d == n) continue;  // n自身は除外

                const size_t candidate_size = doms.at(d).size();
                if (candidate_size > max_size) {
                    max_size = candidate_size;
                    best_candidate = d;
                }
            }

            if (best_candidate) {
                dom_tree.parent[n] = best_candidate;
            }
        }

        return {};
    }

    expected<CFGList> analyze_control_flow_graph(CFGStack& stack, RepositoryProxy proxy) {
        InternalCFGContext ctx{
            .stack = stack,
            .proxy = proxy,
        };
        auto all_stmt = ctx.proxy.get_all_statement();
        CFGList cfg_list;
        for (auto& stmt : *all_stmt) {
            auto fn = stmt.body.func_decl();
            if (!fn) {
                continue;
            }
            ctx.stack.end_of_function = std::make_shared<CFG>();
            MAYBE(cfg, analyze_ref(ctx, fn->body));
            cfg.end->next.push_back(ctx.stack.end_of_function);
            ctx.stack.end_of_function->prev.push_back(cfg.end);
            OptimizeContext ctx;
            ctx.with_root(cfg.start, [&] {
                cfg.start = optimize_cfg_node(cfg.start, ctx);
            });
            DominatorTree dom_tree;
            for (auto& p : ctx.per_roots) {
                MAYBE_VOID(dom_tree, analyze_dominators(dom_tree, p.first, p.second));
            }
            cfg_list.list[get_id(stmt.id)] = CFGResult{
                .cfg = std::move(cfg),
                .dom_tree = std::move(dom_tree),
            };
        }
        return cfg_list;
    }

    void write_cfg(futils::binary::writer& result, const CFGList& m, const MappingTable& ctx) {
        futils::code::CodeWriter<std::string> w;
        std::uint64_t id = 0;
        std::map<std::shared_ptr<CFG>, std::uint64_t> node_id;
        std::set<std::pair<std::shared_ptr<CFG>, std::shared_ptr<CFG>>> dominate_edges;
        std::map<std::shared_ptr<CFGExpression>, std::uint64_t> expr_id;
        std::vector<std::function<void()>> inter_subgraph_vector;
        auto write_expr = [&](auto&& write, auto&& write_expr, const std::shared_ptr<CFGExpression>& cfg, const DominatorTree& dom_tree) -> void {
            if (expr_id.find(cfg) != expr_id.end()) {
                return;
            }
            expr_id[cfg] = id++;
            w.write(std::format("{} [label=\"", expr_id[cfg]));
            auto origin = ctx.get_expression(cfg->original_node);
            if (cfg->children.size()) {
                w.write(std::format("{}:{}\\n", origin ? to_string(origin->body.kind) : "<end>", get_id(cfg->original_node)));
            }
            else {
                w.write(std::format("{}:{}\\n", origin ? to_string(origin->body.kind) : "<phi>", get_id(cfg->original_node)));
            }
            if (origin) {
                origin->body.visit([&](auto&& visitor, std::string_view name, auto&& value) -> void {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_integral_v<T>) {
                        w.write(std::format("{}:{}\\n", name, value));
                    }
                    else if constexpr (std::is_enum_v<T>) {
                        w.write(std::format("{}:{}\\n", name, to_string(value)));
                    }
                    else if constexpr (std::is_same_v<T, ebm::StatementRef>) {
                        if (name == "id") {
                            auto ident = ctx.get_identifier(value);
                            if (ident) {
                                w.write(std::format("{}:{}\\n", name, ident->body.data));
                            }
                        }
                    }
                    else if constexpr (AnyRef<T>) {
                        // ignore
                    }
                    else
                        VISITOR_RECURSE_CONTAINER(visitor, name, value)
                    else VISITOR_RECURSE(visitor, name, value)
                });
            }
            w.writeln("\"];");
            for (auto& child : cfg->children) {
                write_expr(write, write_expr, child, dom_tree);
                w.write(std::format("{} -> {}", expr_id[cfg], expr_id[child]));
                if (child->relation_name.size()) {
                    w.write(" [label=\"", child->relation_name, "\"]");
                }
                w.writeln(";");
            }
            if (cfg->related_cfg) {
                write(write, write_expr, dom_tree, std::nullopt, cfg->related_cfg->start);
                w.writeln(std::format("{} -> {} [style=dotted,label=\"related\"];", expr_id[cfg], node_id[cfg->related_cfg->start]));
            }
            if (auto call_ = origin ? origin->body.call_desc() : nullptr) {
                auto expr = ctx.get_expression(call_->callee);
                while (expr && expr->body.kind != ebm::ExpressionKind::IDENTIFIER) {
                    if (auto member = expr->body.member()) {
                        expr = ctx.get_expression(*member);
                        continue;
                    }
                    break;
                }
                if (expr) {
                    if (auto id = expr->body.id()) {
                        auto found = m.list.find(get_id(*id));
                        if (found != m.list.end()) {
                            inter_subgraph_vector.push_back([&, found, cfg] {
                                w.writeln(std::format("{} -> {} [style=dotted,label=\"call\"];", expr_id[cfg], node_id[found->second.cfg.start]));
                            });
                        }
                    }
                }
            }
        };
        auto write_node = [&](auto&& write, auto&& write_expr, const DominatorTree& dom_tree, std::optional<std::string> name, const std::shared_ptr<CFG>& cfg) -> void {
            if (node_id.find(cfg) != node_id.end()) {
                return;
            }
            node_id[cfg] = id++;
            w.write(std::format("{} [label=\"", node_id[cfg]));
            if (name) {
                w.write(std::format("fn {}\\n", name.value()));
            }
            auto origin = ctx.get_statement(cfg->original_node);
            if (cfg->next.size() == 0) {
                w.write(std::format("{}:{}\\n", origin ? to_string(origin->body.kind) : "<end>", get_id(cfg->original_node)));
            }
            else {
                w.write(std::format("{}:{}\\n", origin ? to_string(origin->body.kind) : "<phi>", get_id(cfg->original_node)));
            }
            if (origin) {
                auto add_io = [&](const ebm::IOData* io) {
                    auto typ = ctx.get_type(io->data_type);
                    w.write(std::format("  Type: {}\\n", typ ? to_string(typ->body.kind) : "<unknown type>"));
                    if (auto size = io->size.size()) {
                        w.write(std::format("  Size: {} {}\\n", size->value(), to_string(io->size.unit)));
                    }
                    else if (auto ref = io->size.ref()) {
                        auto expr = ctx.get_expression(*ref);
                        w.write(std::format("  Size: {}:{} {}\\n", expr ? to_string(expr->body.kind) : "<unknown expr>", ref->id.value(), to_string(io->size.unit)));
                    }
                    else {
                        w.write(std::format("  Size: {}\\n", to_string(io->size.unit)));
                    }
                };
                if (auto f = origin->body.read_data()) {
                    add_io(f);
                }
                if (auto f = origin->body.write_data()) {
                    add_io(f);
                }
            }

            w.writeln("\"];");
            for (auto& n : cfg->next) {
                write(write, write_expr, dom_tree, std::nullopt, n);
                w.write(std::format("{} -> {}", node_id[cfg], node_id[n]));
                if (n->condition) {
                    auto cond_expr = ctx.get_expression(n->condition->original_node);
                    w.write(std::format("[label=\"{}:{}\"]", cond_expr ? to_string(cond_expr->body.kind) : "<unknown expr>", get_id(n->condition->original_node)));
                }
                w.writeln(";");
                if (n->condition) {
                    write_expr(write, write_expr, n->condition, dom_tree);
                    w.writeln(std::format("{} -> {} [style=dotted,label=\"expression\"];", node_id[n], expr_id[n->condition]));
                }

                auto it = dom_tree.parent.find(n);
                if (it != dom_tree.parent.end()) {
                    auto parent = it->second;
                    write(write, write_expr, dom_tree, std::nullopt, parent);
                    auto dom_id = node_id[parent];
                    if (dominate_edges.contains({parent, n})) {
                        continue;
                    }
                    dominate_edges.insert({parent, n});
                    w.writeln(std::format("{} -> {} [style=dotted,label=\"dominates\"];", dom_id, node_id[n]));
                }
            }
            for (auto& n : cfg->lowered) {
                write(write, write_expr, dom_tree, std::nullopt, n.start);
                w.writeln(std::format("{} -> {} [style=dotted,label=\"lowered\"];", node_id[cfg], node_id[n.start]));
            }
        };
        w.writeln("digraph ControlFlowGraph {");
        auto indent = w.indent_scope();
        for (auto& cfg : m.list) {
            auto fn = ctx.get_statement(ebm::StatementRef{cfg.first});
            std::optional<std::string> name;
            if (fn) {
                if (auto fn_decl = fn->body.func_decl()) {
                    auto ident = ctx.get_identifier(fn_decl->name);
                    if (ident) {
                        name = ident->body.data;
                    }
                    else {
                        name = std::format("fn_{}", get_id(fn->id));
                    }
                    auto parent_ident = ctx.get_identifier(fn_decl->parent_format);
                    if (parent_ident) {
                        name = std::format("{}.{}", parent_ident->body.data, *name);
                    }
                }
            }
            w.write("subgraph \"");
            if (name) {
                w.write(*name);
            }
            w.writeln("\" {");
            auto indent = w.indent_scope();
            write_node(write_node, write_expr, cfg.second.dom_tree, name, cfg.second.cfg.start);
            indent.execute();
            w.writeln("}");
        }
        for (auto& f : inter_subgraph_vector) {
            f();
        }
        indent.execute();
        w.write("}\n");
        result.write(w.out());
    }

}  // namespace ebmgen
