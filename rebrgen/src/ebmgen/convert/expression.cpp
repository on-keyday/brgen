#include "../converter.hpp"
#include <core/ast/tool/ident.h>
#include "core/ast/node/ast_enum.h"
#include "core/ast/node/base.h"
#include "core/ast/node/literal.h"
#include "core/ast/node/statement.h"
#include "core/ast/node/type.h"
#include "ebm/extended_binary_module.hpp"
#include "ebmgen/common.hpp"
#include "ebmgen/converter.hpp"
#include "helper.hpp"
#include <fnet/util/base64.h>
#include <core/ast/traverse.h>
#include <memory>
#include <type_traits>

namespace ebmgen {
    expected<ebm::BinaryOp> convert_assignment_binary_op(ast::BinaryOp op) {
        switch (op) {
            case ast::BinaryOp::add_assign:
                return ebm::BinaryOp::add;
            case ast::BinaryOp::sub_assign:
                return ebm::BinaryOp::sub;
            case ast::BinaryOp::mul_assign:
                return ebm::BinaryOp::mul;
            case ast::BinaryOp::div_assign:
                return ebm::BinaryOp::div;
            case ast::BinaryOp::mod_assign:
                return ebm::BinaryOp::mod;
            case ast::BinaryOp::bit_and_assign:
                return ebm::BinaryOp::bit_and;
            case ast::BinaryOp::bit_or_assign:
                return ebm::BinaryOp::bit_or;
            case ast::BinaryOp::bit_xor_assign:
                return ebm::BinaryOp::bit_xor;
            case ast::BinaryOp::left_logical_shift_assign:
                return ebm::BinaryOp::left_shift;
            case ast::BinaryOp::right_logical_shift_assign:
                return ebm::BinaryOp::right_shift;
            default:
                return unexpect_error("Unsupported binary operator: {}", to_string(op));
        }
    }

    expected<ebm::BinaryOp> convert_binary_op(ast::BinaryOp op) {
        switch (op) {
            case ast::BinaryOp::add:
                return ebm::BinaryOp::add;
            case ast::BinaryOp::sub:
                return ebm::BinaryOp::sub;
            case ast::BinaryOp::mul:
                return ebm::BinaryOp::mul;
            case ast::BinaryOp::div:
                return ebm::BinaryOp::div;
            case ast::BinaryOp::mod:
                return ebm::BinaryOp::mod;
            case ast::BinaryOp::equal:
                return ebm::BinaryOp::equal;
            case ast::BinaryOp::not_equal:
                return ebm::BinaryOp::not_equal;
            case ast::BinaryOp::less:
                return ebm::BinaryOp::less;
            case ast::BinaryOp::less_or_eq:
                return ebm::BinaryOp::less_or_eq;
            case ast::BinaryOp::grater:  // typo but keep for backward compatibility
                return ebm::BinaryOp::greater;
            case ast::BinaryOp::grater_or_eq:
                return ebm::BinaryOp::greater_or_eq;
            case ast::BinaryOp::logical_and:
                return ebm::BinaryOp::logical_and;
            case ast::BinaryOp::logical_or:
                return ebm::BinaryOp::logical_or;
            case ast::BinaryOp::left_logical_shift:
                return ebm::BinaryOp::left_shift;
            case ast::BinaryOp::right_logical_shift:
                return ebm::BinaryOp::right_shift;
            case ast::BinaryOp::bit_and:
                return ebm::BinaryOp::bit_and;
            case ast::BinaryOp::bit_or:
                return ebm::BinaryOp::bit_or;
            case ast::BinaryOp::bit_xor:
                return ebm::BinaryOp::bit_xor;
            default:
                return convert_assignment_binary_op(op);
        }
    }

    expected<ebm::BinaryOpKind> decide_binary_op_kind(ebm::BinaryOp op) {
        switch (op) {
            case ebm::BinaryOp::add:
            case ebm::BinaryOp::sub:
            case ebm::BinaryOp::mul:
            case ebm::BinaryOp::div:
            case ebm::BinaryOp::mod:
                return ebm::BinaryOpKind::ARITHMETIC;
            case ebm::BinaryOp::equal:
            case ebm::BinaryOp::not_equal:
            case ebm::BinaryOp::less:
            case ebm::BinaryOp::less_or_eq:
            case ebm::BinaryOp::greater:
            case ebm::BinaryOp::greater_or_eq:
                return ebm::BinaryOpKind::COMPARISON;
            case ebm::BinaryOp::logical_and:
            case ebm::BinaryOp::logical_or:
                return ebm::BinaryOpKind::LOGICAL;
            case ebm::BinaryOp::left_shift:
            case ebm::BinaryOp::right_shift:
            case ebm::BinaryOp::bit_and:
            case ebm::BinaryOp::bit_or:
            case ebm::BinaryOp::bit_xor:
                return ebm::BinaryOpKind::BITWISE;
            default:
                return unexpect_error("Unsupported binary operator");
        }
    }

    expected<ebm::UnaryOp> convert_unary_op(ast::UnaryOp op, const std::shared_ptr<ast::Type>& type) {
        switch (op) {
            case ast::UnaryOp::not_:
                if (ast::as<ast::BoolType>(type)) {
                    return ebm::UnaryOp::logical_not;
                }
                return ebm::UnaryOp::bit_not;
            case ast::UnaryOp::minus_sign:
                return ebm::UnaryOp::minus_sign;
            default:
                return unexpect_error("Unsupported unary operator");
        }
    }

    expected<ebm::CastType> TypeConverter::get_cast_type(ebm::TypeRef dest_ref, ebm::TypeRef src_ref) {
        auto dest = ctx.repository().get_type(dest_ref);
        auto src = ctx.repository().get_type(src_ref);

        if (!dest || !src) {
            return unexpect_error("Invalid type reference for cast");
        }

        if (dest->body.kind == ebm::TypeKind::INT || dest->body.kind == ebm::TypeKind::UINT) {
            if (src->body.kind == ebm::TypeKind::ENUM) {
                return ebm::CastType::ENUM_TO_INT;
            }
            if (src->body.kind == ebm::TypeKind::FLOAT) {
                return ebm::CastType::FLOAT_TO_INT_BIT;
            }
            if (src->body.kind == ebm::TypeKind::BOOL) {
                return ebm::CastType::BOOL_TO_INT;
            }
            if (src->body.kind == ebm::TypeKind::USIZE) {
                return ebm::CastType::USIZE_TO_INT;
            }
            // Handle int/uint size and signedness conversions
            if ((src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) && dest->body.size() && src->body.size()) {
                auto dest_size = dest->body.size()->value();
                auto src_size = src->body.size()->value();
                if (dest_size < src_size) {
                    return ebm::CastType::LARGE_INT_TO_SMALL_INT;
                }
                if (dest_size > src_size) {
                    return ebm::CastType::SMALL_INT_TO_LARGE_INT;
                }
                // Check signedness conversion
                if (dest->body.kind == ebm::TypeKind::UINT && src->body.kind == ebm::TypeKind::INT) {
                    return ebm::CastType::SIGNED_TO_UNSIGNED;
                }
                if (dest->body.kind == ebm::TypeKind::INT && src->body.kind == ebm::TypeKind::UINT) {
                    return ebm::CastType::UNSIGNED_TO_SIGNED;
                }
            }
        }
        else if (dest->body.kind == ebm::TypeKind::ENUM) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_ENUM;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::FLOAT) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_FLOAT_BIT;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::BOOL) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_BOOL;
            }
        }
        else if (dest->body.kind == ebm::TypeKind::USIZE) {
            if (src->body.kind == ebm::TypeKind::INT || src->body.kind == ebm::TypeKind::UINT) {
                return ebm::CastType::INT_TO_USIZE;
            }
        }
        // TODO: Add more complex type conversions (ARRAY, VECTOR, STRUCT, RECURSIVE_STRUCT)

        return ebm::CastType::OTHER;
    }

    template <class T>
    concept has_convert_expr_impl = requires(T t) {
        { convert_expr_impl(std::declval<std::shared_ptr<T>>(), std::declval<ebm::ExpressionBody&>()) } -> std::same_as<expected<void>>;
    };

    expected<ebm::ExpressionRef> ExpressionConverter::convert_expr(const std::shared_ptr<ast::Expr>& node) {
        if (auto identity = ast::as<ast::Identity>(node)) {
            return convert_expr(identity->expr);
        }
        ebm::ExpressionBody body;
        if (!node->expr_type) {
            return unexpect_error("Expression has no type");
        }
        EBMA_CONVERT_TYPE(type_ref, node->expr_type);
        body.type = type_ref;

        expected<void> result = unexpect_error("Unsupported expression type: {}", node_type_to_string(node->node_type));

        brgen::ast::visit(ast::cast_to<ast::Node>(node), [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(n)>, std::shared_ptr>::template param_at<0>;
            if constexpr (std::is_base_of_v<ast::Expr, T>) {
                result = convert_expr_impl(n, body);
            }
        });

        if (!result) {
            return unexpect_error(std::move(result.error()));
        }

        EBMA_ADD_EXPR(ref, std::move(body));
        ctx.repository().add_debug_loc(node->loc, ref);
        return ref;
    }

    expected<ebm::ExpressionRef> get_alignment_requirement(ConverterContext& ctx, std::uint64_t alignment_bytes, ebm::StreamType type) {
        if (alignment_bytes == 0) {
            return unexpect_error("0 is not valid alignment");
        }
        if (alignment_bytes == 1) {
            EBMU_INT_LITERAL(lit, 1);
            return lit;
        }
        ebm::ExpressionBody body;
        EBMU_COUNTER_TYPE(counter_type);
        body.type = counter_type;
        body.kind = ebm::ExpressionKind::GET_STREAM_OFFSET;
        body.stream_type(type);
        body.unit(ebm::SizeUnit::BYTE_FIXED);
        EBMA_ADD_EXPR(stream_offset, std::move(body));

        EBMU_INT_LITERAL(alignment, alignment_bytes);

        if (std::popcount(alignment_bytes) == 1) {
            EBMU_INT_LITERAL(alignment_bitmask, alignment_bytes - 1);
            // size(=offset) & (alignment - 1)
            EBM_BINARY_OP(mod, ebm::BinaryOp::bit_and, counter_type, stream_offset, alignment_bitmask);
            // alignment - (size & (alignment-1))
            EBM_BINARY_OP(cmp, ebm::BinaryOp::sub, counter_type, alignment, mod);
            // (alignment - (size & (alignment-1))) & (alignment - 1)
            EBM_BINARY_OP(req_size, ebm::BinaryOp::bit_and, counter_type, cmp, alignment_bitmask);
            return req_size;
        }
        else {
            // size(=offset) % alignment
            EBM_BINARY_OP(mod, ebm::BinaryOp::mod, counter_type, stream_offset, alignment);

            // alignment - (size % alignment)
            EBM_BINARY_OP(cmp, ebm::BinaryOp::sub, counter_type, alignment, mod);

            // (alignment - (size % alignment)) % alignment
            EBM_BINARY_OP(req_size, ebm::BinaryOp::mod, counter_type, cmp, alignment);
            return req_size;
        }
    }
    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::IntLiteral>& node, ebm::ExpressionBody& body) {
        auto value = node->parse_as<std::uint64_t>();
        if (!value) {
            return unexpect_error("cannot parse int literal");
        }
        body = get_int_literal_body(body.type, *value);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::BoolLiteral>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::LITERAL_BOOL;
        body.bool_value(node->value);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::StrLiteral>& node, ebm::ExpressionBody& body) {
        MAYBE(candidate, decode_base64(ast::cast_to<ast::StrLiteral>(node)));
        body.kind = ebm::ExpressionKind::LITERAL_STRING;
        EBMA_ADD_STRING(str_ref, candidate);
        body.string_value(str_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::TypeLiteral>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::LITERAL_TYPE;
        EBMA_CONVERT_TYPE(type_ref_inner, node->type_literal)
        body.type_ref(type_ref_inner);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Ident>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::IDENTIFIER;
        auto base = ast::tool::lookup_base(ast::cast_to<ast::Ident>(node));
        if (!base) {
            return unexpect_error("Identifier {} not found", node->ident);
        }
        ebm::StatementKind kind = ebm::StatementKind::BLOCK;  // temporary init
        ebm::StatementRef id;
        bool is_state_variable = false;
        auto base_locked = base->first->base.lock();
        bool has_parent = false;
        if (auto m = ast::as<ast::Member>(base_locked); m && m->belong.lock()) {
            if (m->belong.lock()->node_type != ast::NodeType::function) {
                has_parent = true;
            }
        }
        if (auto f = ast::as<ast::Field>(base_locked); f && f->is_state_variable) {
            is_state_variable = true;
            auto current_func = ctx.state().get_format_encode_decode(ctx.state().get_current_node());
            if (!current_func) {
                return unexpect_error("State variable {} can be used only inside encode/decode function", node->ident);
            }
            for (auto& param : current_func->state_variables) {
                if (param.ast_field == base_locked) {
                    if (ctx.state().get_current_generate_type() == GenerateType::Encode) {
                        id = param.enc_var_def;
                    }
                    else if (ctx.state().get_current_generate_type() == GenerateType::Decode) {
                        id = param.dec_var_def;
                    }
                    else if (ctx.state().get_current_generate_type() == GenerateType::PropertyGetter) {
                        id = param.prop_get_var_def;
                    }
                    else if (ctx.state().get_current_generate_type() == GenerateType::PropertySetter) {
                        id = param.prop_set_var_def;
                    }
                    else {
                        return unexpect_error("Invalid generate type for state variable {}", node->ident);
                    }
                    body.id(to_weak(id));
                    return {};
                }
            }
            return unexpect_error("State variable {} not found in current encode/decode function", node->ident);
        }
        if (ast::as<ast::Binary>(base_locked)) {
            // preserve generate type
            EBMA_CONVERT_STATEMENT(id_ref, base_locked);
            body.id(to_weak(id_ref));
            id = id_ref;
        }
        else {  // switch to normal generate type
            const auto normal = ctx.state().set_current_generate_type(GenerateType::Normal);
            EBMA_CONVERT_STATEMENT(id_ref, base_locked);
            body.id(to_weak(id_ref));
            id = id_ref;
        }
        if (auto self = ctx.state().get_self_ref(); self && has_parent && !is_state_variable) {
            if (!ctx.state().is_on_available_check()) {
                auto got = ctx.state().get_self_ref_for_id(id);
                if (got) {
                    MAYBE(expr, ctx.repository().get_expression(*got));
                    auto base = expr.body.base();
                    if (base) {
                        self = *base;
                    }
                }
            }
            EBMA_ADD_EXPR(ident_ref, std::move(body));
            body.kind = ebm::ExpressionKind::MEMBER_ACCESS;
            body.base(*self);
            body.member(ident_ref);
        }
        return {};
    }

    expected<std::pair<ebm::ExpressionRef, ebm::ExpressionRef>> insert_binary_op_cast(ConverterContext& ctx, ebm::BinaryOp bop, ebm::TypeRef dst_type, ebm::ExpressionRef left, ebm::ExpressionRef right) {
        MAYBE(kind, decide_binary_op_kind(bop));
        MAYBE(dst_type_obj, ctx.repository().get_type(dst_type));
        if (kind == ebm::BinaryOpKind::ARITHMETIC || kind == ebm::BinaryOpKind::BITWISE || kind == ebm::BinaryOpKind::COMPARISON) {
            MAYBE(left_expr, ctx.repository().get_expression(left));
            MAYBE(right_expr, ctx.repository().get_expression(right));
            auto left_type = left_expr.body.type;
            auto right_type = right_expr.body.type;
            if (dst_type_obj.body.kind == ebm::TypeKind::VOID) {  // on case of assignment
                dst_type = left_type;
            }
            if (kind == ebm::BinaryOpKind::COMPARISON) {
                MAYBE(common_type, get_common_type(ctx, left_type, right_type));
                if (common_type) {  //  TODO(on-keyday): currently, this is best effort
                    EBM_CAST(left_casted, *common_type, left_type, left);
                    EBM_CAST(right_casted, *common_type, right_type, right);
                    left = left_casted;
                    right = right_casted;
                }
            }
            else {
                EBM_CAST(left_casted, dst_type, left_type, left);
                EBM_CAST(right_casted, dst_type, right_type, right);
                left = left_casted;
                right = right_casted;
            }
        }
        return std::make_pair(left, right);
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Binary>& node, ebm::ExpressionBody& body) {
        if (node->op == ast::BinaryOp::define_assign || node->op == ast::BinaryOp::const_assign) {
            return unexpect_error("define_assign/const_assign should be handled as a statement, not an expression");
        }
        EBMA_CONVERT_EXPRESSION(left_ref, node->left);
        EBMA_CONVERT_EXPRESSION(right_ref, node->right);
        MAYBE(bop, convert_binary_op(node->op));
        MAYBE(casted, insert_binary_op_cast(ctx, bop, body.type, left_ref, right_ref));
        left_ref = casted.first;
        right_ref = casted.second;
        body = make_binary_op(bop, body.type, left_ref, right_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Unary>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::UNARY_OP;
        EBMA_CONVERT_EXPRESSION(operand_ref, node->expr);
        body.operand(operand_ref);
        MAYBE(uop, convert_unary_op(node->op, node->expr_type));
        body.uop(uop);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Index>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::INDEX_ACCESS;
        EBMA_CONVERT_EXPRESSION(base_ref, node->expr);
        EBMA_CONVERT_EXPRESSION(index_ref, node->index);
        body.base(base_ref);
        body.index(index_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::MemberAccess>& node, ebm::ExpressionBody& body) {
        if (auto typ_literal = ast::as<ast::TypeLiteral>(node->target); typ_literal) {
            if (auto ident_ = ast::as<ast::IdentType>(typ_literal->type_literal)) {
                auto base = ast::as<ast::EnumType>(ident_->base.lock());
                if (base) {
                    auto _scope = ctx.state().set_current_generate_type(GenerateType::Normal);
                    EBMA_CONVERT_STATEMENT(enum_decl_stmt, base->base.lock());
                    // Enum member access
                    body.kind = ebm::ExpressionKind::ENUM_MEMBER;
                    body.enum_decl(enum_decl_stmt);
                    auto temporary = ctx.state().set_self_ref(std::nullopt);
                    EBMA_CONVERT_EXPRESSION(member_ref, node->member);
                    body.member(member_ref);
                    return {};
                }
                // currently fallback to normal member access
            }
        }
        body.kind = ebm::ExpressionKind::MEMBER_ACCESS;
        EBMA_CONVERT_EXPRESSION(base_ref, node->target);
        if (auto member = ast::as<ast::Ident>(node->member); member && member->usage == ast::IdentUsage::reference_builtin_fn) {
            if (auto typ = ast::as<ast::ArrayType>(node->target->expr_type)) {
                if (member->ident == "length") {
                    // replace type with USIZE
                    MAYBE(type, get_single_type(ebm::TypeKind::USIZE, ctx));
                    body.type = type;
                    body.kind = ebm::ExpressionKind::ARRAY_SIZE;
                    body.array_expr(base_ref);
                    return {};
                }
            }
            else if (auto ident_ = ast::as<ast::IdentType>(node->target->expr_type)) {
                if (auto enum_ = ast::as<ast::EnumType>(ident_->base.lock())) {
                    if (member->ident == "is_defined") {
                        MAYBE(target_expr, ctx.repository().get_expression(base_ref));
                        auto enum_type = target_expr.body.type;
                        body.kind = ebm::ExpressionKind::ENUM_IS_DEFINED;
                        body.target_expr(base_ref);
                        auto scope = ctx.state().set_current_generate_type(GenerateType::Normal);
                        EBMA_CONVERT_STATEMENT(base_stmt, enum_->base.lock());
                        scope.execute();
                        MAYBE(base_enum, ctx.repository().get_statement(base_stmt));
                        MAYBE(enum_decl_ref, base_enum.body.enum_decl());
                        auto enum_decl = enum_decl_ref;
                        ebm::ExpressionRef cond;
                        EBMU_BOOL_TYPE(bool_type);
                        for (auto& m : enum_decl.members.container) {
                            EBM_IDENTIFIER(member, m, enum_type);
                            EBM_ENUM_MEMBER(enum_member, enum_type, base_stmt, member);
                            MAYBE(eq, convert_equal(base_ref, enum_member));
                            if (is_nil(cond)) {
                                cond = eq;
                            }
                            else {
                                EBM_BINARY_OP(or_, ebm::BinaryOp::logical_or, bool_type, cond, eq);
                                cond = or_;
                            }
                        }
                        if (is_nil(cond)) {
                            return unexpect_error("Enum has no members");
                        }
                        body.lowered_expr(ebm::LoweredExpressionRef{cond});
                        return {};
                    }
                }
            }
            return unexpect_error("Unsupported builtin member function '{}' for type '{}'", member->ident, node_type_to_string(node->target->expr_type->node_type));
        }
        auto temporary = ctx.state().set_self_ref(std::nullopt);
        EBMA_CONVERT_EXPRESSION(member_ref, node->member);
        body.base(base_ref);
        body.member(member_ref);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Cast>& node, ebm::ExpressionBody& body) {
        if (node->arguments.size() == 0) {
            body.kind = ebm::ExpressionKind::DEFAULT_VALUE;
            return {};
        }
        body.kind = ebm::ExpressionKind::TYPE_CAST;
        EBMA_CONVERT_EXPRESSION(source_expr_ref, node->arguments[0]);
        EBMA_CONVERT_TYPE(source_expr_type_ref, node->arguments[0]->expr_type);
        ebm::TypeCastDesc type_cast;
        type_cast.source_expr = source_expr_ref;
        type_cast.from_type = source_expr_type_ref;
        MAYBE(cast_kind, ctx.get_type_converter().get_cast_type(body.type, source_expr_type_ref));
        type_cast.cast_kind = cast_kind;
        if (cast_kind == ebm::CastType::FUNCTION_CAST) {
            // find cast function
            type_cast.cast_function({});  // at post transform phase, fill this field
        }
        body.type_cast_desc(type_cast);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Range>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::RANGE;
        ebm::ExpressionRef start, end;
        if (node->start) {
            EBMA_CONVERT_EXPRESSION(start_ref, node->start);
            start = start_ref;
        }
        if (node->end) {
            EBMA_CONVERT_EXPRESSION(end_ref, node->end);
            end = end_ref;
        }
        body.start(start);
        body.end(end);
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::IOOperation>& node, ebm::ExpressionBody& body) {
        switch (node->method) {
            case ast::IOMethod::input_get:
            case ast::IOMethod::input_peek: {
                body.kind = ebm::ExpressionKind::READ_DATA;
                EBMA_CONVERT_TYPE(type_ref, node->expr_type);
                EBM_DEFAULT_VALUE(default_, type_ref);
                EBM_DEFINE_ANONYMOUS_VARIABLE(var, type_ref, default_);
                body.target_stmt(var_def);
                MAYBE(decode_info, ctx.get_decoder_converter().decode_field_type(node->expr_type, var, nullptr, ctx.state().get_current_function_id()));
                if (node->method == ast::IOMethod::input_peek) {
                    decode_info.read_data()->attribute.is_peek(true);
                }
                EBMA_ADD_STATEMENT(io_statement_ref, std::move(decode_info));
                body.io_statement(io_statement_ref);
                break;
            }
            case ast::IOMethod::output_put: {
                body.kind = ebm::ExpressionKind::WRITE_DATA;
                EBMA_CONVERT_EXPRESSION(output, node->arguments[0]);
                body.target_expr(output);
                MAYBE(encode_info, ctx.get_encoder_converter().encode_field_type(node->arguments[0]->expr_type, output, nullptr, ctx.state().get_current_function_id()));
                EBMA_ADD_STATEMENT(io_statement_ref, std::move(encode_info));
                body.io_statement(io_statement_ref);
                break;
            }
            case ast::IOMethod::input_offset:
            case ast::IOMethod::input_bit_offset: {
                body.kind = ebm::ExpressionKind::GET_STREAM_OFFSET;
                MAYBE(current_encdec, ctx.state().get_format_encode_decode(ctx.state().get_current_node()));
                if (ctx.state().get_current_generate_type() != GenerateType::Encode) {
                    body.io_ref(current_encdec.encoder_input_def);
                    body.stream_type(ebm::StreamType::OUTPUT);
                }
                else if (ctx.state().get_current_generate_type() != GenerateType::Decode) {
                    body.io_ref(current_encdec.decoder_input_def);
                    body.stream_type(ebm::StreamType::INPUT);
                }
                else {
                    return unexpect_error("input.offset/input.bit_offset can be used only inside encode/decode function");
                }
                body.unit(node->method == ast::IOMethod::input_bit_offset ? ebm::SizeUnit::BIT_FIXED : ebm::SizeUnit::BYTE_FIXED);
                break;
            }
            case ast::IOMethod::input_remain: {
                body.kind = ebm::ExpressionKind::GET_REMAINING_BYTES;
                body.stream_type(ebm::StreamType::INPUT);
                MAYBE(current_encdec, ctx.state().get_format_encode_decode(ctx.state().get_current_node()));
                if (ctx.state().get_current_generate_type() != GenerateType::Encode) {
                    body.io_ref(current_encdec.encoder_input_def);
                }
                else if (ctx.state().get_current_generate_type() != GenerateType::Decode) {
                    body.io_ref(current_encdec.decoder_input_def);
                }
                else {
                    return unexpect_error("input.remain can be used only inside encode/decode function");
                }
                break;
            }
            case ast::IOMethod::input_subrange: {
                return unexpect_error("not implemented yet: {}", ast::to_string(node->method));
            }
            default: {
                return unexpect_error("Unhandled IOMethod: {}", ast::to_string(node->method));
            }
        }
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Paren>& node, ebm::ExpressionBody& body) {
        expected<void> result = unexpect_error("Expected expression inside parentheses, got {}", node_type_to_string(node->expr->node_type));
        brgen::ast::visit(ast::cast_to<ast::Node>(node->expr), [&](auto&& n) {
            using T = typename futils::helper::template_instance_of_t<std::decay_t<decltype(n)>, std::shared_ptr>::template param_at<0>;
            if constexpr (std::is_base_of_v<ast::Expr, T>) {
                result = convert_expr_impl(n, body);
            }
            else {
                result = unexpect_error("Expected expression inside parentheses, got {}", node_type_to_string(node->expr->node_type));
            }
        });
        return result;
    }

    expected<void> convert_conditional_statement(ConverterContext& ctx, const std::shared_ptr<ast::Node>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::CONDITIONAL_STATEMENT;
        EBM_DEFAULT_VALUE(default_, body.type);
        EBM_DEFINE_ANONYMOUS_VARIABLE(var, body.type, default_);
        body.target_stmt(var_def);

        const auto _defer = ctx.state().set_current_yield_statement(var_def);

        EBMA_CONVERT_STATEMENT(conditional_stmt_ref, node);
        body.conditional_stmt(conditional_stmt_ref);

        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::If>& node, ebm::ExpressionBody& body) {
        return convert_conditional_statement(ctx, node, body);
    }
    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Match>& node, ebm::ExpressionBody& body) {
        return convert_conditional_statement(ctx, node, body);
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::CharLiteral>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::LITERAL_CHAR;
        MAYBE(value, varint(static_cast<std::uint32_t>(node->code)));
        body.char_value(value);
        return {};
    }

    expected<ebm::ExpressionBody> make_conditional(ConverterContext& ctx, ebm::TypeRef type, ebm::ExpressionRef cond, ebm::ExpressionRef then, ebm::ExpressionRef els) {
        ebm::ExpressionBody body;
        body.type = type;
        body.kind = ebm::ExpressionKind::CONDITIONAL;
        // reusing cast insertion logic TODO(on-keyday): change name of function?
        MAYBE(casted, insert_binary_op_cast(ctx, ebm::BinaryOp::add, type, then, els));
        then = casted.first;
        els = casted.second;
        body.condition(cond);
        body.then(then);
        body.else_(els);

        // lowered
        EBM_DEFAULT_VALUE(default_, body.type);
        EBM_DEFINE_ANONYMOUS_VARIABLE(yielded_value, body.type, default_);
        EBM_IDENTIFIER(temp_var, yielded_value_def, body.type);
        ebm::StatementBody lowered_body;
        lowered_body.kind = ebm::StatementKind::YIELD;
        lowered_body.target(temp_var);
        lowered_body.value(then);
        EBMA_ADD_STATEMENT(yield_then, std::move(lowered_body));
        lowered_body.kind = ebm::StatementKind::YIELD;
        lowered_body.target(temp_var);
        lowered_body.value(els);
        EBMA_ADD_STATEMENT(yield_els, std::move(lowered_body));
        EBM_IF_STATEMENT(cond_stmt, cond, yield_then, yield_els);
        ebm::ExpressionBody lowered_expr;
        lowered_expr.type = body.type;
        lowered_expr.kind = ebm::ExpressionKind::CONDITIONAL_STATEMENT;
        lowered_expr.target_stmt(yielded_value_def);
        lowered_expr.conditional_stmt(cond_stmt);
        EBMA_ADD_EXPR(low_expr, std::move(lowered_expr));
        body.lowered_expr(ebm::LoweredExpressionRef{low_expr});
        return body;
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Cond>& node, ebm::ExpressionBody& body) {
        EBMA_CONVERT_EXPRESSION(cond, node->cond);
        EBMA_CONVERT_EXPRESSION(then, node->then);
        EBMA_CONVERT_EXPRESSION(els, node->els);
        MAYBE(body_, make_conditional(ctx, body.type, cond, then, els));
        body = std::move(body_);

        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::OrCond>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::OR_COND;
        ebm::Expressions conds;
        for (auto& cond : node->conds) {
            EBMA_CONVERT_EXPRESSION(c, cond);
            append(conds, c);
        }
        body.or_cond(std::move(conds));
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Expr>& node, ebm::ExpressionBody& body) {
        return unexpect_error("expr not implemented yet: {}", node_type_to_string(node->node_type));
    }

    expected<ebm::ExpressionRef> convert_equal_impl(ConverterContext& ctx, ebm::ExpressionRef a, ebm::ExpressionRef b, ebm::Expression& A, ebm::Expression& B) {
        EBMU_BOOL_TYPE(bool_type);
        if (A.body.kind != B.body.kind) {  // to prevent expression like 0..1 == 1..0
            if (auto start = A.body.start()) {
                auto end = A.body.end();
                if (is_nil(*start) && is_nil(*end)) {
                    ebm::ExpressionBody body;
                    body.kind = ebm::ExpressionKind::LITERAL_BOOL;
                    body.bool_value(1);
                    EBMA_ADD_EXPR(true_, std::move(body));
                    return true_;
                }
                ebm::ExpressionRef ref;
                if (!is_nil(*start)) {
                    EBM_BINARY_OP(greater, ebm::BinaryOp::less_or_eq, bool_type, *start, b);
                    ref = greater;
                }
                if (!is_nil(*end)) {
                    EBM_BINARY_OP(greater, ebm::BinaryOp::less_or_eq, bool_type, b, *end);
                    if (!is_nil(ref)) {
                        EBM_BINARY_OP(and_, ebm::BinaryOp::logical_and, bool_type, ref, greater);
                        ref = and_;
                    }
                    else {
                        ref = greater;
                    }
                }
                return ref;
            }
            else if (auto start = B.body.start()) {
                return convert_equal_impl(ctx, b, a, B, A);
            }
            else if (auto or_cond = A.body.or_cond()) {
                ebm::ExpressionRef cond;
                for (auto& or_ : or_cond->container) {
                    MAYBE(eq, ctx.get_expression_converter().convert_equal(or_, b));
                    if (is_nil(cond)) {
                        cond = eq;
                    }
                    else {
                        EBM_BINARY_OP(ored, ebm::BinaryOp::logical_or, bool_type, cond, eq);
                        cond = ored;
                    }
                }
                return cond;
            }
            else if (auto end = B.body.or_cond()) {
                return convert_equal_impl(ctx, b, a, B, A);
            }
        }
        EBM_BINARY_OP(eq, ebm::BinaryOp::equal, bool_type, a, b);
        return eq;
    }

    expected<ebm::ExpressionRef> ExpressionConverter::convert_equal(ebm::ExpressionRef a, ebm::ExpressionRef b) {
        MAYBE(A, ctx.repository().get_expression(a));
        MAYBE(B, ctx.repository().get_expression(b));
        return convert_equal_impl(ctx, a, b, A, B);
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Available>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::AVAILABLE;
        EBMA_CONVERT_EXPRESSION(target, node->target);
        body.target_expr(target);
        ebm::ExpressionRef additional_base;
        if (auto member = ctx.repository().get_expression(target)) {
            if (member->body.kind == ebm::ExpressionKind::MEMBER_ACCESS) {
                auto base_ref = *member->body.base();
                if (auto base = ctx.repository().get_expression(base_ref)) {
                    if (base->body.kind != ebm::ExpressionKind::SELF) {
                        additional_base = base_ref;
                    }
                }
            }
        }
        EBMU_BOOL_TYPE(bool_type);
        EBM_BOOL_LITERAL(true_, true);
        EBM_BOOL_LITERAL(false_, false);
        if (auto typ = ast::as<ast::UnionType>(node->target->expr_type)) {
            auto make_lowered_expr = [&]() -> expected<void> {
                ebm::ExpressionRef base_cond;
                if (auto b = typ->cond.lock()) {
                    EBMA_CONVERT_EXPRESSION(cond, b);
                    base_cond = cond;
                }
                struct LCond {
                    ebm::ExpressionRef cond;
                    ebm::ExpressionRef then;
                };

                std::vector<LCond> conds;
                for (auto& c : typ->candidates) {
                    auto expr = c->cond.lock();
                    EBMA_CONVERT_EXPRESSION(cond, expr);
                    if (!is_nil(base_cond)) {
                        MAYBE(eq, convert_equal(base_cond, cond));
                        cond = eq;
                    }
                    conds.push_back(LCond{cond, c->field.lock() != nullptr ? true_ : false_});
                }
                ebm::ExpressionRef else_ = false_;
                for (auto& cond : conds | std::views::reverse) {
                    MAYBE(conditional, make_conditional(ctx, bool_type, cond.cond, cond.then, else_));
                    EBMA_ADD_EXPR(ref, std::move(conditional));
                    else_ = ref;
                }
                body.lowered_expr(ebm::LoweredExpressionRef{else_});
                return {};
            };
            if (!is_nil(additional_base)) {
                auto self_ = ctx.state().set_self_ref(additional_base);
                auto available_ = ctx.state().set_on_available_check(true);
                MAYBE_VOID(ok, make_lowered_expr());
            }
            else {
                MAYBE_VOID(ok, make_lowered_expr());
            }
        }
        else {
            EBM_BOOL_LITERAL(result, true);
            body.lowered_expr(ebm::LoweredExpressionRef{result});
        }
        return {};
    }

    expected<void> ExpressionConverter::convert_expr_impl(const std::shared_ptr<ast::Call>& node, ebm::ExpressionBody& body) {
        body.kind = ebm::ExpressionKind::CALL;
        ebm::CallDesc call;
        EBMA_CONVERT_EXPRESSION(callee, node->callee);
        call.callee = callee;
        for (const auto& arg : node->arguments) {
            EBMA_CONVERT_EXPRESSION(arg_ref, arg);
            MAYBE(expr, ctx.repository().get_expression(arg_ref));
            EBM_AS_ARG(as_arg, expr.body.type, arg_ref);
            append(call.arguments, as_arg);
        }
        body.call_desc(std::move(call));
        return {};
    }

}  // namespace ebmgen
