/*license*/
#include "internal.hpp"
#include <core/ast/traverse.h>
#include <format>
#include <set>
#include "helper.hpp"
#include <fnet/util/base64.h>
#include "macro.hpp"

namespace rebgn {

    CastType get_cast_type(const Storages& dest, const Storages& src) {
        if (dest.storages[0].type == StorageType::VECTOR) {
            if (src.storages[0].type == StorageType::ARRAY) {
                return CastType::ARRAY_TO_VECTOR;
            }
            if (src.storages[0].type == StorageType::INT || src.storages[0].type == StorageType::UINT) {
                return CastType::INT_TO_VECTOR;
            }
        }
        if (dest.storages[0].type == StorageType::ARRAY) {
            if (src.storages[0].type == StorageType::VECTOR) {
                return CastType::VECTOR_TO_ARRAY;
            }
            if (src.storages[0].type == StorageType::INT || src.storages[0].type == StorageType::UINT) {
                return CastType::INT_TO_ARRAY;
            }
        }
        if (dest.storages[0].type == StorageType::INT || dest.storages[0].type == StorageType::UINT) {
            if (src.storages[0].type == StorageType::ENUM) {
                return CastType::ENUM_TO_INT;
            }
            if (src.storages[0].type == StorageType::FLOAT) {
                return CastType::FLOAT_TO_INT_BIT;
            }
            if (dest.storages[0].type == StorageType::UINT || dest.storages[0].type == StorageType::INT) {
                auto dest_size = dest.storages[0].size()->value();
                auto src_size = src.storages[0].size()->value();
                if (dest_size < src_size) {
                    if (dest_size == 1) {
                        return CastType::INT_TO_ONE_BIT;
                    }
                    return CastType::LARGE_INT_TO_SMALL_INT;
                }
                if (dest_size > src_size) {
                    if (src_size == 1) {
                        return CastType::ONE_BIT_TO_INT;
                    }
                    return CastType::SMALL_INT_TO_LARGE_INT;
                }
                if (dest.storages[0].type == StorageType::UINT && src.storages[0].type == StorageType::INT) {
                    return CastType::SIGNED_TO_UNSIGNED;
                }
                if (dest.storages[0].type == StorageType::INT && src.storages[0].type == StorageType::UINT) {
                    return CastType::UNSIGNED_TO_SIGNED;
                }
            }
            if (src.storages[0].type == StorageType::BOOL) {
                return CastType::BOOL_TO_INT;
            }
        }
        if (dest.storages[0].type == StorageType::ENUM) {
            if (src.storages[0].type == StorageType::INT || src.storages[0].type == StorageType::UINT) {
                return CastType::INT_TO_ENUM;
            }
        }
        if (dest.storages[0].type == StorageType::FLOAT) {
            if (src.storages[0].type == StorageType::INT || src.storages[0].type == StorageType::UINT) {
                return CastType::INT_TO_FLOAT_BIT;
            }
        }
        if (dest.storages[0].type == StorageType::BOOL) {
            if (src.storages[0].type == StorageType::INT || src.storages[0].type == StorageType::UINT) {
                return CastType::INT_TO_BOOL;
            }
        }
        if (dest.storages[0].type == StorageType::RECURSIVE_STRUCT_REF) {
            if (src.storages[0].type == StorageType::STRUCT_REF) {
                return CastType::STRUCT_TO_RECURSIVE_STRUCT;
            }
        }
        if (dest.storages[0].type == StorageType::STRUCT_REF) {
            if (src.storages[0].type == StorageType::RECURSIVE_STRUCT_REF) {
                return CastType::RECURSIVE_STRUCT_TO_STRUCT;
            }
        }
        return CastType::OTHER;
    }

    expected<Varint> get_expr(Module& m, const std::shared_ptr<ast::Expr>& n) {
        m.set_prev_expr(null_id);
        auto err = convert_node_definition(m, n);
        if (err) {
            return unexpect_error(std::move(err));
        }
        return m.get_prev_expr();
    }

    expected<Varint> immediate_bool(Module& m, bool b, brgen::lexer::Loc* loc) {
        return immediate_bool(m, [&](auto&&... args) { m.op(args...); }, b, loc);
    }

    expected<Varint> immediate_char(Module& m, std::uint64_t c, brgen::lexer::Loc* loc) {
        auto ident = m.new_id(loc);
        if (!ident) {
            return ident;
        }
        auto val = varint(c);
        if (!val) {
            return val;
        }
        m.op(AbstractOp::IMMEDIATE_CHAR, [&](Code& c) {
            c.int_value(*val);
            c.ident(*ident);
        });
        return ident;
    }

    expected<Varint> immediate(Module& m, std::uint64_t n, brgen::lexer::Loc* loc) {
        return immediate(m, [&](auto&&... args) { m.op(args...); }, n, loc);
    }

    expected<Varint> define_var(Module& m, Varint ident, Varint init_ref, StorageRef typ, ast::ConstantLevel level) {
        m.op(AbstractOp::DEFINE_VARIABLE, [&](Code& c) {
            c.ident(ident);
            c.ref(init_ref);
            c.type(typ);
        });
        m.previous_assignments[ident.value()] = ident.value();
        return ident;
    }

    expected<Varint> define_bool_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level) {
        BM_NEW_ID(ident, unexpect_error, nullptr);
        BM_GET_STORAGE_REF_WITH_LOC(ref, unexpect_error, (Storages{
                                                             .length = *varint(1),
                                                             .storages = {
                                                                 {
                                                                     .type = StorageType::BOOL,
                                                                 },
                                                             },
                                                         }),
                                    nullptr);
        return define_var(m, ident, init_ref, ref,
                          level);
    }

    expected<Varint> define_int_tmp_var(Module& m, Varint init_ref, ast::ConstantLevel level) {
        BM_NEW_ID(ident, unexpect_error, nullptr);
        auto tmp = Storages{
            .length = *varint(1),
            .storages = {
                {
                    .type = StorageType::UINT,
                },
            },
        };
        tmp.storages.front().size(*varint(64));
        BM_GET_STORAGE_REF_WITH_LOC(ref, unexpect_error, tmp, nullptr);
        return define_var(m, ident, init_ref, ref, level);
    }

    expected<Varint> define_typed_tmp_var(Module& m, Varint init_ref, StorageRef typ, ast::ConstantLevel level) {
        BM_NEW_ID(ident, unexpect_error, nullptr);
        return define_var(m, ident, init_ref, std::move(typ), level);
    }

    expected<Varint> define_counter(Module& m, std::uint64_t init) {
        auto init_ref = immediate(m, init);
        if (!init_ref) {
            return init_ref;
        }
        return define_int_tmp_var(m, *init_ref, ast::ConstantLevel::variable);
    }

    Error do_assign(Module& m,
                    const Storages* target_type,
                    const Storages* from_type,
                    Varint left, Varint right, bool should_recursive_struct_assign, brgen::lexer::Loc* loc) {
        auto res = add_assign_cast(m, [&](auto&&... args) { m.op(args...); }, target_type, from_type, right, should_recursive_struct_assign);
        if (!res) {
            return res.error();
        }
        if (*res) {
            right = **res;
        }
        auto prev_assign = m.prev_assign(left.value());
        BM_ASSIGN(m.op, new_id, left, right, *varint(prev_assign), loc);
        m.assign(left.value(), new_id.value());
        return none;
    }

    Error insert_phi(Module& m, PhiStack&& p) {
        auto& start = p.start_state;
        auto new_state = start;
        for (auto& s : start) {
            PhiParams params;
            bool has_diff = false;
            for (auto& c : p.candidates) {
                auto cand = c.candidate[s.first];
                has_diff = has_diff || cand != s.second;
                params.params.push_back(
                    {
                        .condition = *varint(c.condition),
                        .assign = *varint(cand),
                    });
            }
            if (!has_diff) {
                continue;
            }
            if (p.candidates.back().condition != null_id) {
                params.params.push_back(
                    {
                        .condition = *varint(null_id),
                        .assign = *varint(s.second),
                    });
            }
            auto len = varint(params.params.size());
            if (!len) {
                return len.error();
            }
            params.length = len.value();
            auto new_id = m.new_id(nullptr);
            if (!new_id) {
                return new_id.error();
            }
            m.op(AbstractOp::PHI, [&](Code& c) {
                c.ident(*new_id);
                c.ref(*varint(s.first));
                c.phi_params(std::move(params));
            });
            new_state[s.first] = new_id->value();
        }
        m.previous_assignments = std::move(new_state);
        return none;
    }

    template <>
    Error define<ast::IntLiteral>(Module& m, std::shared_ptr<ast::IntLiteral>& node) {
        auto n = node->parse_as<std::uint64_t>();
        if (!n) {
            return error("Invalid integer literal: {}", node->value);
        }
        auto id = immediate(m, *n);
        if (!id) {
            return id.error();
        }
        m.set_prev_expr(id->value());
        return none;
    }

    template <>
    Error define<ast::Import>(Module& m, std::shared_ptr<ast::Import>& node) {
        auto prog_id = m.new_node_id(node->import_desc);
        if (!prog_id) {
            return prog_id.error();
        }
        m.op(AbstractOp::DEFINE_PROGRAM, [&](Code& c) {
            c.ident(*prog_id);
        });
        m.map_struct(node->import_desc->struct_type, prog_id->value());
        auto err = foreach_node(m, node->import_desc->elements, [&](auto& n) {
            return convert_node_definition(m, n);
        });
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_PROGRAM);
        auto import_id = m.new_node_id(node);
        if (!import_id) {
            return import_id.error();
        }
        m.op(AbstractOp::IMPORT, [&](Code& c) {
            c.ident(*import_id);
            c.ref(*prog_id);
        });
        m.set_prev_expr(import_id->value());
        return none;
    }

    template <>
    Error define<ast::Break>(Module& m, std::shared_ptr<ast::Break>& node) {
        m.op(AbstractOp::BREAK);
        return none;
    }

    template <>
    Error define<ast::Continue>(Module& m, std::shared_ptr<ast::Continue>& node) {
        m.op(AbstractOp::CONTINUE);
        return none;
    }

    template <>
    Error define<ast::Match>(Module& m, std::shared_ptr<ast::Match>& node) {
        return convert_match(m, node, [](Module& m, auto& n) {
            return convert_node_definition(m, n);
        });
    }

    template <>
    Error define<ast::If>(Module& m, std::shared_ptr<ast::If>& node) {
        return convert_if(m, node, [](Module& m, auto& n) {
            return convert_node_definition(m, n);
        });
    }

    template <>
    Error define<ast::Loop>(Module& m, std::shared_ptr<ast::Loop>& node) {
        return convert_loop(m, node, [](Module& m, auto& n) {
            return convert_node_definition(m, n);
        });
    }

    template <>
    Error define<ast::Return>(Module& m, std::shared_ptr<ast::Return>& node) {
        auto func = node->related_function.lock();
        if (!func) {
            return error("return statement must be in a function");
        }
        auto ident = m.lookup_ident(func->ident);
        if (!ident) {
            return ident.error();
        }
        if (node->expr) {
            auto val = get_expr(m, node->expr);
            if (!val) {
                return val.error();
            }
            m.op(AbstractOp::RET, [&](Code& c) {
                c.belong(*ident);
                c.ref(*val);
            });
        }
        else {
            m.op(AbstractOp::RET, [&](Code& c) {
                c.belong(*ident);
                c.ref(*varint(null_id));
            });
        }
        return none;
    }

    template <>
    Error define<ast::IOOperation>(Module& m, std::shared_ptr<ast::IOOperation>& node) {
        switch (node->method) {
            case ast::IOMethod::input_backward: {
                auto op = m.on_encode_fn ? AbstractOp::BACKWARD_OUTPUT : AbstractOp::BACKWARD_INPUT;
                if (node->arguments.size()) {
                    auto arg = get_expr(m, node->arguments[0]);
                    if (!arg) {
                        return arg.error();
                    }
                    m.op(op, [&](Code& c) {
                        c.ref(*arg);
                    });
                }
                else {
                    auto imm = immediate(m, 1);
                    if (!imm) {
                        return imm.error();
                    }
                    m.op(op, [&](Code& c) {
                        c.ref(*imm);
                    });
                }
                return none;
            }
            case ast::IOMethod::input_offset:
            case ast::IOMethod::input_bit_offset: {
                auto new_id = m.new_node_id(node);
                if (!new_id) {
                    return new_id.error();
                }
                m.op(node->method == ast::IOMethod::input_offset
                         ? (m.on_encode_fn ? AbstractOp::OUTPUT_BYTE_OFFSET : AbstractOp::INPUT_BYTE_OFFSET)
                         : (m.on_encode_fn ? AbstractOp::OUTPUT_BIT_OFFSET : AbstractOp::INPUT_BIT_OFFSET),
                     [&](Code& c) {
                         c.ident(*new_id);
                     });
                m.set_prev_expr(new_id->value());
                return none;
            }
            case ast::IOMethod::input_get: {
                auto ref = define_storage(m, node->expr_type);
                if (!ref) {
                    return ref.error();
                }
                auto id = m.new_node_id(node);
                if (!id) {
                    return id.error();
                }
                m.op(AbstractOp::NEW_OBJECT, [&](Code& c) {
                    c.ident(*id);
                    c.type(*ref);
                });
                auto tmp_var = define_typed_tmp_var(m, *id, *ref, ast::ConstantLevel::variable);
                if (!tmp_var) {
                    return tmp_var.error();
                }
                auto err = decode_type(m, node->expr_type, *tmp_var, nullptr, nullptr, false);
                if (err) {
                    return err;
                }
                auto prev = m.prev_assign(tmp_var->value());
                m.set_prev_expr(prev);
                return none;
            }
            case ast::IOMethod::output_put: {
                if (!node->arguments.size()) {
                    return error("Invalid output_put: no arguments");
                }
                auto arg = get_expr(m, node->arguments[0]);
                if (!arg) {
                    return arg.error();
                }
                auto err = encode_type(m, node->arguments[0]->expr_type, *arg, nullptr, nullptr, false);
                if (err) {
                    return err;
                }
                return none;
            }
            case ast::IOMethod::config_endian_big:
            case ast::IOMethod::config_endian_little: {
                auto imm = immediate(m, node->method == ast::IOMethod::config_endian_big ? 0 : 1);
                if (!imm) {
                    return imm.error();
                }
                m.set_prev_expr(imm->value());
                return none;
            }
            default: {
                return error("Unsupported IO operation: {}", to_string(node->method));
            }
        }
    }

    template <>
    Error define<ast::BoolLiteral>(Module& m, std::shared_ptr<ast::BoolLiteral>& node) {
        auto ref = immediate_bool(m, node->value, &node->loc);
        if (!ref) {
            return ref.error();
        }
        m.set_prev_expr(ref->value());
        return none;
    }

    template <>
    Error define<ast::CharLiteral>(Module& m, std::shared_ptr<ast::CharLiteral>& node) {
        auto ref = immediate_char(m, node->code, &node->loc);
        if (!ref) {
            return ref.error();
        }
        m.set_prev_expr(ref->value());
        return none;
    }

    expected<Varint> static_str(Module& m, const std::shared_ptr<ast::StrLiteral>& node) {
        std::string candidate;
        if (!futils::base64::decode(node->binary_value, candidate)) {
            return unexpect_error("Invalid base64 string: {}", node->binary_value);
        }
        auto str_ref = m.lookup_string(candidate, &node->loc);
        if (!str_ref) {
            return str_ref.transform([](auto&& v) { return v.first; });
        }
        if (str_ref->second) {
            m.op(AbstractOp::IMMEDIATE_STRING, [&](Code& c) {
                c.ident(str_ref->first);
            });
        }
        return str_ref->first;
    }

    template <>
    Error define<ast::StrLiteral>(Module& m, std::shared_ptr<ast::StrLiteral>& node) {
        auto str_ref = static_str(m, node);
        if (!str_ref) {
            return str_ref.error();
        }
        m.set_prev_expr(str_ref->value());
        return none;
    }

    template <>
    Error define<ast::TypeLiteral>(Module& m, std::shared_ptr<ast::TypeLiteral>& node) {
        auto s = define_storage(m, node->type_literal);
        if (!s) {
            return s.error();
        }
        auto new_id = m.new_node_id(node->type_literal);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::IMMEDIATE_TYPE, [&](Code& c) {
            c.ident(*new_id);
            c.type(*s);
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    template <>
    Error define<ast::Ident>(Module& m, std::shared_ptr<ast::Ident>& node) {
        auto ident = m.lookup_ident(node);
        if (!ident) {
            return ident.error();
        }
        if (auto prev_assign = m.prev_assign(ident->value())) {
            m.set_prev_expr(prev_assign);
        }
        else {
            m.set_prev_expr(ident->value());
        }
        return none;
    }

    template <>
    Error define<ast::Index>(Module& m, std::shared_ptr<ast::Index>& node) {
        auto base = get_expr(m, node->expr);
        if (!base) {
            return error("Invalid index target: {}", base.error().error());
        }
        auto index = get_expr(m, node->index);
        if (!index) {
            return error("Invalid index value: {}", index.error().error());
        }
        auto new_id = m.new_node_id(node);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::INDEX, [&](Code& c) {
            c.ident(*new_id);
            c.left_ref(*base);
            c.right_ref(*index);
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    template <>
    Error define<ast::MemberAccess>(Module& m, std::shared_ptr<ast::MemberAccess>& node) {
        auto base = get_expr(m, node->target);
        if (!base) {
            return error("Invalid member access target: {}", base.error().error());
        }
        if (node->member->usage == ast::IdentUsage::reference_builtin_fn) {
            if (node->member->ident == "length") {
                auto new_id = m.new_node_id(node->member);
                if (!new_id) {
                    return new_id.error();
                }
                m.op(AbstractOp::ARRAY_SIZE, [&](Code& c) {
                    c.ident(*new_id);
                    c.ref(*base);
                });
                m.set_prev_expr(new_id->value());
                return none;
            }
            return error("Unsupported builtin function: {}", node->member->ident);
        }
        auto ident = m.lookup_ident(node->member);
        if (!ident) {
            return ident.error();
        }
        auto new_id = m.new_node_id(node);
        if (!new_id) {
            return new_id.error();
        }
        m.op(AbstractOp::ACCESS, [&](Code& c) {
            c.ident(*new_id);
            c.left_ref(*base);
            c.right_ref(*ident);
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    expected<Varint> define_union(Module& m, Varint field_id, const std::shared_ptr<ast::StructUnionType>& su) {
        auto union_ident = m.new_node_id(su);
        if (!union_ident) {
            return union_ident;
        }
        m.op(AbstractOp::DEFINE_UNION, [&](Code& c) {
            c.ident(*union_ident);
            c.belong(field_id);
        });
        size_t index = 0;
        for (auto& st : su->structs) {
            auto ident = m.new_node_id(st);
            if (!ident) {
                return ident;
            }
            auto idx = varint(index);
            if (!idx) {
                return idx;
            }
            m.op(AbstractOp::DEFINE_UNION_MEMBER, [&](Code& c) {
                c.ident(*ident);
                c.int_value(*idx);
                c.belong(*union_ident);
            });
            m.map_struct(st, ident->value());
            for (auto& f : st->fields) {
                auto err = convert_node_definition(m, f);
                if (err) {
                    return unexpect_error(std::move(err));
                }
            }
            m.op(AbstractOp::END_UNION_MEMBER);
        }
        m.op(AbstractOp::END_UNION);
        return union_ident;
    }

    Error define_storage_internal(Module& m, Storages& s, const std::shared_ptr<ast::Type>& typ, bool should_detect_recursive) {
        auto push = [&](StorageType t, auto&& set) {
            Storage c;
            c.type = t;
            set(c);
            s.storages.push_back(c);
            s.length = *varint(s.storages.size());
        };
        auto typ_with_size = [&](StorageType t, auto&& typ) {
            if (!typ->bit_size) {
                return error("Invalid integer type(maybe bug)");
            }
            auto bit_size = varint(*typ->bit_size);
            if (!bit_size) {
                return bit_size.error();
            }
            push(t, [&](Storage& c) {
                c.size(*bit_size);
            });
            return none;
        };
        if (auto i = ast::as<ast::BoolType>(typ)) {
            push(StorageType::BOOL, [](Storage& c) {});
            return none;
        }
        if (auto i = ast::as<ast::IntType>(typ)) {
            return typ_with_size(i->is_signed ? StorageType::INT : StorageType::UINT, i);
        }
        if (auto i = ast::as<ast::IntLiteralType>(typ)) {
            return typ_with_size(StorageType::UINT, i);
        }
        if (auto f = ast::as<ast::FloatType>(typ)) {
            return typ_with_size(StorageType::FLOAT, f);
        }
        if (auto f = ast::as<ast::StrLiteralType>(typ)) {
            auto len = f->bit_size;
            if (!len) {
                return error("Invalid string literal type(maybe bug)");
            }
            push(StorageType::ARRAY, [&](Storage& c) {
                c.size(*varint(*len / 8));
            });
            push(StorageType::UINT, [&](Storage& c) {
                c.size(*varint(8));
            });
            return none;
        }
        if (auto i = ast::as<ast::IdentType>(typ)) {
            auto base_type = i->base.lock();
            if (!base_type) {
                return error("Invalid ident type(maybe bug)");
            }
            return define_storage_internal(m, s, base_type, should_detect_recursive);
        }
        if (auto i = ast::as<ast::StructType>(typ)) {
            auto l = i->base.lock();
            if (auto member = ast::as<ast::Member>(l)) {
                auto ident = m.lookup_ident(member->ident);
                if (!ident) {
                    return ident.error();
                }
                if (auto fmt = ast::as<ast::Format>(member); should_detect_recursive && fmt && fmt->body->struct_type->recursive) {
                    push(StorageType::RECURSIVE_STRUCT_REF, [&](Storage& c) {
                        c.ref(*ident);
                    });
                }
                else {
                    Varint bit_size_plus_1;
                    if (auto fmt = ast::as<ast::Format>(member); fmt && fmt->body->struct_type->bit_size) {
                        auto s = varint(*fmt->body->struct_type->bit_size + 1);
                        if (!s) {
                            return s.error();
                        }
                        bit_size_plus_1 = s.value();
                    }
                    push(StorageType::STRUCT_REF, [&](Storage& c) {
                        c.ref(*ident);
                        c.size(bit_size_plus_1);
                    });
                }
                return none;
            }
            return error("unknown struct type");
        }
        if (auto e = ast::as<ast::EnumType>(typ)) {
            auto base_enum = e->base.lock();
            if (!base_enum) {
                return error("Invalid enum type(maybe bug)");
            }
            auto ident = m.lookup_ident(base_enum->ident);
            if (!ident) {
                return ident.error();
            }
            push(StorageType::ENUM, [&](Storage& c) {
                c.ref(*ident);
            });
            auto base_type = base_enum->base_type;
            if (!base_type) {
                return none;  // this is abstract enum
            }
            return define_storage_internal(m, s, base_type, should_detect_recursive);
        }
        if (auto su = ast::as<ast::StructUnionType>(typ)) {
            if (!m.union_ref) {
                return error("Invalid union type(maybe bug)");
            }
            auto union_ident = m.union_ref;
            m.union_ref = std::nullopt;
            auto size = su->structs.size();
            push(StorageType::VARIANT, [&](Storage& c) {
                c.size(*varint(size));
                c.ref(*union_ident);
            });
            for (auto& st : su->structs) {
                auto ident = m.lookup_struct(st);
                if (ident.value() == null_id) {
                    return error("Invalid struct ident(maybe bug)");
                }
                Varint bit_size_plus_1;
                if (st->bit_size) {
                    auto s = varint(*st->bit_size + 1);
                    if (!s) {
                        return s.error();
                    }
                    bit_size_plus_1 = s.value();
                }
                push(StorageType::STRUCT_REF, [&](Storage& c) {
                    c.ref(ident);
                    c.size(bit_size_plus_1);
                });
            }
            return none;
        }
        if (auto a = ast::as<ast::ArrayType>(typ)) {
            if (a->length_value) {
                push(StorageType::ARRAY, [&](Storage& c) {
                    c.size(*varint(*a->length_value));
                });
            }
            else {
                push(StorageType::VECTOR, [&](Storage& c) {});
                should_detect_recursive = false;
            }
            auto err = define_storage_internal(m, s, a->element_type, should_detect_recursive);
            if (err) {
                return err;
            }
            return none;
        }
        return error("unsupported type on define storage: {}", node_type_to_string(typ->node_type));
    }

    expected<StorageRef> define_storage(Module& m, const std::shared_ptr<ast::Type>& typ, bool should_detect_recursive) {
        Storages s;
        auto err = define_storage_internal(m, s, typ, should_detect_recursive);
        if (err) {
            return unexpect_error(std::move(err));
        }
        return m.get_storage_ref(std::move(s), &typ->loc);
    }

    expected<Varint> define_expr_variable(Module& m, const std::shared_ptr<ast::Expr>& cond) {
        auto cond_ = get_expr(m, cond);
        if (!cond_) {
            return unexpect_error("Invalid union field condition");
        }
        auto ref = define_storage(m, cond->expr_type);
        if (!ref) {
            return unexpect_error(std::move(ref.error()));
        }
        return define_typed_tmp_var(m, cond_.value(), *ref, ast::ConstantLevel::immutable_variable);
    }

    Error handle_union_type(Module& m, const std::shared_ptr<ast::UnionType>& ty, auto&& block) {
        std::optional<Varint> base_cond;
        if (auto cond = ty->cond.lock()) {
            auto tmp_var = define_expr_variable(m, cond);
            if (!tmp_var) {
                return tmp_var.error();
            }
            base_cond = tmp_var.value();
        }
        std::optional<Varint> prev_cond;
        for (auto& c : ty->candidates) {
            auto cond = c->cond.lock();
            auto field = c->field.lock();
            if (cond && !ast::is_any_range(cond)) {
                auto maybe_range = ast::as<ast::Range>(ast::as<ast::Identity>(cond)->expr);
                Varint origCond;
                if (maybe_range) {
                    if (!base_cond) {
                        return error("Invalid union field condition");
                    }
                    auto err = do_range_compare(m, BinaryOp::equal, maybe_range, *base_cond);
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
                    auto cond2 = get_expr(m, cond);
                    if (!cond2) {
                        return error("Invalid union field condition");
                    }
                    origCond = cond2.value();
                    if (base_cond) {
                        BM_BINARY(m.op, new_id, BinaryOp::equal, *base_cond, origCond);
                        origCond = new_id;
                    }
                }
                auto cond_ = origCond;
                if (prev_cond) {
                    auto new_id_2 = m.new_id(nullptr);
                    if (!new_id_2) {
                        return new_id_2.error();
                    }
                    m.op(AbstractOp::NOT_PREV_THEN, [&](Code& c) {
                        c.ident(*new_id_2);
                        c.left_ref(*prev_cond);
                        c.right_ref(cond_);
                    });
                    cond_ = *new_id_2;
                }
                auto err = block(cond_, origCond, field);
                if (err) {
                    return err;
                }
                prev_cond = cond_;
            }
            else {
                auto imm_true = immediate_bool(m, true);
                if (!imm_true) {
                    return imm_true.error();
                }
                if (prev_cond) {
                    auto new_id = m.new_id(nullptr);
                    if (!new_id) {
                        return new_id.error();
                    }
                    m.op(AbstractOp::NOT_PREV_THEN, [&](Code& c) {
                        c.ident(*new_id);
                        c.left_ref(*prev_cond);
                        c.right_ref(*imm_true);
                    });
                    auto err = block(*new_id, *imm_true, field);
                    if (err) {
                        return err;
                    }
                }
                else {
                    auto err = block(*imm_true, *imm_true, field);
                    if (err) {
                        return err;
                    }
                }
            }
        }
        return none;
    }

    template <>
    Error define<ast::Available>(Module& m, std::shared_ptr<ast::Available>& node) {
        Varint new_id;
        Varint base_expr;
        if (auto memb = ast::as<ast::MemberAccess>(node->target)) {
            auto expr = get_expr(m, memb->target);
            if (!expr) {
                return expr.error();
            }
            base_expr = expr.value();
        }
        if (auto union_ty = ast::as<ast::UnionType>(node->target->expr_type)) {
            auto imm_false = immediate_bool(m, false);
            if (!imm_false) {
                return imm_false.error();
            }
            auto tmp_var = define_bool_tmp_var(m, *imm_false, ast::ConstantLevel::variable);
            auto imm_true = immediate_bool(m, true);
            if (!imm_true) {
                return imm_true.error();
            }
            bool prev = false;
            auto err = handle_union_type(m, ast::cast_to<ast::UnionType>(node->target->expr_type), [&](Varint, Varint cond, std::shared_ptr<ast::Field>& field) {
                BM_BEGIN_COND_BLOCK(m.op, m.code, cond_block, &node->loc);
                BM_NEW_NODE_ID(id, error, node);
                m.op(AbstractOp::FIELD_AVAILABLE, [&](Code& c) {
                    c.ident(id);
                    c.left_ref(base_expr);
                    c.right_ref(cond);
                });
                BM_END_COND_BLOCK(m.op, m.code, cond_block, id);
                if (prev) {
                    m.next_phi_candidate(cond_block.value());
                }
                else {
                    m.init_phi_stack(cond_block.value());
                }
                BM_REF(m.op, prev ? AbstractOp::ELIF : AbstractOp::IF, cond_block);
                if (field) {
                    auto err = do_assign(m, nullptr, nullptr, *tmp_var, *imm_true);
                    if (err) {
                        return err;
                    }
                }
                prev = true;
                return none;
            });
            if (prev) {
                m.op(AbstractOp::END_IF);
                auto err = insert_phi(m, m.end_phi_stack());
                if (err) {
                    return err;
                }
            }
            if (err) {
                return err;
            }
            auto prev_assign = m.prev_assign(tmp_var->value());
            new_id = *varint(prev_assign);
        }
        else {
            auto imm = immediate_bool(m, ast::as<ast::Ident>(node->target) || ast::as<ast::MemberAccess>(node->target));
            if (!imm) {
                return imm.error();
            }
            new_id = *imm;
        }
        m.set_prev_expr(new_id.value());
        return none;
    }

    Error define_union_field(Module& m, const std::shared_ptr<ast::UnionType>& ty, Varint belong) {
        Param param;
        auto err = handle_union_type(m, ty, [&](Varint cond, Varint, auto&& field) {
            if (!field) {
                return none;
            }
            auto ident = m.lookup_ident(field->ident);
            if (!ident) {
                return ident.error();
            }
            auto cond_field_id = m.new_id(nullptr);
            if (!cond_field_id) {
                return cond_field_id.error();
            }
            m.op(AbstractOp::CONDITIONAL_FIELD, [&](Code& c) {
                c.ident(*cond_field_id);
                c.left_ref(cond);
                c.right_ref(*ident);
                c.belong(belong);
            });
            param.refs.push_back(*cond_field_id);
            return none;
        });
        if (err) {
            return err;
        }
        if (ty->common_type) {
            auto s = define_storage(m, ty->common_type, true);
            if (err) {
                return err;
            }
            auto len = varint(param.refs.size());
            if (!len) {
                return len.error();
            }
            param.len = *len;
            auto ident = m.new_id(nullptr);
            if (!ident) {
                return ident.error();
            }
            m.op(AbstractOp::MERGED_CONDITIONAL_FIELD, [&](Code& c) {  // all common type
                c.ident(*ident);
                c.type(*s);
                c.param(std::move(param));
                c.belong(belong);
                c.merge_mode(MergeMode::COMMON_TYPE);
            });
        }
        return none;
    }

    template <>
    Error define<ast::Assert>(Module& m, std::shared_ptr<ast::Assert>& node) {
        auto cond = get_expr(m, node->cond);
        if (!cond) {
            return cond.error();
        }
        m.op(AbstractOp::ASSERT, [&](Code& c) {
            c.ref(*cond);
            c.belong(m.get_function());
        });
        return none;
    }

    template <>
    Error define<ast::ExplicitError>(Module& m, std::shared_ptr<ast::ExplicitError>& node) {
        Param param;
        for (auto& c : node->base->arguments) {
            auto err = get_expr(m, c);
            if (!err) {
                return err.error();
            }
            param.refs.push_back(*err);
        }
        auto len = varint(param.refs.size());
        if (!len) {
            return len.error();
        }
        param.len = *len;
        m.op(AbstractOp::EXPLICIT_ERROR, [&](Code& c) {
            c.param(std::move(param));
            c.belong(m.get_function());
        });
        return none;
    }

    template <>
    Error define<ast::Call>(Module& m, std::shared_ptr<ast::Call>& node) {
        auto callee = get_expr(m, node->callee);
        if (!callee) {
            return callee.error();
        }
        std::vector<Varint> args;
        for (auto& p : node->arguments) {
            auto arg = get_expr(m, p);
            if (!arg) {
                return arg.error();
            }
            args.push_back(arg.value());
        }
        auto new_id = m.new_node_id(node);
        if (!new_id) {
            return new_id.error();
        }
        auto len = varint(args.size());
        if (!len) {
            return len.error();
        }
        Param param;
        param.len = *len;
        param.refs = std::move(args);
        m.op(AbstractOp::CALL, [&](Code& c) {
            c.ident(*new_id);
            c.ref(*callee);
            c.param(std::move(param));
        });
        m.set_prev_expr(new_id->value());
        return none;
    }

    bool is_alignment_vector(ast::Field* t) {
        if (!t) {
            return false;
        }
        if (auto arr = ast::as<ast::ArrayType>(t->field_type)) {
            auto elm_is_int = ast::as<ast::IntType>(arr->element_type);
            if (elm_is_int && !elm_is_int->is_signed &&
                elm_is_int->bit_size == 8 &&
                t->arguments &&
                t->arguments->alignment_value &&
                *t->arguments->alignment_value != 0 &&
                *t->arguments->alignment_value % 8 == 0) {
                return true;
            }
        }
        return false;
    }

    template <>
    Error define<ast::Field>(Module& m, std::shared_ptr<ast::Field>& node) {
        if (!node->ident) {
            auto temporary_name = std::make_shared<ast::Ident>(node->loc, std::format("field{}", m.object_id));
            temporary_name->base = node;
            node->ident = temporary_name;
        }
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        // maybe null
        auto parent = m.lookup_struct(node->belong_struct.lock());
        if (auto union_struct = ast::as<ast::StructUnionType>(node->field_type)) {
            auto union_ident_ref = define_union(m, *ident, ast::cast_to<ast::StructUnionType>(node->field_type));
            if (!union_ident_ref) {
                return union_ident_ref.error();
            }
            m.union_ref = union_ident_ref.value();
            auto s = define_storage(m, node->field_type, true);
            if (!s) {
                return s.error();
            }
            m.op(AbstractOp::DEFINE_FIELD, [&](Code& c) {
                c.ident(*ident);
                c.belong(parent);
                c.type(*s);
            });
        }
        else {
            if (!ast::as<ast::UnionType>(node->field_type)) {
                StorageRef type;
                if (is_alignment_vector(node.get())) {
                    // alignment vector should have alignment byte - 1 array
                    Storages s;
                    s.length.value(2);
                    s.storages.push_back(Storage{.type = StorageType::ARRAY});
                    s.storages.push_back(Storage{.type = StorageType::UINT});
                    s.storages[0].size(*varint(*node->arguments->alignment_value / 8 - 1));
                    s.storages[1].size(*varint(8));
                    auto ref = m.get_storage_ref(std::move(s), &node->loc);
                    if (!ref) {
                        return ref.error();
                    }
                    type = *ref;
                }
                else {
                    auto s = define_storage(m, node->field_type, true);
                    if (!s) {
                        return s.error();
                    }
                    type = *s;
                }
                m.op(AbstractOp::DEFINE_FIELD, [&](Code& c) {
                    c.ident(*ident);
                    c.belong(parent);
                    c.type(type);
                });
            }
            else {
                m.op(AbstractOp::DEFINE_PROPERTY, [&](Code& c) {
                    c.ident(*ident);
                    c.belong(parent);
                });
                auto err = define_union_field(m, ast::cast_to<ast::UnionType>(node->field_type), *ident);
                if (err) {
                    return err;
                }
                m.op(AbstractOp::END_PROPERTY);
            }
        }

        return none;
    }

    template <>
    Error define<ast::Function>(Module& m, std::shared_ptr<ast::Function>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        Varint parent;
        if (auto p = node->belong.lock()) {
            auto parent_ = m.lookup_ident(p->ident);
            if (!parent_) {
                return parent_.error();
            }
            parent = *parent_;
        }

        bool is_encoder = false, is_decoder = false;
        if (auto fmt = ast::as<ast::Format>(node->belong.lock())) {
            if (fmt->encode_fn.lock() == node) {
                is_encoder = true;
            }
            if (fmt->decode_fn.lock() == node) {
                is_decoder = true;
            }
        }
        bool is_encoder_or_decoder = is_encoder || is_decoder;
        if (is_encoder) {
            m.on_encode_fn = true;
        }
        if (is_decoder) {
            m.on_encode_fn = false;
        }
        m.op(AbstractOp::DEFINE_FUNCTION, [&](Code& c) {
            c.ident(*ident);
            c.belong(parent);
            if (is_encoder_or_decoder) {
                c.func_type(is_encoder ? FunctionType::ENCODE : FunctionType::DECODE);
            }
            else {
                if (parent.value() == null_id || m.code[m.ident_index_table[parent.value()]].op == rebgn::AbstractOp::DEFINE_PROGRAM) {
                    c.func_type(FunctionType::FREE);
                }
                else {
                    if (node->is_cast) {
                        c.func_type(FunctionType::CAST);
                    }
                    else {
                        c.func_type(FunctionType::MEMBER);
                    }
                }
            }
        });
        if (node->return_type && !ast::as<ast::VoidType>(node->return_type)) {
            Storages s;
            if (is_encoder_or_decoder) {
                s.storages.push_back(Storage{.type = StorageType::CODER_RETURN});
                s.length.value(1);
            }
            auto err = define_storage_internal(m, s, node->return_type, false);
            if (err) {
                return err;
            }
            auto ref = m.get_storage_ref(std::move(s), &node->return_type->loc);
            if (!ref) {
                return ref.error();
            }
            m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
                c.type(*ref);
            });
        }
        else if (is_encoder_or_decoder) {
            auto ref = m.get_storage_ref(Storages{
                                             .length = varint(1).value(),
                                             .storages = {Storage{.type = StorageType::CODER_RETURN}},
                                         },
                                         &node->loc);
            if (!ref) {
                return ref.error();
            }
            m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
                c.type(*ref);
            });
        }
        for (auto& p : node->parameters) {
            auto param_ident = m.lookup_ident(p->ident);
            if (!param_ident) {
                return param_ident.error();
            }
            auto s = define_storage(m, p->field_type);
            if (!s) {
                return s.error();
            }
            m.op(AbstractOp::DEFINE_PARAMETER, [&](Code& c) {
                c.ident(*param_ident);
                c.belong(*ident);
                c.type(*s);
            });
        }
        m.init_phi_stack(0);  // temporary
        auto old = m.enter_function(*ident);
        auto err = foreach_node(m, node->body->elements, [&](auto& n) {
            return convert_node_definition(m, n);
        });
        old.execute();
        if (err) {
            return err;
        }
        if (is_encoder_or_decoder) {
            m.op(AbstractOp::RET_SUCCESS, [&](Code& c) {
                c.belong(*ident);
            });
        }
        m.op(AbstractOp::END_FUNCTION);
        m.end_phi_stack();  // restore
        return none;
    }

    template <>
    Error define<ast::State>(Module& m, std::shared_ptr<ast::State>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_STATE, [&](Code& c) {
            c.ident(*ident);
        });
        m.map_struct(node->body->struct_type, ident->value());
        for (auto& f : node->body->struct_type->fields) {
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
        }
        m.op(AbstractOp::END_STATE);
        return none;
    }

    template <>
    Error define<ast::Format>(Module& m, std::shared_ptr<ast::Format>& node) {
        auto ident = m.lookup_ident(node->ident);
        if (!ident) {
            return ident.error();
        }
        m.op(AbstractOp::DEFINE_FORMAT, [&](Code& c) {
            c.ident(*ident);
        });
        m.map_struct(node->body->struct_type, ident->value());
        if (node->body->struct_type->type_map) {
            auto s = define_storage(m, node->body->struct_type->type_map->type_literal, true);
            if (!s) {
                return s.error();
            }
            m.op(AbstractOp::RETURN_TYPE, [&](Code& c) {
                c.type(*s);
            });
        }
        std::vector<std::shared_ptr<ast::Field>> bit_fields;
        std::unordered_map<std::shared_ptr<ast::Node>, Varint> bit_field_begin;
        std::unordered_set<std::shared_ptr<ast::Node>> bit_field_end;
        for (auto& f : node->body->struct_type->fields) {
            if (auto field = ast::as<ast::Field>(f)) {
                if (field->bit_alignment != field->eventual_bit_alignment) {
                    bit_fields.push_back(ast::cast_to<ast::Field>(f));
                    continue;
                }
                if (bit_fields.size()) {
                    bit_fields.push_back(ast::cast_to<ast::Field>(f));
                    PackedOpType type = PackedOpType::FIXED;
                    for (auto& bf : bit_fields) {
                        if (!bf->field_type->bit_size) {
                            type = PackedOpType::VARIABLE;
                            break;
                        }
                    }
                    std::string ident_concat = "bit_field";
                    for (auto& bf : bit_fields) {
                        ident_concat += "_";
                        if (!bf->ident) {
                            bf->ident = std::make_shared<ast::Ident>(bf->loc, std::format("field{}", m.object_id));
                            bf->ident->base = bf;
                            m.lookup_ident(bf->ident);  // register
                        }
                        ident_concat += bf->ident->ident;
                    }
                    auto temporary_name = std::make_shared<ast::Ident>(node->loc, ident_concat);
                    temporary_name->base = node;
                    auto begin_field = std::move(bit_fields.front());
                    auto end_field = std::move(bit_fields.back());
                    auto id = m.lookup_ident(temporary_name);
                    if (!id) {
                        return id.error();
                    }
                    if (auto b = ast::as<ast::StructUnionType>(begin_field->field_type)) {
                        bit_field_begin.emplace(b->base.lock(), *id);  // if or match
                    }
                    if (auto b = ast::as<ast::StructUnionType>(end_field->field_type)) {
                        bit_field_end.insert(b->base.lock());  // if or match
                    }
                    m.bit_field_variability.emplace(begin_field, type);
                    bit_field_begin.emplace(std::move(begin_field), *id);
                    bit_field_end.insert(std::move(end_field));
                    bit_fields.clear();
                }
            }
        }
        for (auto& f : node->body->struct_type->fields) {
            if (auto ft = ast::as<ast::Field>(f)) {
                if (auto found = bit_field_begin.find(ast::cast_to<ast::Field>(f));
                    found != bit_field_begin.end()) {
                    m.op(AbstractOp::DEFINE_BIT_FIELD, [&](Code& c) {
                        c.ident(found->second);
                        c.belong(ident.value());
                        // type will be filled later
                    });
                    m.map_struct(node->body->struct_type, found->second.value());  // temporary remap
                }
            }
            auto err = convert_node_definition(m, f);
            if (err) {
                return err;
            }
            if (ast::as<ast::Field>(f)) {
                if (bit_field_end.contains(ast::cast_to<ast::Field>(f))) {
                    m.op(AbstractOp::END_BIT_FIELD);
                    m.map_struct(node->body->struct_type, ident->value());  // restore
                }
            }
        }
        m.op(AbstractOp::END_FORMAT);
        m.bit_field_begin.insert(bit_field_begin.begin(), bit_field_begin.end());
        m.bit_field_end.insert(bit_field_end.begin(), bit_field_end.end());
        return none;
    }

    template <>
    Error define<ast::Enum>(Module& m, std::shared_ptr<ast::Enum>& node) {
        BM_LOOKUP_IDENT(ident, m, node->ident);
        StorageRef base_type;  // base type or null
        if (node->base_type) {
            BM_DEFINE_STORAGE(s, m, node->base_type);
            base_type = s;
        }
        m.op(AbstractOp::DEFINE_ENUM, [&](Code& c) {
            c.ident(ident);
            c.type(base_type);
        });
        for (auto& me : node->members) {
            BM_LOOKUP_IDENT(mident, m, me->ident);
            BM_GET_EXPR(ref, m, me->value);
            Varint str_ref;
            if (me->str_literal) {
                auto st = static_str(m, me->str_literal);
                if (!st) {
                    return st.error();
                }
                str_ref = *st;
            }
            m.op(AbstractOp::DEFINE_ENUM_MEMBER, [&](Code& c) {
                c.ident(mident);
                c.left_ref(ref);
                c.right_ref(str_ref);
                c.belong(ident);
            });
        }
        m.op(AbstractOp::END_ENUM);
        return none;
    }

    template <>
    Error define<ast::Paren>(Module& m, std::shared_ptr<ast::Paren>& node) {
        return convert_node_definition(m, node->expr);
    }

    template <>
    Error define<ast::Unary>(Module& m, std::shared_ptr<ast::Unary>& node) {
        auto ident = m.new_node_id(node);
        if (!ident) {
            return ident.error();
        }
        auto target = get_expr(m, node->expr);
        if (!target) {
            return error("Invalid unary expression");
        }
        auto uop = UnaryOp(node->op);
        if (uop == UnaryOp::logical_not) {
            if (!ast::as<ast::BoolType>(node->expr->expr_type)) {
                uop = UnaryOp::bit_not;
            }
        }
        m.op(AbstractOp::UNARY, [&](Code& c) {
            c.ident(*ident);
            c.uop(uop);
            c.ref(*target);
        });
        m.set_prev_expr(ident->value());
        return none;
    }

    template <>
    Error define<ast::Cond>(Module& m, std::shared_ptr<ast::Cond>& node) {
        BM_DEFINE_STORAGE(s, m, node->expr_type);
        BM_NEW_OBJECT(m.op, new_id, s);
        auto tmp_var = define_typed_tmp_var(m, new_id, s, ast::ConstantLevel::variable);
        if (!tmp_var) {
            return tmp_var.error();
        }
        auto target_type = m.get_storage(s);
        if (!target_type) {
            return target_type.error();
        }
        BM_COND_IN_BLOCK(m.op, m.code, cond_block, node->cond);
        m.init_phi_stack(cond_block.value());
        BM_REF(m.op, AbstractOp::IF, cond_block);
        BM_GET_EXPR(then, m, node->then);
        auto then_type = may_get_type(m, node->then->expr_type);
        if (!then_type) {
            return then_type.error();
        }
        auto err = do_assign(m, &*target_type, *then_type ? &**then_type : nullptr, *tmp_var, then);
        if (err) {
            return err;
        }
        m.next_phi_candidate(null_id);
        m.op(AbstractOp::ELSE);
        BM_GET_EXPR(els, m, node->els);
        auto els_type = may_get_type(m, node->els->expr_type);
        if (!els_type) {
            return els_type.error();
        }
        err = do_assign(m, &*target_type, *els_type ? &**els_type : nullptr, *tmp_var, els);
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_IF);
        err = insert_phi(m, m.end_phi_stack());
        if (err) {
            return err;
        }
        auto prev = m.prev_assign(tmp_var->value());
        m.set_prev_expr(prev);
        return none;
    }

    template <>
    Error define<ast::Cast>(Module& m, std::shared_ptr<ast::Cast>& node) {
        std::vector<Varint> args;
        for (auto& p : node->arguments) {
            BM_GET_EXPR(arg, m, p);
            args.push_back(arg);
        }
        BM_DEFINE_STORAGE(s, m, node->expr_type);
        if (args.size() == 0) {
            BM_NEW_OBJECT(m.op, ident, s);
            m.set_prev_expr(ident.value());
        }
        else if (args.size() == 1) {
            auto expr_type = may_get_type(m, node->arguments[0]->expr_type);
            if (!expr_type) {
                return expr_type.error();
            }
            if (!*expr_type) {
                return error("Invalid cast expression");
            }
            auto from = m.get_storage_ref(**expr_type, &node->loc);
            if (!from) {
                return from.error();
            }
            auto to = m.get_storage(s);
            if (!to) {
                return to.error();
            }
            auto c = add_assign_cast(m, [&](auto&&... args) { return m.op(args...); }, &*to, &**expr_type, args[0]);
            if (!c) {
                return c.error();
            }
            if (*c) {
                m.set_prev_expr((**c).value());
            }
            else {
                m.set_prev_expr(args[0].value());
            }
        }
        else {
            BM_NEW_NODE_ID(ident, error, node);
            Param param;
            param.len = varint(args.size()).value();
            param.refs = std::move(args);
            m.op(AbstractOp::CALL_CAST, [&](Code& c) {
                c.ident(ident);
                c.type(s);
                c.param(std::move(param));
            });
            m.set_prev_expr(ident.value());
        }

        return none;
    }

    expected<std::optional<Storages>> may_get_type(Module& m, const std::shared_ptr<ast::Type>& typ) {
        std::optional<Storages> s;
        if (auto u = ast::as<ast::UnionType>(typ)) {
            if (u->common_type) {
                s = Storages();
                auto err = define_storage_internal(m, s.value(), u->common_type, true);
                if (err) {
                    return unexpect_error(std::move(err));
                }
            }
        }
        else {
            s = Storages();
            auto err = define_storage_internal(m, s.value(), typ, true);
            if (err) {
                return unexpect_error(std::move(err));
            }
        }
        return s;
    }

    bool should_be_recursive_struct_assign(const std::shared_ptr<ast::Node>& node) {
        if (auto ident = ast::as<ast::Ident>(node)) {
            auto [base, _] = *ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
            if (auto field = ast::as<ast::Field>(base->base.lock())) {
                // on dynamic array, we should not detect it as recursive struct assign
                if (auto arr = ast::as<ast::ArrayType>(field->field_type); arr && !arr->length_value) {
                    return false;
                }
                return true;  // fixed array of recursive struct or direct recursive struct
            }
        }
        else if (auto ident = ast::as<ast::Index>(node)) {
            return should_be_recursive_struct_assign(ident->expr);
        }
        else if (auto member = ast::as<ast::MemberAccess>(node)) {
            return should_be_recursive_struct_assign(member->base.lock());
        }
        return false;
    }

    bool is_range_compare(const std::shared_ptr<ast::Node>& node) {
        if (auto binary = ast::as<ast::Binary>(node)) {
            if (binary->op == ast::BinaryOp::equal || binary->op == ast::BinaryOp::not_equal) {
                if (ast::as<ast::Range>(binary->left) || ast::as<ast::Range>(binary->right)) {
                    return true;
                }
            }
        }
        return false;
    }

    Error do_range_compare(Module& m, BinaryOp op, ast::Range* range, Varint expr_) {
        std::optional<Varint> start, end;
        if (range->start) {
            BM_GET_EXPR(start_, m, range->start);
            start = start_;
        }
        if (range->end) {
            BM_GET_EXPR(end_, m, range->end);
            end = end_;
        }
        Varint new_id;
        if (start) {
            BM_BINARY(m.op, id, BinaryOp::less_or_eq, *start, expr_);
            new_id = id;
        }
        if (end) {
            BM_BINARY(m.op, id, BinaryOp::less_or_eq, expr_, *end);
            if (new_id.value() != 0) {
                BM_BINARY(m.op, new_id_2, BinaryOp::logical_and, new_id, id);
                new_id = new_id_2;
            }
            else {
                new_id = id;
            }
        }
        if (op == BinaryOp::not_equal) {
            BM_UNARY(m.op, new_id_2, UnaryOp::logical_not, new_id);
            new_id = new_id_2;
        }
        m.set_prev_expr(new_id.value());
        return none;
    }

    Error do_range_compare(Module& m, BinaryOp op, std::shared_ptr<ast::Expr>& left, std::shared_ptr<ast::Expr>& right) {
        assert(op == BinaryOp::equal || op == BinaryOp::not_equal);
        assert(ast::as<ast::Range>(left) || ast::as<ast::Range>(right));
        if (ast::as<ast::Range>(left) && ast::as<ast::Range>(right)) {
            return error("Currently, range comparison is not supported");
        }
        auto l_range = ast::as<ast::Range>(left);
        auto r_range = ast::as<ast::Range>(right);
        auto do_compare = [&](ast::Range* range, const std::shared_ptr<ast::Expr>& expr) {
            auto expr_ = get_expr(m, expr);
            if (!expr_) {
                return expr_.error();
            }
            return do_range_compare(m, op, range, expr_.value());
        };
        if (l_range) {
            return do_compare(l_range, right);
        }
        else {
            return do_compare(r_range, left);
        }
    }

    Error do_OrCond(Module& m, const std::shared_ptr<ast::Expr>& base, const std::shared_ptr<ast::OrCond>& cond) {
        return none;
    }

    template <>
    Error define<ast::Binary>(Module& m, std::shared_ptr<ast::Binary>& node) {
        if (node->op == ast::BinaryOp::define_assign ||
            node->op == ast::BinaryOp::const_assign) {
            auto ident = ast::cast_to<ast::Ident>(node->left);
            BM_LOOKUP_IDENT(ident_, m, ident);
            BM_GET_EXPR(right_ref, m, node->right);
            BM_DEFINE_STORAGE(typ, m, node->left->expr_type, true);
            if (node->right->constant_level == ast::ConstantLevel::constant &&
                node->op == ast::BinaryOp::const_assign) {
                m.op(AbstractOp::DEFINE_CONSTANT, [&](Code& c) {
                    c.ident(ident_);
                    c.ref(right_ref);
                    c.type(typ);
                });
            }
            else {
                auto res = define_var(m, ident_, right_ref, typ, node->right->constant_level);
                if (!res) {
                    return res.error();
                }
            }
            return none;
        }
        if (node->op == ast::BinaryOp::append_assign) {
            auto idx = ast::cast_to<ast::Index>(node->left);
            if (!idx) {
                return error("Invalid append assign expression");
            }
            auto left_ref = get_expr(m, idx->expr);
            if (!left_ref) {
                return error("Invalid binary expression");
            }
            auto right_ref = get_expr(m, node->right);
            if (!right_ref) {
                return error("Invalid binary expression");
            }
            m.op(AbstractOp::APPEND, [&](Code& c) {
                c.left_ref(*left_ref);
                c.right_ref(*right_ref);
            });
            return none;
        }
        if (is_range_compare(node)) {
            return do_range_compare(m, BinaryOp(node->op), node->left, node->right);
        }
        auto left_ref = get_expr(m, node->left);
        if (!left_ref) {
            return error("Invalid binary expression: {}", left_ref.error().error());
        }
        auto right_ref = get_expr(m, node->right);
        if (!right_ref) {
            return error("Invalid binary expression");
        }
        if (node->op == ast::BinaryOp::comma) {  // comma operator
            m.set_prev_expr(right_ref->value());
            return none;
        }
        auto handle_assign = [&](Varint right_ref) -> Error {
            auto left_type = may_get_type(m, node->left->expr_type);
            if (!left_type) {
                return left_type.error();
            }
            auto right_type = may_get_type(m, node->right->expr_type);
            if (!right_type) {
                return right_type.error();
            }
            return do_assign(m,
                             *left_type ? &**left_type : nullptr,
                             *right_type ? &**right_type : nullptr,
                             *left_ref, right_ref,
                             should_be_recursive_struct_assign(node->left), &node->loc);
        };
        if (node->op == ast::BinaryOp::assign) {
            return handle_assign(*right_ref);
        }
        /*
        auto ident = m.new_node_id(node);
        if (!ident) {
            return ident.error();
        }
        */
        auto bop = BinaryOp(node->op);
        bool should_assign = false;
        switch (bop) {
            case BinaryOp::div_assign:
                bop = BinaryOp::div;
                should_assign = true;
                break;
            case BinaryOp::mul_assign:
                bop = BinaryOp::mul;
                should_assign = true;
                break;
            case BinaryOp::mod_assign:
                bop = BinaryOp::mod;
                should_assign = true;
                break;
            case BinaryOp::add_assign:
                bop = BinaryOp::add;
                should_assign = true;
                break;
            case BinaryOp::sub_assign:
                bop = BinaryOp::sub;
                should_assign = true;
                break;
            case BinaryOp::left_logical_shift_assign:
                bop = BinaryOp::left_logical_shift;
                should_assign = true;
                break;
            case BinaryOp::right_logical_shift_assign:
                bop = BinaryOp::right_logical_shift;
                should_assign = true;
                break;
            case BinaryOp::right_arithmetic_shift_assign:
                bop = BinaryOp::right_arithmetic_shift;
                should_assign = true;
                break;
            case BinaryOp::left_arithmetic_shift_assign:  // TODO(on-keyday): is this needed?
                bop = BinaryOp::left_arithmetic_shift;
                should_assign = true;
                break;
            case BinaryOp::bit_and_assign:
                bop = BinaryOp::bit_and;
                should_assign = true;
                break;
            case BinaryOp::bit_or_assign:
                bop = BinaryOp::bit_or;
                should_assign = true;
                break;
            case BinaryOp::bit_xor_assign:
                bop = BinaryOp::bit_xor;
                should_assign = true;
                break;
            default:
                break;
        }
        BM_BINARY_NODE(m.op, ident, bop, *left_ref, *right_ref, node);
        if (should_assign) {
            auto err = handle_assign(ident);
            if (err) {
                return err;
            }
        }
        else {
            m.set_prev_expr(ident.value());
        }
        return none;
    }

    template <>
    Error define<ast::Identity>(Module& m, std::shared_ptr<ast::Identity>& node) {
        return convert_node_definition(m, node->expr);
    }

    template <>
    Error define<ast::SpecifyOrder>(Module& m, std::shared_ptr<ast::SpecifyOrder>& node) {
        if (node->order_type == ast::OrderType::byte) {
            if (node->order_value) {
                if (node->order_value == 0) {
                    m.set_endian(Endian::big);
                }
                else if (node->order_value == 1) {
                    m.set_endian(Endian::little);
                }
                else if (node->order_value == 2) {
                    m.set_endian(Endian::native);
                }
                else {
                    return error("Invalid endian value");
                }
            }
            else {
                auto expr = get_expr(m, node->order);
                if (!expr) {
                    return expr.error();
                }
                auto new_id = m.new_node_id(node);
                if (!new_id) {
                    return new_id.error();
                }
                m.op(AbstractOp::DYNAMIC_ENDIAN, [&](Code& c) {
                    c.ident(*new_id);
                    c.ref(*expr);
                });
                m.set_endian(Endian::dynamic, new_id->value());
            }
        }
        return none;
    }

    template <>
    Error define<ast::Metadata>(Module& m, std::shared_ptr<ast::Metadata>& node) {
        auto node_name = m.lookup_metadata(node->name, &node->loc);
        if (!node_name) {
            return node_name.error();
        }
        std::vector<Varint> values;
        for (auto& value : node->values) {
            auto val = get_expr(m, value);
            if (!val) {
                return val.error();
            }
            values.push_back(*val);
        }
        Metadata md;
        md.name = *node_name;
        md.refs = std::move(values);
        auto length = varint(md.refs.size());
        if (!length) {
            return length.error();
        }
        md.len = length.value();
        m.op(AbstractOp::METADATA, [&](Code& c) {
            c.metadata(std::move(md));
        });
        return none;
    }

    template <>
    Error define<ast::Program>(Module& m, std::shared_ptr<ast::Program>& node) {
        auto pid = m.new_node_id(node);
        if (!pid) {
            return pid.error();
        }
        m.op(AbstractOp::DEFINE_PROGRAM, [&](Code& c) {
            c.ident(*pid);
        });
        m.map_struct(node->struct_type, pid->value());
        auto err = foreach_node(m, node->elements, [&](auto& n) {
            return convert_node_definition(m, n);
        });
        if (err) {
            return err;
        }
        m.op(AbstractOp::END_PROGRAM);
        return none;
    }

    template <>
    Error define<ast::ImplicitYield>(Module& m, std::shared_ptr<ast::ImplicitYield>& node) {
        return convert_node_definition(m, node->expr);
    }

    template <class T>
    concept has_define = requires(Module& m, std::shared_ptr<T>& n) {
        define<T>(m, n);
    };

    Error convert_node_definition(Module& m, const std::shared_ptr<ast::Node>& n) {
        Error err;
        ast::visit(n, [&](auto&& node) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(node)>, std::shared_ptr>::template param_at<0>;
            if constexpr (has_define<T>) {
                err = define<T>(m, node);
            }
        });
        return err;
    }

    expected<Module> convert(std::shared_ptr<brgen::ast::Node>& node) {
        Module m;
        auto err = convert_node_definition(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        err = convert_node_encode(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        err = convert_node_decode(m, node);
        if (err) {
            return futils::helper::either::unexpected(err);
        }
        return m;
    }
}  // namespace rebgn
