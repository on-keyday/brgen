/*license*/
#include <bmgen/internal.hpp>
#include <bmgen/bit.hpp>

namespace rebgn {
    std::string property_name_suffix(Module& m, const Storages& s) {
        std::string suffix;
        for (auto& storage : s.storages) {
            if (storage.type == StorageType::ARRAY) {
                suffix += "array_";
            }
            else if (storage.type == StorageType::OPTIONAL) {
                suffix += "optional_";
            }
            else if (storage.type == StorageType::PTR) {
                suffix += "ptr_";
            }
            else if (storage.type == StorageType::STRUCT_REF) {
                if (auto found = m.ident_table_rev.find(storage.ref().value().value());
                    found != m.ident_table_rev.end()) {
                    suffix += found->second->ident;
                }
                else {
                    suffix += std::format("struct_{}", storage.ref().value().value());
                }
            }
            else if (storage.type == StorageType::ENUM) {
                if (auto found = m.ident_table_rev.find(storage.ref().value().value());
                    found != m.ident_table_rev.end()) {
                    suffix += found->second->ident;
                    break;
                }
                else {
                    suffix += std::format("enum_{}", storage.ref().value().value());
                }
            }
            else if (storage.type == StorageType::VARIANT) {
                suffix += "Variant";
            }
            else if (storage.type == StorageType::UINT) {
                suffix += std::format("uint{}", storage.size().value().value());
            }
            else if (storage.type == StorageType::INT) {
                suffix += std::format("int{}", storage.size().value().value());
            }
            else {
                suffix += "unknown";
            }
        }
        return suffix;
    }

    struct RetrieveVarCtx {
        std::unordered_set<ObjectID> variables;
        std::vector<ObjectID> ordered_variables;
    };

    expected<ast::Field*> get_field(Module& m, ObjectID id) {
        auto base = m.ident_table_rev.find(id);
        if (base == m.ident_table_rev.end()) {
            return unexpect_error("Invalid ident");
        }
        auto field_ptr = ast::as<ast::Field>(base->second->base.lock());
        if (!field_ptr) {
            return unexpect_error("Invalid field");
        }
        return field_ptr;
    }

    Error retrieve_var(Module& m, auto&& op, const rebgn::Code& code, RetrieveVarCtx& variables) {
        switch (code.op) {
            case rebgn::AbstractOp::NOT_PREV_THEN: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::CAST: {
                return retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::ARRAY_SIZE: {
                return retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::CALL_CAST: {
                auto param = code.param().value();
                for (auto&& p : param.refs) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                break;
            }
            case rebgn::AbstractOp::INDEX: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::APPEND: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::ASSIGN: {
                auto ref = code.ref().value().value();
                if (ref != 0) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[ref]], variables);
                    if (err) {
                        return err;
                    }
                }
                else {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                return retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::ACCESS: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::BINARY: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                err = retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::UNARY: {
                return retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
            }
            case rebgn::AbstractOp::DEFINE_PARAMETER: {
                // variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::DEFINE_FIELD: {
                auto ident = code.ident().value().value();
                auto got = get_field(m, ident);
                if (got) {
                    auto field = *got;
                    if (field->is_state_variable) {
                        if (!variables.variables.contains(ident)) {
                            op(AbstractOp::STATE_VARIABLE_PARAMETER, [&](Code& m) {
                                m.ref(code.ident().value());
                            });
                            variables.variables.insert(ident);
                            variables.ordered_variables.push_back(ident);
                        }
                    }
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_PROPERTY: {
                // variables.insert(code.ident().value().value());
                break;
            }
            case rebgn::AbstractOp::DEFINE_CONSTANT: {
                break;  // TODO(on-keyday): check constant origin
            }
            case rebgn::AbstractOp::BEGIN_COND_BLOCK: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::DEFINE_VARIABLE: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                if (!variables.variables.contains(code.ident().value().value())) {
                    variables.variables.insert(code.ident().value().value());
                    variables.ordered_variables.push_back(code.ident().value().value());
                    op(AbstractOp::DECLARE_VARIABLE, [&](Code& m) {
                        m.ref(code.ident().value());
                    });
                }
                break;
            }
            case rebgn::AbstractOp::FIELD_AVAILABLE: {
                if (auto left_ref = code.left_ref(); left_ref->value() != 0) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[left_ref.value().value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.right_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                break;
            }
            case rebgn::AbstractOp::CALL: {
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[code.ref().value().value()]], variables);
                if (err) {
                    return err;
                }
                auto params = code.param().value();
                for (auto& p : params.refs) {
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                break;
            }
            case rebgn::AbstractOp::PHI: {
                if (variables.variables.contains(code.ident().value().value())) {
                    return none;
                }
                variables.variables.insert(code.ident().value().value());
                auto phis = code.phi_params().value();
                auto ref = code.ref().value().value();
                for (auto& p : phis.params) {
                    if (p.condition.value() != 0) {
                        auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.condition.value()]], variables);
                        if (err) {
                            return err;
                        }
                    }
                    auto err = retrieve_var(m, op, m.code[m.ident_index_table[p.assign.value()]], variables);
                    if (err) {
                        return err;
                    }
                }
                bool first = true;
                for (auto& p : phis.params) {
                    auto tmp_id = m.new_id(nullptr);
                    if (!tmp_id) {
                        return tmp_id.error();
                    }
                    auto idx = m.ident_index_table[p.assign.value()];
                    if (p.condition.value() == 0) {
                        if (!first) {
                            op(AbstractOp::ELSE, [&](Code& m) {});
                        }
                    }
                    else {
                        if (first) {
                            op(AbstractOp::IF, [&](Code& m) {
                                m.ref(*varint(p.condition.value()));
                            });
                            first = false;
                        }
                        else {
                            op(AbstractOp::ELIF, [&](Code& m) {
                                m.ref(*varint(p.condition.value()));
                            });
                        }
                    }
                    if (m.code[idx].op == AbstractOp::ASSIGN) {
                        auto right_ref = m.code[idx].right_ref().value();
                        op(AbstractOp::ASSIGN, [&](Code& m) {
                            m.ident(*tmp_id);
                            m.left_ref(*varint(ref));
                            m.right_ref(right_ref);
                        });
                    }
                }
                if (!first) {
                    op(AbstractOp::END_IF, [&](Code& m) {});
                }
                break;
            }
            case rebgn::AbstractOp::IMMEDIATE_CHAR:
            case rebgn::AbstractOp::IMMEDIATE_INT:
            case rebgn::AbstractOp::IMMEDIATE_STRING:
            case rebgn::AbstractOp::IMMEDIATE_TRUE:
            case rebgn::AbstractOp::IMMEDIATE_FALSE:
            case rebgn::AbstractOp::IMMEDIATE_INT64:
            case rebgn::AbstractOp::IMMEDIATE_TYPE:
            case rebgn::AbstractOp::NEW_OBJECT:
                break;
            default:
                return error("Invalid op: {}", to_string(code.op));
        }
        return none;
    }

    bool can_set_ident(const std::shared_ptr<ast::Node>& node) {
        if (auto ident = ast::as<ast::Ident>(node); ident) {
            auto [base, _] = *ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
            if (auto f = ast::as<ast::Field>(base->base.lock())) {
                return true;
            }
        }
        else if (auto member = ast::as<ast::MemberAccess>(node); member) {
            return can_set_ident(member->target);
        }
        return false;
    }

    bool can_set_array_length(ast::Field* field_ptr) {
        if (auto array = ast::as<ast::ArrayType>(field_ptr->field_type);
            array && array->length && !array->length_value && can_set_ident(array->length) &&
            !ast::as<ast::UnionType>(array->length->expr_type)) { /*TODO(on-keyday): support union*/
            return true;
        }
        return false;
    }

    expected<Varint> access_array_length(Module& m, RetrieveVarCtx& rvar, const std::shared_ptr<ast::Node>& len, auto&& op) {
        if (auto ident = ast::as<ast::Ident>(len); ident) {
            auto l = m.lookup_ident(ast::cast_to<ast::Ident>(len));
            if (!l) {
                return l;
            }
            auto got = get_field(m, l->value());
            if (got) {
                auto field_ptr = *got;
                if (field_ptr->is_state_variable) {
                    if (!rvar.variables.contains(l->value())) {
                        op(AbstractOp::STATE_VARIABLE_PARAMETER, [&](Code& m) {
                            m.ref(*l);
                        });
                        rvar.variables.insert(l->value());
                        rvar.ordered_variables.push_back(l->value());
                    }
                }
            }
            return l;
        }
        else if (auto member = ast::as<ast::MemberAccess>(len); member) {
            auto target = access_array_length(m, rvar, member->target, op);
            if (!target) {
                return target;
            }
            auto member_id = m.lookup_ident(member->member);
            if (!member_id) {
                return member_id;
            }
            auto id = m.new_node_id(len);
            if (!id) {
                return id;
            }
            op(AbstractOp::ACCESS, [&](Code& m) {
                m.ident(*id);
                m.left_ref(*target);
                m.right_ref(*member_id);
            });
            return *id;
        }
        return unexpect_error("Invalid node");
    }

    Error add_array_length_setter(Module& m, RetrieveVarCtx& rvar, ast::Field* field_ptr, Varint array_ref, Varint function, auto&& op) {
        auto array = ast::as<ast::ArrayType>(field_ptr->field_type);
        if (!array) {
            return error("Invalid field type");
        }
        auto target_id = access_array_length(m, rvar, array->length, op);
        if (!target_id) {
            return target_id.error();
        }
        auto bit_size = array->length->expr_type->bit_size;
        if (auto u = ast::as<ast::UnionType>(array->length->expr_type); u && u->common_type) {
            bit_size = u->common_type->bit_size;
        }
        if (!bit_size) {
            return error("Invalid bit size");
        }
        auto max_value = safe_left_shift(1, *bit_size) - 1;
        BM_IMMEDIATE(op, value_id, max_value);
        BM_NEW_ID(length_id, error, nullptr);
        op(AbstractOp::ARRAY_SIZE, [&](Code& m) {
            m.ident(length_id);
            m.ref(array_ref);
        });
        BM_BINARY(op, cmp_id, BinaryOp::less_or_eq, length_id, value_id);
        op(AbstractOp::ASSERT, [&](Code& m) {
            m.ref(cmp_id);
            m.belong(function);
        });
        auto dst = Storages{.length = *varint(1), .storages = {Storage{.type = StorageType::UINT}}};
        auto src = dst;
        dst.storages[0].size(*varint(*bit_size));
        src.storages[0].size(*varint(*bit_size + 1));  // to force insert cast
        auto cast = add_assign_cast(m, op, &dst, &src, length_id, false);
        if (!cast) {
            return cast.error();
        }
        if (!*cast) {
            return error("Failed to add cast");
        }
        BM_ASSIGN(op, assign_id, *target_id, **cast, null_varint, nullptr);
        return none;
    }

    Error derive_property_getter_setter(Module& m, Code& base, std::vector<Code>& funcs, std::unordered_map<ObjectID, std::pair<Varint, Varint>>& merged_fields) {
        auto op = [&](AbstractOp op, auto&& set) {
            funcs.push_back(make_code(op, set));
        };
        auto belong_of_belong = m.code[m.ident_index_table[base.belong().value().value()]].belong().value();
        auto merged_ident = base.ident().value();
        auto getter_setter = merged_fields[merged_ident.value()];
        auto getter_ident = getter_setter.first;
        auto setter_ident = getter_setter.second;
        auto mode = base.merge_mode().value();
        auto type_found = m.get_storage(base.type().value());
        if (!type_found) {
            return type_found.error();
        }
        auto type = type_found.value();
        auto originalType = type;
        auto originalTypeRef = base.type().value();
        auto param = base.param().value();
        auto do_foreach = [&](auto&& action, auto&& ret_empty) {
            RetrieveVarCtx variables;
            for (auto& c : param.refs) {
                auto& conditional_field = m.code[m.ident_index_table[c.value()]];
                auto err = retrieve_var(m, op, m.code[m.ident_index_table[conditional_field.left_ref().value().value()]], variables);
                if (err) {
                    return err;
                }
            }
            std::optional<ObjectID> prev_cond;
            for (auto& c : param.refs) {
                // get CONDITIONAL_FIELD or CONDITIONAL_PROPERTY
                auto& code = m.code[m.ident_index_table[c.value()]];
                // get condition expr
                auto expr = &m.code[m.ident_index_table[code.left_ref()->value()]];
                auto expr_ref = expr->ident().value();
                auto orig = expr;
                if (expr->op == AbstractOp::NOT_PREV_THEN) {
                    std::vector<ObjectID> conditions;

                    while (!prev_cond || prev_cond != expr->left_ref()->value()) {
                        auto prev_expr = &m.code[m.ident_index_table[expr->left_ref()->value()]];
                        if (prev_expr->op == AbstractOp::NOT_PREV_THEN) {
                            conditions.push_back(prev_expr->right_ref()->value());
                        }
                        else {
                            conditions.push_back(prev_expr->ident()->value());
                            break;
                        }
                        expr = prev_expr;
                    }
                    ObjectID last = 0;
                    for (auto& co : conditions) {
                        if (last) {
                            BM_BINARY(op, and_, BinaryOp::logical_or, *varint(co), *varint(last));
                            last = and_.value();
                        }
                        else {
                            last = co;
                        }
                    }
                    if (last != 0) {
                        op(AbstractOp::IF, [&](Code& m) {
                            m.ref(*varint(last));
                        });
                        auto err = ret_empty();
                        if (err) {
                            return err;
                        }
                        op(AbstractOp::END_IF, [&](Code& m) {});
                    }
                    expr_ref = orig->right_ref().value();
                }
                BM_REF(op, AbstractOp::IF, expr_ref);
                auto err = action(code, variables);
                if (err) {
                    return err;
                }
                op(AbstractOp::END_IF, [&](Code& m) {});
                prev_cond = orig->ident()->value();
            }
            return none;
        };

        // getter function
        op(AbstractOp::DEFINE_FUNCTION, [&](Code& n) {
            n.ident(getter_ident);
            n.belong(belong_of_belong);
            n.func_type(FunctionType::UNION_GETTER);
        });

        type.length = *varint(type.storages.size() + 1);
        if (mode == MergeMode::COMMON_TYPE) {
            type.storages.insert(type.storages.begin(), Storage{.type = StorageType::OPTIONAL});
        }
        else {
            type.storages.insert(type.storages.begin(), Storage{.type = StorageType::PTR});
        }
        auto ret_type_ref = m.get_storage_ref(type, nullptr);
        if (!ret_type_ref) {
            return ret_type_ref.error();
        }
        op(AbstractOp::RETURN_TYPE, [&](Code& n) {
            n.type(*ret_type_ref);
        });
        op(AbstractOp::PROPERTY_FUNCTION, [&](Code& m) {
            m.ref(merged_ident);
        });
        auto ret_empty = [&]() {
            BM_NEW_ID(empty_ident, error, nullptr);
            if (mode == MergeMode::COMMON_TYPE) {
                op(AbstractOp::EMPTY_OPTIONAL, [&](Code& m) {
                    m.ident(empty_ident);
                });
            }
            else {
                op(AbstractOp::EMPTY_PTR, [&](Code& m) {
                    m.ident(empty_ident);
                });
            }
            op(AbstractOp::RET, [&](Code& m) {
                m.ref(empty_ident);
                m.belong(getter_ident);
            });
            return none;
        };
        auto err = do_foreach([&](const Code& code, RetrieveVarCtx& rvar) {
            auto expr_ref = code.right_ref().value();
            auto ident = m.new_id(nullptr);
            if (!ident) {
                return ident.error();
            }
            if (code.op == AbstractOp::CONDITIONAL_PROPERTY) {
                auto found = merged_fields.find(expr_ref.value());
                if (found == merged_fields.end()) {
                    return error("Invalid conditional property");
                }
                auto getter = found->second.first;
                op(AbstractOp::CALL, [&](Code& code) {
                    code.ident(*ident);
                    code.ref(getter);
                });
            }
            else {
                auto& field = m.code[m.ident_index_table[expr_ref.value()]];
                auto belong = field.belong().value();
                op(AbstractOp::CHECK_UNION, [&](Code& m) {
                    m.ref(belong);
                    m.check_at(mode == MergeMode::COMMON_TYPE
                                   ? UnionCheckAt::PROPERTY_GETTER_OPTIONAL
                                   : UnionCheckAt::PROPERTY_GETTER_PTR);
                });
                if (mode == MergeMode::COMMON_TYPE) {
                    op(AbstractOp::OPTIONAL_OF, [&](Code& m) {
                        m.ident(*ident);
                        m.ref(expr_ref);
                        m.type(originalTypeRef);
                    });
                }
                else {
                    op(AbstractOp::ADDRESS_OF, [&](Code& m) {
                        m.ident(*ident);
                        m.ref(expr_ref);
                    });
                }
            }
            op(AbstractOp::RET, [&](Code& m) {
                m.ref(*ident);
                m.belong(getter_ident);
            });
            return none;
        },
                              ret_empty);
        if (err) {
            return err;
        }
        ret_empty();
        op(AbstractOp::END_FUNCTION, [&](Code& m) {});

        op(AbstractOp::DEFINE_FUNCTION, [&](Code& n) {
            n.ident(setter_ident);
            n.belong(belong_of_belong);
            n.func_type(FunctionType::UNION_SETTER);
        });
        auto prop = m.new_id(nullptr);
        if (!prop) {
            return prop.error();
        }
        op(AbstractOp::PROPERTY_INPUT_PARAMETER, [&](Code& n) {
            n.ident(*prop);
            n.left_ref(merged_ident);
            n.right_ref(setter_ident);
            n.type(originalTypeRef);
        });
        ret_type_ref = m.get_storage_ref(Storages{
                                             .length = *varint(1),
                                             .storages = {Storage{.type = StorageType::PROPERTY_SETTER_RETURN}},
                                         },
                                         nullptr);
        if (!ret_type_ref) {
            return ret_type_ref.error();
        }
        op(AbstractOp::RETURN_TYPE, [&](Code& n) {
            n.type(*ret_type_ref);
        });
        op(AbstractOp::PROPERTY_FUNCTION, [&](Code& m) {
            m.ref(merged_ident);
        });
        auto bool_ident = m.new_id(nullptr);
        if (!bool_ident) {
            return bool_ident.error();
        }
        auto ret_false = [&]() {
            op(AbstractOp::RET_PROPERTY_SETTER_FAIL, [&](Code& m) {
                m.belong(setter_ident);
            });
            return none;
        };
        err = do_foreach([&](const Code& code, RetrieveVarCtx& rvar) {
            auto expr_ref = code.right_ref().value();
            if (code.op == AbstractOp::CONDITIONAL_PROPERTY) {
                auto found = merged_fields.find(expr_ref.value());
                if (found == merged_fields.end()) {
                    return error("Invalid conditional property");
                }
                auto setter = found->second.second;
                op(AbstractOp::PROPERTY_ASSIGN, [&](Code& code) {
                    code.left_ref(setter);
                    code.right_ref(*prop);
                });
            }
            else {
                auto& field = m.code[m.ident_index_table[expr_ref.value()]];
                auto belong = field.belong().value();
                op(AbstractOp::SWITCH_UNION, [&](Code& m) {
                    m.ref(belong);
                });
                auto ident = m.new_id(nullptr);
                if (!ident) {
                    return ident.error();
                }
                auto field_ptr = get_field(m, expr_ref.value());
                if (!field_ptr) {
                    return field_ptr.error();
                }
                auto storage = define_storage(m, (*field_ptr)->field_type, true);
                if (!storage) {
                    return storage.error();
                }
                if (can_set_array_length(*field_ptr)) {
                    auto err = add_array_length_setter(m, rvar, *field_ptr, *prop, setter_ident, op);
                    if (err) {
                        return err;
                    }
                }
                auto to_type = m.get_storage(*storage);
                if (!to_type) {
                    return to_type.error();
                }
                auto right = *prop;
                auto maybe_cast = add_assign_cast(m, op, &*to_type, &originalType, right);
                if (!maybe_cast) {
                    return maybe_cast.error();
                }
                if (*maybe_cast) {
                    right = **maybe_cast;
                }
                op(AbstractOp::ASSIGN, [&](Code& m) {
                    m.ident(*ident);
                    m.left_ref(expr_ref);
                    m.right_ref(right);
                });
            }
            op(AbstractOp::RET_PROPERTY_SETTER_OK, [&](Code& m) {
                m.belong(setter_ident);
            });
            return none;
        },
                         ret_false);
        if (err) {
            return err;
        }
        ret_false();
        op(AbstractOp::END_FUNCTION, [&](Code& m) {});
        return none;
    }

    Error derive_set_array_function(Module& m, Varint setter_ident, ObjectID ident, ast::Field* field_ptr, auto&& op) {
        auto belong = m.code[m.ident_index_table[ident]].belong().value();
        auto originalType = define_storage(m, field_ptr->field_type);
        if (!originalType) {
            return originalType.error();
        }
        op(AbstractOp::DEFINE_FUNCTION, [&](Code& n) {
            n.ident(setter_ident);
            n.belong(belong);
            n.func_type(FunctionType::VECTOR_SETTER);
        });
        auto prop = m.new_id(nullptr);
        if (!prop) {
            return prop.error();
        }
        auto ident_ref = varint(ident);
        if (!ident_ref) {
            return ident_ref.error();
        }
        op(AbstractOp::PROPERTY_INPUT_PARAMETER, [&](Code& n) {
            n.ident(*prop);
            n.left_ref(*ident_ref);
            n.right_ref(setter_ident);
            n.type(*originalType);
        });
        auto ret_type_ref = m.get_storage_ref(
            Storages{
                .length = *varint(1),
                .storages = {Storage{.type = StorageType::PROPERTY_SETTER_RETURN}},
            },
            nullptr);
        if (!ret_type_ref) {
            return ret_type_ref.error();
        }
        op(AbstractOp::RETURN_TYPE, [&](Code& n) {
            n.type(*ret_type_ref);
        });
        op(AbstractOp::PROPERTY_FUNCTION, [&](Code& m) {
            m.ref(*ident_ref);
        });
        RetrieveVarCtx rvar;
        auto err = add_array_length_setter(m, rvar, field_ptr, *prop, setter_ident, op);
        if (err) {
            return err;
        }
        BM_ASSIGN(op, assign_id, *ident_ref, *prop, null_varint, nullptr);
        op(AbstractOp::RET_PROPERTY_SETTER_OK, [&](Code& m) {
            m.belong(setter_ident);
        });
        op(AbstractOp::END_FUNCTION, [&](Code& m) {});
        return none;
    }

    // derive union property getter and setter
    // also derive dynamic array setter if simple array length
    Error derive_property_functions(Module& m) {
        std::vector<Code> funcs;
        auto op = [&](AbstractOp o, auto&& set) {
            funcs.push_back(make_code(o, set));
        };
        std::unordered_map<ObjectID, std::pair<Varint, Varint>> merged_fields;
        std::unordered_map<ObjectID, ObjectID> properties_to_merged;
        std::unordered_map<ObjectID, std::pair<ast::Field*, Varint>> set_array_length;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::DEFINE_FIELD) {
                auto belong = c.belong().value().value();
                if (belong == 0) {
                    continue;
                }
                auto belong_idx = m.ident_index_table.find(belong);
                if (belong_idx == m.ident_index_table.end()) {
                    return error("Invalid belong");
                }
                if (m.code[belong_idx->second].op != AbstractOp::DEFINE_FORMAT) {
                    continue;
                }
                auto field_ptr = get_field(m, c.ident().value().value());
                if (!field_ptr) {
                    continue;  // best effort
                }
                if (can_set_array_length(*field_ptr)) {
                    expected<Varint> func_ident;
                    if (!(*field_ptr)->ident) {
                        func_ident = m.new_id(&(*field_ptr)->loc);
                        if (!func_ident) {
                            return func_ident.error();
                        }
                    }
                    else {
                        auto temporary_setter_ident = std::make_shared<ast::Ident>((*field_ptr)->loc, (*field_ptr)->ident->ident);
                        temporary_setter_ident->base = (*field_ptr)->ident->base;
                        func_ident = m.lookup_ident(temporary_setter_ident);
                        if (!func_ident) {
                            return func_ident.error();
                        }
                    }
                    set_array_length.emplace(c.ident()->value(), std::make_pair(*field_ptr, *func_ident));
                }
            }
            else if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                auto belong = c.belong().value();
                auto mode = c.merge_mode().value();
                auto merged_ident = c.ident().value();
                auto base = m.ident_table_rev[belong.value()];
                std::string ident = base->ident;
                if (mode == MergeMode::COMMON_TYPE ||
                    mode == MergeMode::STRICT_COMMON_TYPE) {
                    properties_to_merged[belong.value()] = merged_ident.value();
                }
                else {
                    auto found = m.get_storage(c.type().value());
                    if (!found) {
                        return found.error();
                    }
                    ident += "_" + property_name_suffix(m, found.value());
                }
                auto temporary_getter_ident = std::make_shared<ast::Ident>(base->loc, ident);
                temporary_getter_ident->base = base->base;
                auto getter_ident = m.lookup_ident(temporary_getter_ident);
                if (!getter_ident) {
                    return getter_ident.error();
                }
                merged_fields[merged_ident.value()].first = *getter_ident;
                auto temporary_setter_ident = std::make_shared<ast::Ident>(base->loc, ident);
                temporary_setter_ident->base = base->base;
                // setter function
                auto setter_ident = m.lookup_ident(temporary_setter_ident);
                if (!setter_ident) {
                    return setter_ident.error();
                }
                merged_fields[merged_ident.value()].second = *setter_ident;
            }
        }
        for (auto& c : m.code) {
            if (c.op == AbstractOp::ASSIGN) {
                auto left_ref = c.left_ref().value();
                if (auto found = properties_to_merged.find(left_ref.value()); found != properties_to_merged.end()) {
                    if (auto found2 = merged_fields.find(found->second); found2 != merged_fields.end()) {
                        auto right_ref = c.right_ref().value();
                        c.op = AbstractOp::PROPERTY_ASSIGN;
                        c.left_ref(found2->second.second);
                        c.right_ref(right_ref);
                    }
                }
            }
            else if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                auto err = derive_property_getter_setter(m, c, funcs, merged_fields);
                if (err) {
                    return err;
                }
            }
            else if (c.op == AbstractOp::DEFINE_FIELD) {
                auto ident = c.ident().value();
                if (auto found = set_array_length.find(ident.value());
                    found != set_array_length.end()) {
                    auto err = derive_set_array_function(m, found->second.second, ident.value(), found->second.first, op);
                    if (err) {
                        return err;
                    }
                }
            }
        }
        std::vector<Code> rebound;
        for (auto& c : m.code) {
            if (c.op == AbstractOp::MERGED_CONDITIONAL_FIELD) {
                auto ident = c.ident().value();
                auto found = merged_fields.find(ident.value());
                if (found != merged_fields.end()) {
                    rebound.push_back(std::move(c));
                    rebound.push_back(make_code(AbstractOp::DEFINE_PROPERTY_GETTER, [&](auto& c) {
                        c.left_ref(ident);
                        c.right_ref(*varint(found->second.first.value()));
                    }));
                    rebound.push_back(make_code(AbstractOp::DEFINE_PROPERTY_SETTER, [&](auto& c) {
                        c.left_ref(ident);
                        c.right_ref(*varint(found->second.second.value()));
                    }));
                }
                continue;
            }
            else if (c.op == AbstractOp::DEFINE_FIELD) {
                auto ident = c.ident().value();
                if (auto found = set_array_length.find(ident.value());
                    found != set_array_length.end()) {
                    rebound.push_back(std::move(c));
                    rebound.push_back(make_code(AbstractOp::DECLARE_FUNCTION, [&](auto& c) {
                        c.ref(found->second.second);
                    }));
                    continue;
                }
            }
            rebound.push_back(std::move(c));
        }
        m.code = std::move(rebound);
        m.code.insert(m.code.end(), funcs.begin(), funcs.end());
        return none;
    }

}  // namespace rebgn
