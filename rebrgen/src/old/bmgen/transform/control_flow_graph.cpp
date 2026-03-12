/*license*/
#include <bmgen/internal.hpp>

namespace rebgn {
    void write_cfg(futils::binary::writer& w, Module& m) {
        w.write("digraph G {\n");
        std::uint64_t id = 0;
        std::map<std::shared_ptr<CFG>, std::uint64_t> node_id;
        std::map<std::uint64_t, std::shared_ptr<CFG>> fn_to_cfg;
        std::function<void(std::shared_ptr<CFG>&)> write_node = [&](std::shared_ptr<CFG>& cfg) {
            if (node_id.find(cfg) != node_id.end()) {
                return;
            }
            node_id[cfg] = id++;
            w.write(std::format("  {} [label=\"", node_id[cfg]));
            std::vector<std::shared_ptr<CFG>> callee;
            w.write(std::format("{} bit\\n", cfg->sum_bits));
            for (auto& i : cfg->basic_block) {
                w.write(std::format("{}", to_string(m.code[i].op)));
                if (m.code[i].op == AbstractOp::DEFINE_FUNCTION) {
                    if (auto found = m.ident_table_rev.find(m.code[i].ident().value().value());
                        found != m.ident_table_rev.end()) {
                        w.write(" ");
                        if (auto bs = m.ident_table_rev.find(m.code[i].belong().value().value());
                            bs != m.ident_table_rev.end()) {
                            w.write(bs->second->ident);
                            w.write(".");
                        }
                        w.write(found->second->ident);
                    }
                }
                if (m.code[i].op == AbstractOp::ENCODE_INT || m.code[i].op == AbstractOp::DECODE_INT) {
                    w.write(std::format(" {}", m.code[i].bit_size().value().value()));
                }
                if (m.code[i].op == AbstractOp::CALL_ENCODE || m.code[i].op == AbstractOp::CALL_DECODE) {
                    auto& call = fn_to_cfg[m.code[i].left_ref().value().value()];
                    callee.push_back(call);
                }
                w.write("\\n");
            }
            w.write("\"];\n");
            for (auto& c : callee) {
                write_node(c);
                w.write(std::format("  {} -> {} [style=dotted];\n", node_id[cfg], node_id[c]));
            }
            for (auto& n : cfg->next) {
                write_node(n);
                w.write(std::format("  {} -> {};\n", node_id[cfg], node_id[n]));
            }
        };
        for (auto& cfg : m.cfgs) {
            auto ident = m.code[cfg->basic_block[0]].ident().value().value();
            fn_to_cfg[ident] = cfg;
        }
        for (auto& cfg : m.cfgs) {
            write_node(cfg);
        }
        w.write("}\n");
    }

    struct ReservedBlock {
        std::shared_ptr<CFG> block;
        std::shared_ptr<CFG> end;
    };

    expected<std::shared_ptr<CFG>> make_control_flow_graph(Module& m, futils::view::rspan<size_t> index_of_operation_and_loop) {
        std::shared_ptr<CFG> root, current;
        root = current = std::make_shared<CFG>(CFG{});
        auto fn_end = std::make_shared<CFG>(CFG{});
        std::vector<ReservedBlock> loops;
        std::vector<ReservedBlock> ifs;
        std::vector<ReservedBlock> matchs;
        size_t i = 0;
        for (; i < index_of_operation_and_loop.size(); i++) {
            switch (m.code[index_of_operation_and_loop[i]].op) {
                case AbstractOp::DEFINE_FUNCTION: {
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::END_FUNCTION: {
                    current->next.push_back(fn_end);
                    fn_end->prev.push_back(current);
                    fn_end->basic_block.push_back(index_of_operation_and_loop[i]);
                    current = fn_end;
                    break;
                }
                case AbstractOp::ENCODE_INT:
                case AbstractOp::DECODE_INT:
                    current->sum_bits += m.code[index_of_operation_and_loop[i]].bit_size().value().value();
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                case AbstractOp::CALL_ENCODE:
                case AbstractOp::CALL_DECODE: {
                    auto plus = m.code[index_of_operation_and_loop[i]].bit_size_plus().value().value();
                    if (plus) {
                        current->sum_bits += plus - 1;
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::LOOP_INFINITE:
                case AbstractOp::LOOP_CONDITION: {
                    if (current->basic_block.size()) {
                        auto loop = std::make_shared<CFG>(CFG{});
                        current->next.push_back(loop);
                        loop->prev.push_back(current);
                        current = loop;
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    if (m.code[index_of_operation_and_loop[i]].op == AbstractOp::LOOP_CONDITION) {
                        current->next.push_back(end);
                        end->prev.push_back(current);
                    }
                    loops.push_back({current, end});
                    auto block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(block);
                    block->prev.push_back(current);
                    current = block;
                    break;
                }
                case AbstractOp::END_LOOP: {
                    if (loops.size() == 0) {
                        return unexpect_error("Invalid loop");
                    }
                    auto loop = loops.back();
                    loops.pop_back();
                    auto next_block = loop.end;
                    current->next.push_back(loop.block);
                    loop.block->prev.push_back(current);
                    current = next_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::IF: {
                    if (current->basic_block.size()) {
                        auto if_block = std::make_shared<CFG>(CFG{});
                        current->next.push_back(if_block);
                        if_block->prev.push_back(current);
                        current = if_block;
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    ifs.push_back({current, end});
                    auto then_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(then_block);
                    then_block->prev.push_back(current);
                    current = then_block;
                    break;
                }
                case AbstractOp::ELIF:
                case AbstractOp::ELSE: {
                    if (ifs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    current->next.push_back(ifs.back().end);
                    ifs.back().end->prev.push_back(current);
                    auto if_block = std::make_shared<CFG>(CFG{});
                    ifs.back().block->next.push_back(if_block);
                    if_block->prev.push_back(ifs.back().block);
                    current = if_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::END_IF: {
                    if (ifs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    auto if_block = ifs.back();
                    ifs.pop_back();
                    auto next_block = if_block.end;
                    auto has_else = false;
                    for (auto& n : if_block.block->next) {
                        if (n->basic_block.size() && m.code[n->basic_block[0]].op == AbstractOp::ELSE) {
                            has_else = true;
                            break;
                        }
                    }
                    if (!has_else) {
                        if_block.block->next.push_back(next_block);
                        next_block->prev.push_back(if_block.block);
                    }
                    current->next.push_back(next_block);
                    next_block->prev.push_back(current);
                    current = next_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::MATCH:
                case AbstractOp::EXHAUSTIVE_MATCH: {
                    auto match_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(match_block);
                    match_block->prev.push_back(current);
                    current = match_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto end = std::make_shared<CFG>(CFG{});
                    if (m.code[index_of_operation_and_loop[i]].op == AbstractOp::MATCH) {
                        current->next.push_back(end);
                        end->prev.push_back(current);
                    }
                    matchs.push_back({match_block, end});
                    break;
                }
                case AbstractOp::CASE:
                case AbstractOp::DEFAULT_CASE: {
                    if (matchs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    auto case_block = std::make_shared<CFG>(CFG{});
                    current->next.push_back(case_block);
                    case_block->prev.push_back(current);
                    current = case_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::END_CASE: {
                    if (matchs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    current->next.push_back(matchs.back().end);
                    matchs.back().end->prev.push_back(current);
                    current = matchs.back().block;
                    break;
                }
                case AbstractOp::END_MATCH: {
                    if (matchs.size() == 0) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    auto match_block = matchs.back();
                    matchs.pop_back();
                    auto next_block = match_block.end;
                    current = next_block;
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
                }
                case AbstractOp::RET: {
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    current->next.push_back(fn_end);
                    fn_end->prev.push_back(current);
                    auto block = std::make_shared<CFG>(CFG{});  // no relation with current
                    current = block;
                    break;
                }
                case AbstractOp::BREAK: {
                    if (!loops.size()) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    loops.back().end->prev.push_back(current);
                    current->next.push_back(loops.back().end);
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto block = std::make_shared<CFG>(CFG{});  // no relation with current
                    current = block;
                    break;
                }
                case AbstractOp::CONTINUE: {
                    if (!loops.size()) {
                        return unexpect_error("Invalid {}", to_string(m.code[index_of_operation_and_loop[i]].op));
                    }
                    loops.back().block->next.push_back(loops.back().end);
                    loops.back().end->prev.push_back(loops.back().block);
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    auto block = std::make_shared<CFG>(CFG{});  // no relation with current
                    current = block;
                    break;
                }
                default:
                    current->basic_block.push_back(index_of_operation_and_loop[i]);
                    break;
            }
        }
        return root;
    }

    Error search_encode_decode(Module& m, std::vector<size_t>& index_of_operation_and_loop, Varint fn) {
        auto& start = m.ident_index_table[fn.value()];
        index_of_operation_and_loop.push_back(start);  // for define function
        size_t i = start;
        for (; m.code[i].op != AbstractOp::END_FUNCTION; i++) {
            switch (m.code[i].op) {
                case AbstractOp::IF:
                case AbstractOp::ELIF:
                case AbstractOp::ELSE:
                case AbstractOp::END_IF:
                case AbstractOp::MATCH:
                case AbstractOp::EXHAUSTIVE_MATCH:
                case AbstractOp::CASE:
                case AbstractOp::DEFAULT_CASE:
                case AbstractOp::END_CASE:
                case AbstractOp::END_MATCH:
                case AbstractOp::LOOP_INFINITE:
                case AbstractOp::LOOP_CONDITION:
                case AbstractOp::END_LOOP:
                case AbstractOp::BREAK:
                case AbstractOp::CONTINUE:
                case AbstractOp::RET:
                case AbstractOp::ENCODE_INT:
                case AbstractOp::DECODE_INT:
                case AbstractOp::CALL_ENCODE:
                case AbstractOp::CALL_DECODE:
                    index_of_operation_and_loop.push_back(i);
                    break;
                default:
                    index_of_operation_and_loop.push_back(i);  // for other operation
                    break;
            }
        }
        index_of_operation_and_loop.push_back(i);  // for end function
        return none;
    }

    Error generate_cfg1(Module& m) {
        for (size_t i = 0; i < m.code.size(); i++) {
            auto& c = m.code[i];
            if (c.op == AbstractOp::DEFINE_ENCODER || c.op == AbstractOp::DEFINE_DECODER) {
                auto fn = c.right_ref().value();
                std::vector<size_t> index_of_operation_and_loop;
                auto err = search_encode_decode(m, index_of_operation_and_loop, fn);
                if (err) {
                    return err;
                }
                auto cfg = make_control_flow_graph(m, index_of_operation_and_loop);
                if (!cfg) {
                    return cfg.error();
                }
                m.cfgs.push_back(cfg.value());
            }
        }
        return none;
    }

    /*
    Error add_packed_operation_for_cfg(Module& m) {
        std::unordered_map<size_t, std::shared_ptr<CFG>> index_to_cfg;
        std::unordered_set<std::shared_ptr<CFG>> visited;
        std::unordered_set<size_t> insert_define_packed_operation;
        std::unordered_set<size_t> insert_end_packed_operation;
        auto&& f = [&](auto&& f, std::shared_ptr<CFG>& cfg) -> Error {
            if (visited.find(cfg) != visited.end()) {
                return none;
            }
            visited.insert(cfg);
            cfg->sum_bits;
            return none;
        };
        for (auto& cfg : m.cfgs) {
            auto err = f(f, cfg);
            if (err) {
                return err;
            }
        }
        return none;
    }
    */

}  // namespace rebgn