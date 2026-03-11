/*license*/
#pragma once
#include "convert.hpp"
#include "helper.hpp"
#include "macro.hpp"
namespace rebgn {
    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n);
    Error convert_node_encode(Module& m, const std::shared_ptr<ast::Node>& n);
    Error convert_node_decode(Module& m, const std::shared_ptr<ast::Node>& n);

    template <class T>
    Error define(Module& m, std::shared_ptr<T>& node) = delete;

    template <class T>
    Error encode(Module& m, std::shared_ptr<T>& node) = delete;

    template <class T>
    Error decode(Module& m, std::shared_ptr<T>& node) = delete;

    expected<Varint> static_str(Module& m, const std::shared_ptr<ast::StrLiteral>& node);
    expected<Varint> immediate(Module& m, auto&& op, std::uint64_t n, brgen::lexer::Loc* loc = nullptr) {
        if (auto found = m.immediate_table.find(n); found != m.immediate_table.end()) {
            return varint(found->second);
        }
        auto ident = m.new_id(loc);
        if (!ident) {
            return ident;
        }
        auto val = varint(n);
        if (!val) {
            op(AbstractOp::IMMEDIATE_INT64, [&](Code& c) {
                c.int_value64(n);
                c.ident(*ident);
            });
        }
        else {
            op(AbstractOp::IMMEDIATE_INT, [&](Code& c) {
                c.int_value(*val);
                c.ident(*ident);
            });
        }
        m.immediate_table[n] = ident->value();
        return ident;
    }

    expected<Varint> immediate_bool(Module& m, auto&& op, bool b, brgen::lexer::Loc* loc = nullptr) {
        if (b && m.true_id) {
            return *m.true_id;
        }
        if (!b && m.false_id) {
            return *m.false_id;
        }
        auto ident = m.new_id(loc);
        if (!ident) {
            return ident;
        }
        if (b) {
            op(AbstractOp::IMMEDIATE_TRUE, [&](Code& c) {
                c.ident(*ident);
            });
            m.true_id = *ident;
        }
        else {
            op(AbstractOp::IMMEDIATE_FALSE, [&](Code& c) {
                c.ident(*ident);
            });
            m.false_id = *ident;
        }
        return ident;
    }

    expected<Varint> immediate(Module& m, std::uint64_t n, brgen::lexer::Loc* loc = nullptr);
    expected<Varint> immediate_bool(Module& m, bool b, brgen::lexer::Loc* loc = nullptr);
    expected<Varint> define_var(Module& m, Varint ident, Varint init_ref, StorageRef typ, ast::ConstantLevel level);
    expected<Varint> define_int_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level);
    expected<Varint> define_bool_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level);
    expected<Varint> define_typed_tmp_var(Module& m, Varint init_ref, StorageRef typ, ast::ConstantLevel level);
    expected<Varint> define_counter(Module& m, std::uint64_t init);
    expected<StorageRef> define_storage(Module& m, const std::shared_ptr<ast::Type>& typ, bool should_detect_recursive = false);
    expected<Varint> get_expr(Module& m, const std::shared_ptr<ast::Expr>& n);
    bool is_alignment_vector(ast::Field* t);
    bool is_range_compare(const std::shared_ptr<ast::Node>& node);
    Error do_range_compare(Module& m, BinaryOp op, ast::Range* range, Varint expr_);
    Error do_range_compare(Module& m, BinaryOp op, std::shared_ptr<ast::Expr>& left, std::shared_ptr<ast::Expr>& right);
    expected<Varint> define_expr_variable(Module& m, const std::shared_ptr<ast::Expr>& cond);

    Error encode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field, bool should_init_recursive);
    Error decode_type(Module& m, std::shared_ptr<ast::Type>& typ, Varint base_ref, std::shared_ptr<ast::Type> mapped_type, ast::Field* field, bool should_init_recursive);
    Error insert_phi(Module& m, PhiStack&& p);
    Error do_assign(Module& m,
                    const Storages* target_type,
                    const Storages* source_type,
                    Varint left, Varint right, bool should_recursive_struct_assign = false, brgen::lexer::Loc* loc = nullptr);

    CastType get_cast_type(const Storages& dest, const Storages& src);
    expected<std::optional<Varint>> add_assign_cast(Module& m, auto&& op, const Storages* dest, const Storages* src, Varint right,
                                                    bool should_recursive_struct_assign = false) {
        if (!dest || !src) {
            return std::nullopt;
        }
        auto dst_key = storage_key(*dest);
        auto src_key = storage_key(*src);
        auto src_copy = *src;
        if (dst_key == src_key) {
            if (!should_recursive_struct_assign || dst_key.find("RECURSIVE_STRUCT") == std::string::npos) {
                return std::nullopt;
            }
            for (auto& src : src_copy.storages) {
                if (src.type == StorageType::RECURSIVE_STRUCT_REF) {
                    auto src_ref = src.ref().value();
                    src.type = StorageType::STRUCT_REF;
                    src.ref(src_ref);
                }
            }
        }
        BM_GET_STORAGE_REF_WITH_LOC(dst_ref, unexpect_error, *dest, nullptr);
        BM_GET_STORAGE_REF_WITH_LOC(src_ref, unexpect_error, src_copy, nullptr);
        BM_CAST_WITH_ERROR(op, ident, unexpect_error, dst_ref, src_ref, right, (get_cast_type(*dest, src_copy)));
        return ident;
    }
    expected<std::optional<Storages>> may_get_type(Module& m, const std::shared_ptr<ast::Type>& typ);
    inline void maybe_insert_eval_expr(Module& m, const std::shared_ptr<ast::Node>& n) {
        if (!ast::as<ast::Expr>(n)) {
            return;
        }
        if (auto prev = m.get_prev_expr(); prev) {
            m.op(AbstractOp::EVAL_EXPR, [&](Code& c) {
                c.ref(*prev);
            });
        }
    }
    Error foreach_node(Module& m, ast::node_list& nodes, auto&& block) {
        for (auto& n : nodes) {
            m.set_prev_expr(null_id);
            auto err = block(n);
            if (err) {
                return err;
            }
            maybe_insert_eval_expr(m, n);
        }
        return none;
    }
    Error conditional_loop(Module& m, Varint cond, auto&& block) {
        m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
            c.ref(cond);
        });
        auto err = block();
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_LOOP);
        return none;
    }

    Error counter_loop(Module& m, auto&& op, Varint length, auto&& block) {
        auto counter = define_counter(m, 0);
        if (!counter) {
            return counter.error();
        }
        BM_BINARY(op, cmp, BinaryOp::less, *counter, length);
        op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
            c.ref(cmp);
        });
        auto err = block(*counter);
        if (err) {
            return err;
        }
        op(AbstractOp::INC, [&](Code& c) {
            c.ref(*counter);
        });
        op(AbstractOp::END_LOOP, [&](Code& c) {});
        return none;
    }

    Error counter_loop(Module& m, Varint length, auto&& block) {
        return counter_loop(m, [&](auto&& op, auto&& block) { return m.op(op, block); }, length, block);
    }

    void add_switch_union(Module& m, std::shared_ptr<ast::StructType>& node);

    inline auto make_yield_value_post_proc(Module& m, std::optional<Varint>& yield_value, std::optional<Storages>& yield_storage) {
        return [&](std::shared_ptr<ast::Node>& last) {
            if (yield_value) {
                auto prev_expr = m.get_prev_expr();
                if (!prev_expr) {
                    return prev_expr.error();
                }
                auto imp = ast::as<ast::ImplicitYield>(last);
                if (!imp) {
                    return error("Implicit yield is expected but {}", node_type_to_string(last->node_type));
                }
                auto pre_type = may_get_type(m, imp->expr->expr_type);
                if (!pre_type) {
                    return pre_type.error();
                }
                return do_assign(m, &*yield_storage, *pre_type ? &**pre_type : nullptr, *yield_value, *prev_expr);
            }
            return none;
        };
    }

    inline auto make_yield_value_final_proc(Module& m, std::optional<Varint>& yield_value) {
        return [&]() {
            if (yield_value) {
                m.set_prev_expr(yield_value->value());
            }
        };
    }

    Error convert_if(Module& m, std::shared_ptr<ast::If>& node, auto&& eval) {
        std::optional<Varint> yield_value;
        std::optional<Storages> yield_storage;
        if (!ast::as<ast::VoidType>(node->expr_type)) {
            auto s = define_storage(m, node->expr_type);
            if (!s) {
                return s.error();
            }
            auto new_id = m.new_node_id(node->expr_type);
            if (!new_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*new_id);
                c.type(*s);
            });
            auto tmp_var = define_typed_tmp_var(m, *new_id, *s, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            yield_value = *tmp_var;
            auto got = m.get_storage(*s);
            if (!got) {
                return got.error();
            }
            yield_storage = got.value();
        }
        auto yield_value_postproc = make_yield_value_post_proc(m, yield_value, yield_storage);
        auto yield_value_finalproc = make_yield_value_final_proc(m, yield_value);
        BM_COND_IN_BLOCK(m.op, m.code, cond_block, node->cond->expr);
        m.init_phi_stack(cond_block.value());
        BM_REF(m.op, AbstractOp::IF, cond_block);
        add_switch_union(m, node->then->struct_type);
        std::shared_ptr<ast::Node> last = nullptr;
        auto err = foreach_node(m, node->then->elements, [&](auto& n) {
            auto err = eval(m, n);
            if (err) {
                return err;
            }
            last = n;
            return none;
        });
        if (err) {
            return err;
        }
        err = yield_value_postproc(last);
        if (err) {
            return err;
        }
        if (node->els) {
            std::shared_ptr<ast::If> els = node;
            while (els) {
                if (auto e = ast::as<ast::If>(els->els)) {
                    auto cond = get_expr(m, e->cond->expr);
                    if (!cond) {
                        return cond.error();
                    }
                    m.next_phi_candidate(cond->value());
                    m.op(AbstractOp::ELIF, [&](Code& c) {
                        c.ref(*cond);
                    });
                    add_switch_union(m, e->then->struct_type);
                    std::shared_ptr<ast::Node> last = nullptr;
                    auto err = foreach_node(m, e->then->elements, [&](auto& n) {
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                        last = n;
                        return none;
                    });
                    if (err) {
                        return err;
                    }
                    err = yield_value_postproc(last);
                    if (err) {
                        return err;
                    }
                    els = ast::cast_to<ast::If>(els->els);
                }
                else if (auto block = ast::as<ast::IndentBlock>(els->els)) {
                    m.next_phi_candidate(null_id);
                    m.op(AbstractOp::ELSE);
                    add_switch_union(m, block->struct_type);
                    std::shared_ptr<ast::Node> last = nullptr;
                    auto err = foreach_node(m, block->elements, [&](auto& n) {
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                        last = n;
                        return none;
                    });
                    if (err) {
                        return err;
                    }
                    err = yield_value_postproc(last);
                    if (err) {
                        return err;
                    }
                    break;
                }
                else {
                    break;
                }
            }
        }
        m.op(AbstractOp::END_IF);
        err = insert_phi(m, m.end_phi_stack());
        if (err) {
            return err;
        }
        yield_value_finalproc();
        return none;
    }

    inline bool can_be_numeric_match(std::shared_ptr<ast::Match>& node) {
        if (!node->cond) {
            return false;
        }
        auto expr_type = node->cond->expr->expr_type;
        if (auto u = ast::as<ast::UnionType>(expr_type); u && u->common_type) {
            expr_type = u->common_type;
        }
        if (!ast::as<ast::IntType>(expr_type) && !ast::as<ast::IntLiteralType>(expr_type) && !ast::as<ast::EnumType>(expr_type)) {
            return false;
        }
        for (auto& b : node->branch) {
            if (!ast::is_any_range(b->cond->expr)) {
                if (!ast::as<ast::IntType>(b->cond->expr->expr_type) &&
                    !ast::as<ast::IntLiteralType>(b->cond->expr->expr_type) &&
                    !ast::as<ast::EnumType>(b->cond->expr->expr_type)) {
                    return false;
                }
            }
        }
        return true;
    }

    Error convert_match(Module& m, std::shared_ptr<ast::Match>& node, auto&& eval) {
        std::optional<Varint> yield_value;
        std::optional<Storages> yield_storage;
        if (!ast::as<ast::VoidType>(node->expr_type)) {
            auto s = define_storage(m, node->expr_type);
            if (!s) {
                return s.error();
            }
            auto new_id = m.new_node_id(node->expr_type);
            if (!new_id) {
                return error("Failed to generate new id");
            }
            m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                c.ident(*new_id);
                c.type(*s);
            });
            auto tmp_var = define_typed_tmp_var(m, *new_id, *s, ast::ConstantLevel::variable);
            if (!tmp_var) {
                return tmp_var.error();
            }
            yield_value = *tmp_var;
            auto got = m.get_storage(*s);
            if (!got) {
                return got.error();
            }
            yield_storage = std::move(got.value());
        }
        auto yield_value_postproc = make_yield_value_post_proc(m, yield_value, yield_storage);
        auto yield_value_finalproc = make_yield_value_final_proc(m, yield_value);
        if (can_be_numeric_match(node)) {
            auto cond = get_expr(m, node->cond);
            if (!cond) {
                return cond.error();
            }
            m.init_phi_stack(cond->value());
            if (node->struct_union_type->exhaustive) {
                m.op(AbstractOp::EXHAUSTIVE_MATCH, [&](Code& c) {
                    c.ref(*cond);
                });
            }
            else {
                m.op(AbstractOp::MATCH, [&](Code& c) {
                    c.ref(*cond);
                });
            }
            for (auto& c : node->branch) {
                if (ast::is_any_range(c->cond->expr)) {
                    m.next_phi_candidate(null_id);
                    m.op(AbstractOp::DEFAULT_CASE);
                }
                else {
                    auto cond = get_expr(m, c->cond->expr);
                    if (!cond) {
                        return cond.error();
                    }
                    m.next_phi_candidate(cond->value());
                    m.op(AbstractOp::CASE, [&](Code& c) {
                        c.ref(*cond);
                    });
                }
                std::shared_ptr<ast::Node> last = nullptr;
                if (auto scoped = ast::as<ast::ScopedStatement>(c->then)) {
                    add_switch_union(m, scoped->struct_type);
                    m.set_prev_expr(null_id);
                    auto err = eval(m, scoped->statement);
                    if (err) {
                        return err;
                    }
                    maybe_insert_eval_expr(m, scoped->statement);
                    last = scoped->statement;
                }
                else if (auto block = ast::as<ast::IndentBlock>(c->then)) {
                    add_switch_union(m, block->struct_type);
                    auto err = foreach_node(m, block->elements, [&](auto& n) {
                        auto err = eval(m, n);
                        if (err) {
                            return err;
                        }
                        last = n;
                        return none;
                    });
                    if (err) {
                        return err;
                    }
                }
                else {
                    return error("Invalid match branch");
                }
                auto err = yield_value_postproc(last);
                if (err) {
                    return err;
                }
                m.op(AbstractOp::END_CASE);
            }
            m.op(AbstractOp::END_MATCH);
            auto err = insert_phi(m, m.end_phi_stack());
            if (err) {
                return err;
            }
            yield_value_finalproc();
            return none;
        }
        if (node->trial_match) {
            return error("Trial match is not supported yet");
        }
        std::optional<Varint> base_expr;
        if (node->cond) {
            auto v = define_expr_variable(m, node->cond);
            if (!v) {
                return v.error();
            }
            base_expr = v.value();
        }
        // translate into if-else
        std::shared_ptr<ast::Node> last = nullptr;
        for (auto& c : node->branch) {
            if (ast::is_any_range(c->cond->expr)) {
                // if first branch, so we don't need to use if-elses
                if (last) {
                    m.next_phi_candidate(null_id);
                    m.op(AbstractOp::ELSE);
                    last = c;
                }
            }
            else {
                auto range = ast::as<ast::Range>(c->cond->expr);
                Varint origCond;
                BM_BEGIN_COND_BLOCK(m.op, m.code, cond_block, &c->loc);
                if (range) {
                    if (!base_expr) {
                        return error("Invalid match branch; range condition is not allowed without base condition");
                    }
                    auto err = do_range_compare(m, BinaryOp::equal, range, *base_expr);
                    if (err) {
                        return err;
                    }
                    auto ok = m.get_prev_expr();
                    if (!ok) {
                        return ok.error();
                    }
                    origCond = ok.value();
                }
                else {
                    BM_GET_EXPR(cond, m, c->cond->expr);
                    if (base_expr) {
                        BM_BINARY(m.op, new_id, BinaryOp::equal, *base_expr, cond);
                        origCond = new_id;
                    }
                    else {
                        origCond = cond;
                    }
                }
                BM_END_COND_BLOCK(m.op, m.code, cond_block, origCond);
                if (!last) {
                    m.init_phi_stack(cond_block.value());
                    BM_REF(m.op, AbstractOp::IF, cond_block);
                    last = c;
                }
                else {
                    m.next_phi_candidate(cond_block.value());
                    BM_REF(m.op, AbstractOp::ELIF, cond_block);
                    last = c;
                }
            }
            std::shared_ptr<ast::Node> last_yield = nullptr;
            if (auto scoped = ast::as<ast::ScopedStatement>(c->then)) {
                add_switch_union(m, scoped->struct_type);
                m.set_prev_expr(null_id);
                auto err = eval(m, scoped->statement);
                if (err) {
                    return err;
                }
                maybe_insert_eval_expr(m, scoped->statement);
                last_yield = scoped->statement;
            }
            else if (auto block = ast::as<ast::IndentBlock>(c->then)) {
                add_switch_union(m, block->struct_type);
                auto err = foreach_node(m, block->elements, [&](auto& n) {
                    auto err = eval(m, n);
                    if (err) {
                        return err;
                    }
                    last_yield = n;
                    return none;
                });
                if (err) {
                    return err;
                }
            }
            else {
                return error("Invalid match branch");
            }
            auto err = yield_value_postproc(last_yield);
            if (err) {
                return err;
            }
        }
        if (last) {
            m.op(AbstractOp::END_IF);
            auto err = insert_phi(m, m.end_phi_stack());
            if (err) {
                return err;
            }
        }
        yield_value_finalproc();
        return none;
    }

    Error convert_loop(Module& m, std::shared_ptr<ast::Loop>& node, auto&& eval) {
        auto inner_block = [&] {
            return foreach_node(m, node->body->elements, [&](auto& n) {
                return eval(m, n);
            });
        };
        if (node->init) {
            if (auto bop = ast::as<ast::Binary>(node->init);
                bop && bop->op == ast::BinaryOp::in_assign) {  // `for x in y`
                auto ident = ast::as<ast::Ident>(bop->left);
                if (!ident) {
                    return error("Invalid loop init target :{}", node_type_to_string(bop->left->node_type));
                }
                auto target = get_expr(m, bop->right);
                if (!target) {
                    return error("Invalid loop init: {}", target.error().error());
                }
                if (ast::as<ast::IntType>(bop->right->expr_type)) {
                    return counter_loop(m, target.value(), [&](Varint counter) {
                        auto id = m.lookup_ident(ast::cast_to<ast::Ident>(bop->left));
                        if (!id) {
                            return id.error();
                        }
                        auto typ = define_storage(m, bop->left->expr_type);
                        if (!typ) {
                            return typ.error();
                        }
                        auto res = define_var(m, id.value(), counter, *typ, ast::ConstantLevel::immutable_variable);
                        if (!res) {
                            return res.error();
                        }
                        auto err = inner_block();
                        if (err) {
                            return err;
                        }
                        return none;
                    });
                }
                else if (auto range = ast::as<ast::RangeType>(bop->right->expr_type)) {
                    auto l = range->range.lock();
                    Varint start, end;
                    if (l->start) {
                        auto s = get_expr(m, l->start);
                        if (!s) {
                            return s.error();
                        }
                    }
                    else {
                        auto s = immediate(m, 0);
                        if (!s) {
                            return s.error();
                        }
                    }
                    if (l->end) {
                        auto e = get_expr(m, l->end);
                        if (!e) {
                            return e.error();
                        }
                    } /*otherwise, its inf*/
                    auto tmp_var = define_bool_tmp_var(m, start, ast::ConstantLevel::variable);
                    if (!tmp_var) {
                        return tmp_var.error();
                    }
                    if (end.value() != 0) {
                        BM_BINARY_NODE(m.op, id, l->op == ast::BinaryOp::range_inclusive ? BinaryOp::less_or_eq : BinaryOp::less, *tmp_var, end, node);
                        m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
                            c.ref(id);
                        });
                    }
                    else {
                        m.op(AbstractOp::LOOP_INFINITE);
                    }
                    auto err = inner_block();
                    if (err) {
                        return err;
                    }
                    m.op(AbstractOp::INC, [&](Code& c) {
                        c.ref(*tmp_var);
                    });
                    m.op(AbstractOp::END_LOOP);
                }
                else if (ast::as<ast::ArrayType>(bop->right->expr_type)) {
                    auto size_id = m.new_id(nullptr);
                    if (!size_id) {
                        return size_id.error();
                    }
                    m.op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
                        c.ident(*size_id);
                        c.ref(*target);
                    });
                    return counter_loop(m, *size_id, [&](Varint counter) {
                        BM_INDEX(m.op, idx, *target, counter);
                        BM_LOOKUP_IDENT(ident, m, ast::cast_to<ast::Ident>(bop->left));
                        m.op(AbstractOp::DEFINE_VARIABLE_REF, [&](Code& c) {
                            c.ident(ident);
                            c.ref(idx);
                        });
                        auto err = inner_block();
                        if (err) {
                            return err;
                        }
                        return none;
                    });
                }
                else if (auto lit = ast::as<ast::StrLiteral>(bop->right)) {
                    auto str_id = static_str(m, ast::cast_to<ast::StrLiteral>(bop->right));
                    if (!str_id) {
                        return str_id.error();
                    }
                    auto len = varint(lit->length);
                    if (!len) {
                        return len.error();
                    }
                    return counter_loop(m, *len, [&](Varint counter) {
                        BM_INDEX(m.op, id, *str_id, counter);
                        BM_LOOKUP_IDENT(ident, m, ast::cast_to<ast::Ident>(bop->left));
                        m.op(AbstractOp::DEFINE_VARIABLE_REF, [&](Code& c) {
                            c.ident(ident);
                            c.ref(id);
                        });
                        auto err = inner_block();
                        if (err) {
                            return err;
                        }
                        return none;
                    });
                }
                else {
                    return error("Invalid loop init type : {}", node_type_to_string(bop->right->expr_type->node_type));
                }
            }
            auto err = eval(m, node->init);
            if (err) {
                return err;
            }
        }
        if (node->cond) {
            BM_BEGIN_COND_BLOCK(m.op, m.code, cond_block, &node->cond->loc);
            BM_GET_EXPR(cond, m, node->cond);
            BM_END_COND_BLOCK(m.op, m.code, cond_block, cond);
            m.op(AbstractOp::LOOP_CONDITION, [&](Code& c) {
                c.ref(cond);
            });
        }
        else {
            m.op(AbstractOp::LOOP_INFINITE);
        }
        auto err = inner_block();
        if (err) {
            return err;
        }
        if (node->step) {
            auto err = eval(m, node->step);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_LOOP);
        return none;
    }

    Code make_code(AbstractOp op, auto&& set) {
        Code c;
        c.op = op;
        set(c);
        return c;
    }
}  // namespace rebgn
