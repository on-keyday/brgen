/*license*/
#pragma once

#include "ebm/extended_binary_module.hpp"
#include "../common.hpp"
namespace ebmgen {

#define EBM_AST_CONSTRUCTOR(ref_name, RefType, add_func, make_func, ...) \
    ebm::RefType ref_name;                                               \
    {                                                                    \
        MAYBE(new_ref____, add_func(make_func(__VA_ARGS__)));            \
        ref_name = new_ref____;                                          \
    }

#define EBM_AST_STATEMENT(ref_name, make_func, ...) \
    EBM_AST_CONSTRUCTOR(ref_name, StatementRef, ctx.repository().add_statement, make_func, __VA_ARGS__)

#define EBM_AST_EXPRESSION(ref_name, make_func, ...) \
    EBM_AST_CONSTRUCTOR(ref_name, ExpressionRef, ctx.repository().add_expr, make_func, __VA_ARGS__)

#define EBM_AST_EXPRESSION_REF(ref_name) ebm::ExpressionRef ref_name;
#define EBM_AST_STATEMENT_REF(ref_name) ebm::StatementRef ref_name;
#define EBM_AST_VARIABLE_REF(ref_name) EBM_AST_EXPRESSION_REF(ref_name) EBM_AST_STATEMENT_REF(ref_name##_def)
#define EBM_AST_VARIABLE_REF_SET(ref_name, expr_name, def_name) \
    ref_name = expr_name;                                       \
    ref_name##_def = def_name;

    // currently new ConverterContext

    // ebm ast related

#define EBMA_CONVERT_EXPRESSION(ref_name, expr) \
    MAYBE(ref_name, ctx.convert_expr(expr));

#define EBMA_CONVERT_TYPE(ref_name, ...) \
    MAYBE(ref_name, ctx.convert_type(__VA_ARGS__));

#define EBMA_CONVERT_STATEMENT(ref_name, ...) \
    MAYBE(ref_name, ctx.convert_statement(__VA_ARGS__));

#define EBMA_ADD_IDENTIFIER(ref_name, ident) \
    MAYBE(ref_name, ctx.repository().add_identifier(ident));

#define EBMA_ADD_STRING(ref_name, candidate) \
    MAYBE(ref_name, ctx.repository().add_string(candidate));

#define EBMA_ADD_TYPE(ref_name, ...) \
    MAYBE(ref_name, ctx.repository().add_type(__VA_ARGS__));

#define EBMA_ADD_STATEMENT(ref_name, ...) \
    MAYBE(ref_name, ctx.repository().add_statement(__VA_ARGS__));

#define EBMA_ADD_EXPR(ref_name, ...) \
    MAYBE(ref_name, ctx.repository().add_expr(__VA_ARGS__));

    // ebm utility

#define EBMU_BOOL_TYPE(ref_name) \
    MAYBE(ref_name, get_bool_type(ctx))

#define EBMU_VOID_TYPE(ref_name) \
    MAYBE(ref_name, get_void_type(ctx))

#define EBMU_COUNTER_TYPE(ref_name) \
    MAYBE(ref_name, get_counter_type(ctx))

#define EBMU_UINT_TYPE(ref_name, n) \
    MAYBE(ref_name, get_unsigned_n_int(ctx, n))

#define EBMU_U8(ref_name) \
    EBMU_UINT_TYPE(ref_name, 8)

#define EBMU_U8_N_ARRAY(ref_name, n, annot) \
    MAYBE(ref_name, get_u8_n_array(ctx, n, annot))

#define EBMU_INT_LITERAL(ref_name, value) \
    MAYBE(ref_name, get_int_literal(ctx, value))

    // main converter functions

    ebm::ExpressionBody make_default_value(ebm::TypeRef type);

#define EBM_DEFAULT_VALUE(ref_name, typ) \
    EBM_AST_EXPRESSION(ref_name, make_default_value, typ)

    ebm::StatementBody make_variable_decl(ebm::IdentifierRef name, ebm::TypeRef type, ebm::ExpressionRef initial_ref, ebm::VariableDeclKind decl_kind, bool is_reference);

    ebm::StatementBody make_parameter_decl(ebm::IdentifierRef name, ebm::TypeRef type, bool is_state_variable);

    ebm::ExpressionBody make_identifier_expr(ebm::StatementRef id, ebm::TypeRef type);

#define EBM_IDENTIFIER(ref_name, id, typ) \
    EBM_AST_EXPRESSION(ref_name, make_identifier_expr, id, typ)

#define EBM_DEFINE_VARIABLE(ref_name, id, typ, initial_ref, decl_kind, is_reference)                                    \
    EBM_AST_VARIABLE_REF(ref_name) {                                                                                    \
        MAYBE(new_id_, ctx.repository().new_statement_id());                                                            \
        EBMA_ADD_STATEMENT(new_var_ref_, new_id_, (make_variable_decl(id, typ, initial_ref, decl_kind, is_reference))); \
        EBM_IDENTIFIER(new_expr_ref_, new_var_ref_, typ);                                                               \
        EBM_AST_VARIABLE_REF_SET(ref_name, new_expr_ref_, new_var_ref_);                                                \
    }

#define EBM_DEFINE_ANONYMOUS_VARIABLE(ref_name, typ, initial_ref) \
    EBM_DEFINE_VARIABLE(ref_name, {}, typ, initial_ref, ebm::VariableDeclKind::MUTABLE, false)

#define EBM_DEFINE_PARAMETER(ref_name, id, typ, is_state_variable)                                     \
    EBM_AST_VARIABLE_REF(ref_name) {                                                                   \
        MAYBE(new_id_, ctx.repository().new_statement_id());                                           \
        EBMA_ADD_STATEMENT(new_var_ref__, new_id_, (make_parameter_decl(id, typ, is_state_variable))); \
        EBM_IDENTIFIER(new_expr_ref__, new_var_ref__, typ);                                            \
        EBM_AST_VARIABLE_REF_SET(ref_name, new_expr_ref__, new_var_ref__);                             \
    }

    ebm::ExpressionBody make_cast(ebm::TypeRef to_typ, ebm::TypeRef from_typ, ebm::ExpressionRef expr, ebm::CastType cast_kind);

#define EBM_CAST(ref_name, to_typ, from_typ, expr)                                                 \
    ebm::ExpressionRef ref_name;                                                                   \
    {                                                                                              \
        if (get_id(from_typ) != get_id(to_typ)) {                                                  \
            MAYBE(cast_kind________, ctx.get_type_converter().get_cast_type(to_typ, from_typ));    \
            EBMA_ADD_EXPR(cast_ref________, make_cast(to_typ, from_typ, expr, cast_kind________)); \
            ref_name = cast_ref________;                                                           \
        }                                                                                          \
        else {                                                                                     \
            ref_name = expr;                                                                       \
        }                                                                                          \
    }

    ebm::ExpressionBody make_binary_op(ebm::BinaryOp bop, ebm::TypeRef type, ebm::ExpressionRef left, ebm::ExpressionRef right);

#define EBM_BINARY_OP(ref_name, bop__, typ, left__, right__)                                       \
    ebm::ExpressionRef ref_name;                                                                   \
    {                                                                                              \
        MAYBE(casted, insert_binary_op_cast(ctx, bop__, typ, left__, right__));                    \
        EBM_AST_EXPRESSION(ref_name##__, make_binary_op, bop__, typ, casted.first, casted.second); \
        ref_name = ref_name##__;                                                                   \
    }

    ebm::ExpressionBody make_unary_op(ebm::UnaryOp uop, ebm::TypeRef type, ebm::ExpressionRef operand);

#define EBM_UNARY_OP(ref_name, uop__, typ, operand__) \
    EBM_AST_EXPRESSION(ref_name, make_unary_op, uop__, typ, operand__)

    ebm::StatementBody make_assignment(ebm::ExpressionRef target, ebm::ExpressionRef value, ebm::StatementRef previous_assignment);

#define EBM_ASSIGNMENT(ref_name, target__, value__) \
    EBM_AST_STATEMENT(ref_name, make_assignment, target__, value__, {})

    ebm::ExpressionBody make_index(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef index);

#define EBM_INDEX(ref_name, type, base__, index__) \
    EBM_AST_EXPRESSION(ref_name, make_index, type, base__, index__)

    ebm::StatementBody make_write_data(ebm::IOData io_data);

#define EBM_WRITE_DATA(ref_name, io_data) \
    EBM_AST_STATEMENT(ref_name, make_write_data, io_data)

    ebm::StatementBody make_read_data(ebm::IOData io_data);

#define EBM_RESERVE_DATA(ref_name, reserve_data) \
    EBM_AST_STATEMENT(ref_name, make_reserve_data, reserve_data)

    ebm::StatementBody make_reserve_data(ebm::ReserveData reserve_data);

#define EBM_READ_DATA(ref_name, io_data) \
    EBM_AST_STATEMENT(ref_name, make_read_data, io_data)

// internally, represent as i = i + 1 because no INCREMENT operator in EBM
#define EBM_INCREMENT(ref_name, target__, target_type_)                              \
    ebm::StatementRef ref_name;                                                      \
    {                                                                                \
        EBMU_INT_LITERAL(one, 1);                                                    \
        EBM_BINARY_OP(incremented, ebm::BinaryOp::add, target_type_, target__, one); \
        EBM_ASSIGNMENT(increment_ref____, target__, incremented);                    \
        ref_name = increment_ref____;                                                \
    }

    ebm::StatementBody make_block(ebm::Block&& block);

#define EBM_BLOCK(ref_name, block_body__) \
    EBM_AST_STATEMENT(ref_name, make_block, block_body__)

    ebm::StatementBody make_loop(ebm::LoopStatement&& loop_stmt);

#define EBM_LOOP(ref_name, loop_stmt__) \
    EBM_AST_STATEMENT(ref_name, make_loop, std::move(loop_stmt__))

    ebm::StatementBody make_while_loop(ebm::ExpressionRef condition, ebm::StatementRef body);

    ebm::StatementBody make_for_loop(ebm::StatementRef init, ebm::ExpressionRef condition, ebm::StatementRef step, ebm::StatementRef body);

#define EBM_WHILE_LOOP(ref_name, condition__, body__) \
    EBM_AST_STATEMENT(ref_name, make_while_loop, condition__, body__)

#define EBM_FOR_LOOP(ref_name, init, condition__, step, body__) \
    EBM_AST_STATEMENT(ref_name, make_for_loop, init, condition__, step, body__)

#define EBM_COUNTER_LOOP_START_CUSTOM(counter_name, counter_type)                       \
    EBM_AST_VARIABLE_REF(counter_name);                                                 \
    {                                                                                   \
        EBMU_INT_LITERAL(zero, 0);                                                      \
        MAYBE(zero_expr__, ctx.repository().get_expression(zero));                      \
        EBM_CAST(zero_casted, counter_type, zero_expr__.body.type, zero);               \
        EBM_DEFINE_ANONYMOUS_VARIABLE(counter_name##__, counter_type, zero_casted);     \
        EBM_AST_VARIABLE_REF_SET(counter_name, counter_name##__, counter_name##___def); \
    }

#define EBM_COUNTER_LOOP_START(counter_name)                                            \
    EBM_AST_VARIABLE_REF(counter_name);                                                 \
    {                                                                                   \
        EBMU_COUNTER_TYPE(counter_type);                                                \
        EBM_COUNTER_LOOP_START_CUSTOM(counter_name##__, counter_type);                  \
        EBM_AST_VARIABLE_REF_SET(counter_name, counter_name##__, counter_name##___def); \
    }

#define EBM_COUNTER_LOOP_END_BODY(loop_stmt, counter_name, counter_type, limit_expr__, body__) \
    ebm::StatementBody loop_stmt;                                                              \
    {                                                                                          \
        EBMU_BOOL_TYPE(bool_type);                                                             \
        EBM_BINARY_OP(cmp, ebm::BinaryOp::less, bool_type, counter_name, limit_expr__);        \
        EBM_INCREMENT(inc, counter_name, counter_type);                                        \
        loop_stmt = make_for_loop(counter_name##_def, cmp, inc, body__);                       \
    }

#define EBM_COUNTER_LOOP_END_CUSTOM(loop_stmt, counter_name, counter_type, limit_expr__, body__) \
    ebm::StatementRef loop_stmt;                                                                 \
    {                                                                                            \
        EBM_COUNTER_LOOP_END_BODY(body, counter_name, counter_type, limit_expr__, body__);       \
        EBMA_ADD_STATEMENT(added, std::move(body));                                              \
        loop_stmt = added;                                                                       \
    }

#define EBM_COUNTER_LOOP_END(loop_stmt, counter_name, limit_expr__, body__)                           \
    ebm::StatementRef loop_stmt;                                                                      \
    {                                                                                                 \
        EBMU_COUNTER_TYPE(counter_type);                                                              \
        EBM_COUNTER_LOOP_END_CUSTOM(loop_stmt##__, counter_name, counter_type, limit_expr__, body__); \
        loop_stmt = loop_stmt##__;                                                                    \
    }

    ebm::ExpressionBody make_array_size(ebm::TypeRef type, ebm::ExpressionRef array_expr);

#define EBM_ARRAY_SIZE(ref_name, array_expr__)                                          \
    ebm::ExpressionRef ref_name;                                                        \
    {                                                                                   \
        EBMU_COUNTER_TYPE(counter_type);                                                \
        EBMA_ADD_EXPR(array_size_ref____, make_array_size(counter_type, array_expr__)); \
        ref_name = array_size_ref____;                                                  \
    }

    ebm::LoweredIOStatement make_lowered_statement(ebm::LoweringIOType lowering_type, ebm::StatementRef body);

    ebm::StatementBody make_error_report(ebm::StringRef message, ebm::Expressions&& arguments);

#define EBM_ERROR_REPORT(ref_name, message__, arguments__) \
    EBM_AST_STATEMENT(ref_name, make_error_report, message__, arguments__)

    ebm::StatementBody make_if_statement(ebm::ExpressionRef condition, ebm::StatementRef then_block, ebm::StatementRef else_block);
#define EBM_IF_STATEMENT(ref_name, condition__, then_block__, else_block__) \
    EBM_AST_STATEMENT(ref_name, make_if_statement, condition__, then_block__, else_block__)

    ebm::StatementBody make_assert_statement(ebm::ExpressionRef condition, ebm::StatementRef lowered_statement);
#define EBM_ASSERT(ref_name, condition__, lowered_statement__) \
    EBM_AST_STATEMENT(ref_name, make_assert_statement, condition__, lowered_statement__)

    ebm::StatementBody make_lowered_statements(ebm::LoweredIOStatements&& lowered_statements);

#define EBM_LOWERED_IO_STATEMENTS(ref_name, lowered_statements__) \
    EBM_AST_STATEMENT(ref_name, make_lowered_statements, std::move(lowered_statements__))

    ebm::ExpressionBody make_member_access(ebm::TypeRef type, ebm::ExpressionRef base, ebm::ExpressionRef member);
#define EBM_MEMBER_ACCESS(ref_name, type, base__, member__) \
    EBM_AST_EXPRESSION(ref_name, make_member_access, type, base__, member__)

    ebm::ExpressionBody make_as_arg(ebm::TypeRef type, ebm::ExpressionRef target_expr);

#define EBM_AS_ARG(ref_name, type, target_expr__) \
    EBM_AST_EXPRESSION(ref_name, make_as_arg, type, target_expr__)

    ebm::ExpressionBody make_enum_member(ebm::TypeRef type, ebm::StatementRef enum_decl, ebm::ExpressionRef member);
#define EBM_ENUM_MEMBER(ref_name, type, enum_decl__, member__) \
    EBM_AST_EXPRESSION(ref_name, make_enum_member, type, enum_decl__, member__)

    ebm::ExpressionBody make_call(ebm::TypeRef typ, ebm::CallDesc&& call_desc);
#define EBM_CALL(ref_name, typ, call_desc__) \
    EBM_AST_EXPRESSION(ref_name, make_call, typ, call_desc__)

    ebm::StatementBody make_expression_statement(ebm::ExpressionRef expr);

#define EBM_EXPR_STATEMENT(ref_name, expr) \
    EBM_AST_STATEMENT(ref_name, make_expression_statement, expr)

    ebm::ExpressionBody make_error_check(ebm::TypeRef type, ebm::ExpressionRef target_expr);
#define EBM_IS_ERROR(ref_name, target_expr)                                       \
    ebm::ExpressionRef ref_name;                                                  \
    {                                                                             \
        EBMU_BOOL_TYPE(bool_type);                                                \
        EBMA_ADD_EXPR(error_check_ref, make_error_check(bool_type, target_expr)); \
        ref_name = error_check_ref;                                               \
    }

    ebm::StatementBody make_error_return(ebm::ExpressionRef value, ebm::StatementRef related_function, ebm::StatementRef related_field);
#define EBM_ERROR_RETURN(ref_name, value, related_function, related_field) \
    EBM_AST_STATEMENT(ref_name, make_error_return, value, related_function, related_field)

    ebm::ExpressionBody make_max_value(ebm::TypeRef type, ebm::ExpressionRef lowered_expr);

#define EBM_MAX_VALUE(ref_name, type, lowered_expr) \
    EBM_AST_EXPRESSION(ref_name, make_max_value, type, lowered_expr)

    ebm::ExpressionBody make_can_read_stream(ebm::TypeRef type, ebm::StatementRef io_ref, ebm::StreamType stream_type, ebm::Size num_bytes);

#define EBM_CAN_READ_STREAM(ref_name, io_ref, stream_type, num_bytes) \
    EBMU_BOOL_TYPE(ref_name##_type_____);                             \
    EBM_AST_EXPRESSION(ref_name, make_can_read_stream, ref_name##_type_____, io_ref, stream_type, num_bytes);

    ebm::StatementBody make_append(ebm::ExpressionRef target, ebm::ExpressionRef value);
#define EBM_APPEND(ref_name, target, value) \
    EBM_AST_STATEMENT(ref_name, make_append, target, value)

    ebm::ExpressionBody make_get_remaining_bytes(ebm::TypeRef type, ebm::StreamType stream_type, ebm::StatementRef io_ref);

#define EBM_GET_REMAINING_BYTES(ref_name, stream_type, io_ref) \
    EBMU_COUNTER_TYPE(ref_name##_type_____);                   \
    EBM_AST_EXPRESSION(ref_name, make_get_remaining_bytes, ref_name##_type_____, stream_type, io_ref)
    ebm::StatementBody make_break(ebm::StatementRef loop_id);
    ebm::StatementBody make_continue(ebm::StatementRef loop_id);

#define EBM_BREAK(ref_name, loop_id) \
    EBM_AST_STATEMENT(ref_name, make_break, loop_id)
#define EBM_CONTINUE(ref_name, loop_id) \
    EBM_AST_STATEMENT(ref_name, make_continue, loop_id)

    ebm::ExpressionBody make_range(ebm::TypeRef type, ebm::ExpressionRef start, ebm::ExpressionRef end);

#define EBM_RANGE(ref_name, type, start, end) \
    EBM_AST_EXPRESSION(ref_name, make_range, type, start, end)

    ebm::ExpressionBody make_sub_range_init(ebm::TypeRef type, ebm::StatementRef init);

#define EBM_SUB_RANGE_INIT(ref_name, type, init) \
    EBM_AST_EXPRESSION(ref_name, make_sub_range_init, type, init)

    ebm::StatementBody make_property_member_decl(ebm::PropertyMemberDecl&& prop_member);

#define EBM_PROPERTY_MEMBER_DECL(ref_name, prop_member__) \
    EBM_AST_STATEMENT(ref_name, make_property_member_decl, prop_member__)

    ebm::StatementBody make_property_decl(ebm::PropertyDecl&& prop_decl);

#define EBM_PROPERTY_DECL(ref_name, prop_) \
    EBM_AST_STATEMENT(ref_name, make_property_decl, prop_)

    ebm::ExpressionBody make_or_cond(ebm::TypeRef type, ebm::Expressions&& conds);

#define EBM_OR_COND(ref_name, type, conds) \
    EBM_AST_EXPRESSION(ref_name, make_or_cond, type, conds)

    ebm::IOData make_io_data(ebm::StatementRef io_ref, ebm::StatementRef field, ebm::ExpressionRef target, ebm::TypeRef data_type, ebm::IOAttribute attr, ebm::Size size);

    ebm::StatementBody make_return(ebm::ExpressionRef ret, ebm::StatementRef related_function);

#define EBM_RETURN(ref_name, value, related_function) \
    EBM_AST_STATEMENT(ref_name, make_return, value, related_function)

    ebm::ExpressionBody make_addressof(ebm::TypeRef type, ebm::ExpressionRef target);
    ebm::ExpressionBody make_optionalof(ebm::TypeRef type, ebm::ExpressionRef target);

#define EBM_ADDRESSOF(ref_name, type, target) \
    EBM_AST_EXPRESSION(ref_name, make_addressof, type, target)

#define EBM_OPTIONALOF(ref_name, type, target) \
    EBM_AST_EXPRESSION(ref_name, make_optionalof, type, target)

    ebm::ExpressionBody make_setter_status(ebm::TypeRef type, ebm::SetterStatus status);

#define EBM_SETTER_STATUS(ref_name, type, status) \
    EBM_AST_EXPRESSION(ref_name, make_setter_status, type, status)

    ebm::ExpressionBody make_bool_literal(ebm::TypeRef type, bool value);

#define EBM_BOOL_LITERAL(ref_name, value) \
    EBMU_BOOL_TYPE(ref_name##_type_____); \
    EBM_AST_EXPRESSION(ref_name, make_bool_literal, ref_name##_type_____, value)

    ebm::ExpressionBody make_self(ebm::TypeRef type);
#define EBM_SELF(ref_name, typ) \
    EBM_AST_EXPRESSION(ref_name, make_self, typ)

    ebm::StatementBody make_init_check_statement(ebm::InitCheck&& init_check);
#define EBM_INIT_CHECK_STATEMENT(ref_name, init_check__) \
    EBM_AST_STATEMENT(ref_name, make_init_check_statement, init_check__)

    expected<ebm::Size> make_fixed_size(size_t n, ebm::SizeUnit unit);
    expected<ebm::Size> make_dynamic_size(ebm::ExpressionRef ref, ebm::SizeUnit unit);
    ebm::Size get_size(size_t bit_size);

    ebm::Condition make_condition(ebm::ExpressionRef cond);

    ebm::StatementBody make_endian_convert(ebm::StatementKind kind, ebm::Endian endian, ebm::ExpressionRef src, ebm::ExpressionRef target, ebm::StatementRef lowered);

#define EBM_ENDIAN_CONVERT(ref_name, kind, endian, src, target, lowered) \
    EBM_AST_STATEMENT(ref_name, make_endian_convert, kind, endian, src, target, lowered)

    ebm::StatementBody make_length_check(ebm::LengthCheckType type, ebm::ExpressionRef target, ebm::ExpressionRef expected_length, ebm::StatementRef related_function, ebm::StatementRef lowered_statement);
#define EBM_LENGTH_CHECK(ref_name, type, target, expected_length, related_function, lowered_statement) \
    EBM_AST_STATEMENT(ref_name, make_length_check, type, target, expected_length, related_function, lowered_statement)

#define COMMON_BUFFER_SETUP(IO_MACRO, io_stmt, io_ref, field_ref, annot)                       \
    EBMU_U8_N_ARRAY(u8_n_array, n, annot);                                                     \
    EBM_DEFAULT_VALUE(new_obj_ref, u8_n_array);                                                \
    EBM_DEFINE_ANONYMOUS_VARIABLE(buffer, u8_n_array, new_obj_ref);                            \
    EBMU_UINT_TYPE(value_type, n * 8);                                                         \
    EBMU_INT_LITERAL(zero, 0);                                                                 \
    MAYBE(io_size, make_fixed_size(n, ebm::SizeUnit::BYTE_FIXED));                             \
    IO_MACRO(io_stmt, (make_io_data(io_ref, field_ref, buffer, u8_n_array, endian, io_size))); \
    EBMU_U8(u8_t);

}  // namespace ebmgen
