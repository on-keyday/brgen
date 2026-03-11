/*license*/
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include "ast.hpp"
#include "flags.hpp"
#include "../../bm/binary_module.hpp"
#include "../../bm2/context.hpp"

namespace rebgn {

// Function type for generating an AST Expression from an AbstractOp
using AstGeneratorFunc = std::function<std::unique_ptr<Expression>(const bm2::Context& ctx, size_t code_idx, Flags& flags)>;

// Map to store AST generator functions
inline std::map<rebgn::AbstractOp, AstGeneratorFunc> ast_generators;

// Forward declarations for recursive calls
std::unique_ptr<Expression> generate_expression_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<Statement> generate_statement_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_program_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_program_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_format_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_format_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_field_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_property_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_property_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_function_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_function_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_enum_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_enum_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_enum_member_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_union_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_union_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_union_member_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_union_member_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_state_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_state_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_bit_field_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_end_bit_field_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_encoder_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);
std::unique_ptr<rebgn::Statement> generate_define_decoder_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags);

// Helper to find the end of an IF block (and its ELIF/ELSE branches)
// Returns the index of the END_IF, or the next statement after the ELSE block
inline size_t find_end_of_if_block(const bm2::Context& ctx, size_t start_idx) {
    size_t nested_if_count = 0;
    for (size_t i = start_idx + 1; i < ctx.bm.code.size(); ++i) {
        const auto& current_code = ctx.bm.code[i];
        if (current_code.op == rebgn::AbstractOp::IF) {
            nested_if_count++;
        } else if (current_code.op == rebgn::AbstractOp::END_IF) {
            if (nested_if_count == 0) {
                return i; // Found the matching END_IF
            } else {
                nested_if_count--;
            }
        }
    }
    return ctx.bm.code.size(); // Should not happen in a well-formed AST
}

// Generator for IMMEDIATE_INT
inline std::unique_ptr<rebgn::Expression> generate_immediate_int(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit(std::to_string(code.int_value()->value()));
}

// Generator for IMMEDIATE_STRING
inline std::unique_ptr<rebgn::Expression> generate_immediate_string(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit("" + ctx.ident_table.at(code.ident()->value()) + "");
}

// Generator for IMMEDIATE_TRUE
inline std::unique_ptr<rebgn::Expression> generate_immediate_true(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("true");
}

// Generator for IMMEDIATE_FALSE
inline std::unique_ptr<rebgn::Expression> generate_immediate_false(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("false");
}

// Generator for IMMEDIATE_INT64
inline std::unique_ptr<rebgn::Expression> generate_immediate_int64(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit(std::to_string(code.int_value64().value()) + "LL");
}

// Generator for IMMEDIATE_CHAR
inline std::unique_ptr<rebgn::Expression> generate_immediate_char(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return lit("'" + std::string(1, static_cast<char>(code.int_value()->value())) + "'");
}

// Generator for IMMEDIATE_TYPE
inline std::unique_ptr<rebgn::Expression> generate_immediate_type(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    // Assuming StorageRef points to an identifier that represents the type name
    return lit(ctx.ident_table.at(code.type().value().ref.value()));
}

// Generator for EMPTY_PTR
inline std::unique_ptr<rebgn::Expression> generate_empty_ptr(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("nullptr");
}

// Generator for EMPTY_OPTIONAL
inline std::unique_ptr<rebgn::Expression> generate_empty_optional(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return lit("std::nullopt");
}

// Generator for PHI, DECLARE_VARIABLE, DEFINE_VARIABLE_REF, BEGIN_COND_BLOCK
inline std::unique_ptr<rebgn::Expression> generate_ref_evaluation(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    return generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
}

// Generator for ASSIGN
inline std::unique_ptr<rebgn::Expression> generate_assign_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return bin_op(std::move(left_expr), "=", std::move(right_expr));
}

// Generator for PROPERTY_ASSIGN
inline std::unique_ptr<rebgn::Expression> generate_property_assign_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return bin_op(std::move(left_expr), "=", std::move(right_expr));
}

// Generator for ASSERT
inline std::unique_ptr<rebgn::Statement> generate_assert_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> condition = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    return expr_stmt(func_call("assert", std::move(condition)));
}

// Generator for LENGTH_CHECK
inline std::unique_ptr<rebgn::Statement> generate_length_check_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return expr_stmt(func_call("length_check", std::move(left_expr), std::move(right_expr)));
}

// Generator for EXPLICIT_ERROR
inline std::unique_ptr<rebgn::Statement> generate_explicit_error_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    // Assuming ref is a string literal for now
    std::string error_message = ctx.ident_table.at(code.param()->refs[0].value());    std::string quoted_error_message = "\"" + error_message + "\"";    return expr_stmt(func_call("explicit_error", lit(quoted_error_message)));
}

// Generator for ACCESS
inline std::unique_ptr<rebgn::Expression> generate_access_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::string right_ident = ctx.ident_table.at(code.right_ref()->value());

    // Check if the left_expr is an IMMEDIATE_TYPE (representing an enum)
    // This is a simplification; a more robust solution might involve type checking the AST node
    bool is_enum_member = (ctx.bm.code[ctx.ident_index_table.at(code.left_ref()->value())].op == rebgn::AbstractOp::IMMEDIATE_TYPE);

    if (is_enum_member) {
        // Enum member access (e.g., EnumType::EnumValue)
        return bin_op(std::move(left_expr), "::", lit(right_ident));
    } else {
        // Normal member access (e.g., object.member)
        return bin_op(std::move(left_expr), ".", lit(right_ident));
    }
}

// Generator for INDEX
inline std::unique_ptr<rebgn::Expression> generate_index_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return idx_expr(std::move(left_expr), std::move(right_expr));
}

// Generator for APPEND
inline std::unique_ptr<rebgn::Expression> generate_append_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    return member_func_call(std::move(left_expr), "append", std::move(right_expr));
}

// Generator for INC
inline std::unique_ptr<rebgn::Expression> generate_inc_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    return un_op("++", std::move(expr));
}

// Generator for ARRAY_SIZE
inline std::unique_ptr<rebgn::Expression> generate_array_size_expression(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> target_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    return member_func_call(std::move(target_expr), "size");
}

// Generator for BINARY operations
inline std::unique_ptr<rebgn::Expression> generate_binary_op(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string op_str = to_string(code.bop().value());
    size_t left_idx = ctx.ident_index_table.at(code.left_ref()->value());
    size_t right_idx = ctx.ident_index_table.at(code.right_ref()->value());
    std::unique_ptr<rebgn::Expression> left_expr = generate_expression_from_code(ctx, left_idx, flags);
    std::unique_ptr<rebgn::Expression> right_expr = generate_expression_from_code(ctx, right_idx, flags);
    return bin_op(std::move(left_expr), op_str, std::move(right_expr));
}

// Generator for UNARY operations
inline std::unique_ptr<rebgn::Expression> generate_unary_op(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string op_str = to_string(code.uop().value());
    size_t ref_idx = ctx.ident_index_table.at(code.ref()->value());
    std::unique_ptr<rebgn::Expression> expr = generate_expression_from_code(ctx, ref_idx, flags);
    return un_op(op_str, std::move(expr));
}

// Generator for IF statements
inline std::unique_ptr<rebgn::Statement> generate_if_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> condition = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);

    std::vector<std::unique_ptr<rebgn::Statement>> then_statements;
    size_t current_idx = code_idx + 1;
    size_t end_of_if = find_end_of_if_block(ctx, code_idx);

    // Parse then block
    while (current_idx < end_of_if && ctx.bm.code[current_idx].op != rebgn::AbstractOp::ELIF && ctx.bm.code[current_idx].op != rebgn::AbstractOp::ELSE) {
        then_statements.push_back(generate_statement_from_code(ctx, current_idx, flags));
        current_idx++;
    }
    std::unique_ptr<rebgn::Block> then_block = block(std::move(then_statements));

    std::unique_ptr<rebgn::Statement> else_block = nullptr;

    // Handle ELIF and ELSE
    if (current_idx < end_of_if) {
        if (ctx.bm.code[current_idx].op == rebgn::AbstractOp::ELIF) {
            // ELIF is essentially an IF statement in the else branch
            else_block = generate_if_statement(ctx, current_idx, flags); // Recursively call for ELIF
        } else if (ctx.bm.code[current_idx].op == rebgn::AbstractOp::ELSE) {
            std::vector<std::unique_ptr<rebgn::Statement>> else_statements;
            current_idx++; // Move past ELSE op
            while (current_idx < end_of_if) {
                else_statements.push_back(generate_statement_from_code(ctx, current_idx, flags));
                current_idx++;
            }
            else_block = block(std::move(else_statements));
        }
    }

    return if_stmt(std::move(condition), std::move(then_block), std::move(else_block));
}

// Generator for LOOP statements
inline std::unique_ptr<rebgn::Statement> generate_loop_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> condition = nullptr; // Default to infinite loop
    if (code.op == rebgn::AbstractOp::LOOP_CONDITION) {
        condition = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    }

    // For now, loop body is a placeholder
    std::vector<std::unique_ptr<rebgn::Statement>> loop_body_statements;
    loop_body_statements.push_back(expr_stmt(lit("// loop body")));
    std::unique_ptr<rebgn::Block> loop_body = block(std::move(loop_body_statements));

    return loop_stmt(std::move(loop_body), std::move(condition));
}

// Generator for RETURN_TYPE
inline std::unique_ptr<rebgn::Statement> generate_return_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::unique_ptr<rebgn::Expression> expr = nullptr;
    // Assuming RETURN_TYPE might have an associated expression via 'ref'
    if (code.ref()) { // Check if ref exists
        expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    }
    return ret_stmt(std::move(expr));
}

// Generator for DEFINE_VARIABLE
inline std::unique_ptr<rebgn::Statement> generate_variable_declaration(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string var_name = "my_variable";
    if (code.ident()) {
        var_name = ctx.ident_table.at(code.ident()->value());
    }
    std::unique_ptr<rebgn::Expression> initializer = nullptr;
    if (code.ref()) {
        initializer = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.ref()->value()), flags);
    }
    return var_decl("int", var_name, std::move(initializer));
}

// Central dispatch for expressions
std::unique_ptr<rebgn::Expression> generate_expression_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    switch (code.op) {
        case rebgn::AbstractOp::IMMEDIATE_INT:
            return generate_immediate_int(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_STRING:
            return generate_immediate_string(ctx, code_idx, flags);
        case rebgn::AbstractOp::BINARY:
            return generate_binary_op(ctx, code_idx, flags);
        case rebgn::AbstractOp::UNARY:
            return generate_unary_op(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_TRUE:
            return generate_immediate_true(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_FALSE:
            return generate_immediate_false(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_INT64:
            return generate_immediate_int64(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_CHAR:
            return generate_immediate_char(ctx, code_idx, flags);
        case rebgn::AbstractOp::IMMEDIATE_TYPE:
            return generate_immediate_type(ctx, code_idx, flags);
        case rebgn::AbstractOp::EMPTY_PTR:
            return generate_empty_ptr(ctx, code_idx, flags);
        case rebgn::AbstractOp::EMPTY_OPTIONAL:
            return generate_empty_optional(ctx, code_idx, flags);
        case rebgn::AbstractOp::PHI:
        case rebgn::AbstractOp::DECLARE_VARIABLE:
        case rebgn::AbstractOp::DEFINE_VARIABLE_REF:
        case rebgn::AbstractOp::BEGIN_COND_BLOCK:
            return generate_ref_evaluation(ctx, code_idx, flags);
        case rebgn::AbstractOp::ASSIGN:
            return generate_assign_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::PROPERTY_ASSIGN:
            return generate_property_assign_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::ACCESS:
            return generate_access_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::INDEX:
            return generate_index_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::ARRAY_SIZE:
            return generate_array_size_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::APPEND:
            return generate_append_expression(ctx, code_idx, flags);
        case rebgn::AbstractOp::INC:
            return generate_inc_expression(ctx, code_idx, flags);
        // Add more expression types here
        default:
            return lit("// Unimplemented Expression: " + std::string(to_string(code.op)));
    }
}

// Central dispatch for statements
std::unique_ptr<rebgn::Statement> generate_statement_from_code(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    switch (code.op) {
        case rebgn::AbstractOp::IF:
        case rebgn::AbstractOp::ELIF:
        case rebgn::AbstractOp::ELSE:
            return generate_if_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::LOOP_INFINITE:
        case rebgn::AbstractOp::LOOP_CONDITION:
            return generate_loop_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::RETURN_TYPE:
            return generate_return_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_VARIABLE:
            return generate_variable_declaration(ctx, code_idx, flags);
        case rebgn::AbstractOp::ASSERT:
            return generate_assert_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::LENGTH_CHECK:
            return generate_length_check_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::EXPLICIT_ERROR:
            return generate_explicit_error_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_PROGRAM:
            return generate_define_program_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_PROGRAM:
            return generate_end_program_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_FORMAT:
            return generate_define_format_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_FORMAT:
            return generate_end_format_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_FIELD:
            return generate_define_field_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_PROPERTY:
            return generate_define_property_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_PROPERTY:
            return generate_end_property_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_FUNCTION:
            return generate_define_function_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_FUNCTION:
            return generate_end_function_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_ENUM:
            return generate_define_enum_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_ENUM:
            return generate_end_enum_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_ENUM_MEMBER:
            return generate_define_enum_member_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_UNION:
            return generate_define_union_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_UNION:
            return generate_end_union_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_UNION_MEMBER:
            return generate_define_union_member_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_UNION_MEMBER:
            return generate_end_union_member_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_STATE:
            return generate_define_state_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_STATE:
            return generate_end_state_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_BIT_FIELD:
            return generate_define_bit_field_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::END_BIT_FIELD:
            return generate_end_bit_field_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_ENCODER:
            return generate_define_encoder_statement(ctx, code_idx, flags);
        case rebgn::AbstractOp::DEFINE_DECODER:
            return generate_define_decoder_statement(ctx, code_idx, flags);
        default: {
            // If it's an expression, wrap it in an ExpressionStatement
            // Otherwise, it's an unimplemented statement
            if (is_expr(code.op)) {
                return expr_stmt(generate_expression_from_code(ctx, code_idx, flags));
            } else {
                return expr_stmt(lit("// Unimplemented Statement: " + std::string(to_string(code.op))));
            }
        }
    }
}

// Generator for DEFINE_PROGRAM
inline std::unique_ptr<rebgn::Statement> generate_define_program_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string program_name = "main"; // Default name
    if (code.ident()) {
        program_name = ctx.ident_table.at(code.ident()->value());
    }
    // For now, just return a comment indicating the program definition
    return expr_stmt(lit("// Define Program: " + program_name));
}

// Generator for END_PROGRAM
inline std::unique_ptr<rebgn::Statement> generate_end_program_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    // For now, just return a comment indicating the end of program
    return expr_stmt(lit("// End Program"));
}

// Generator for DEFINE_FORMAT
inline std::unique_ptr<rebgn::Statement> generate_define_format_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string format_name = "";
    if (code.ident()) {
        format_name = ctx.ident_table.at(code.ident()->value());
    }
    return expr_stmt(lit("// Define Format: " + format_name));
}

// Generator for END_FORMAT
inline std::unique_ptr<rebgn::Statement> generate_end_format_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Format"));
}

// Generator for DEFINE_FIELD
inline std::unique_ptr<rebgn::Statement> generate_define_field_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string field_name = "";
    if (code.ident()) {
        field_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string belong_name = "";
    if (code.belong()) {
        belong_name = ctx.ident_table.at(code.belong()->value());
    }
    std::string type_name = "";
    if (code.type()) {
        type_name = ctx.ident_table.at(code.type()->ref.value());
    }
    return expr_stmt(lit("// Define Field: " + field_name + ", Belong: " + belong_name + ", Type: " + type_name));
}

// Generator for DEFINE_PROPERTY
inline std::unique_ptr<rebgn::Statement> generate_define_property_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string property_name = "";
    if (code.ident()) {
        property_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string belong_name = "";
    if (code.belong()) {
        belong_name = ctx.ident_table.at(code.belong()->value());
    }
    return expr_stmt(lit("// Define Property: " + property_name + ", Belong: " + belong_name));
}

// Generator for END_PROPERTY
inline std::unique_ptr<rebgn::Statement> generate_end_property_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Property"));
}

// Generator for DEFINE_FUNCTION
inline std::unique_ptr<rebgn::Statement> generate_define_function_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string function_name = "";
    if (code.ident()) {
        function_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string belong_name = "";
    if (code.belong()) {
        belong_name = ctx.ident_table.at(code.belong()->value());
    }
    std::string func_type = to_string(code.func_type().value());
    return expr_stmt(lit("// Define Function: " + function_name + ", Belong: " + belong_name + ", Type: " + func_type));
}

// Generator for END_FUNCTION
inline std::unique_ptr<rebgn::Statement> generate_end_function_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Function"));
}

// Generator for DEFINE_ENUM
inline std::unique_ptr<rebgn::Statement> generate_define_enum_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string enum_name = "";
    if (code.ident()) {
        enum_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string type_name = "";
    if (code.type()) {
        type_name = ctx.ident_table.at(code.type()->ref.value());
    }
    return expr_stmt(lit("// Define Enum: " + enum_name + ", Type: " + type_name));
}

// Generator for END_ENUM
inline std::unique_ptr<rebgn::Statement> generate_end_enum_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Enum"));
}

// Generator for DEFINE_ENUM_MEMBER
inline std::unique_ptr<rebgn::Statement> generate_define_enum_member_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string member_name = "";
    if (code.ident()) {
        member_name = ctx.ident_table.at(code.ident()->value());
    }
    std::unique_ptr<rebgn::Expression> left_expr = nullptr;
    if (code.left_ref()) {
        left_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.left_ref()->value()), flags);
    }
    std::unique_ptr<rebgn::Expression> right_expr = nullptr;
    if (code.right_ref()) {
        right_expr = generate_expression_from_code(ctx, ctx.ident_index_table.at(code.right_ref()->value()), flags);
    }
    return expr_stmt(lit("// Define Enum Member: " + member_name + ", Left Ref: " + (code.left_ref() ? std::to_string(code.left_ref()->value()) : "null") + ", Right Ref: " + (code.right_ref() ? std::to_string(code.right_ref()->value()) : "null")));
}

// Generator for DEFINE_UNION
inline std::unique_ptr<rebgn::Statement> generate_define_union_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string union_name = "";
    if (code.ident()) {
        union_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string belong_name = "";
    if (code.belong()) {
        belong_name = ctx.ident_table.at(code.belong()->value());
    }
    return expr_stmt(lit("// Define Union: " + union_name + ", Belong: " + belong_name));
}

// Generator for END_UNION
inline std::unique_ptr<rebgn::Statement> generate_end_union_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Union"));
}

// Generator for DEFINE_UNION_MEMBER
inline std::unique_ptr<rebgn::Statement> generate_define_union_member_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string member_name = "";
    if (code.ident()) {
        member_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string belong_name = "";
    if (code.belong()) {
        belong_name = ctx.ident_table.at(code.belong()->value());
    }
    return expr_stmt(lit("// Define Union Member: " + member_name + ", Belong: " + belong_name));
}

// Generator for END_UNION_MEMBER
inline std::unique_ptr<rebgn::Statement> generate_end_union_member_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Union Member"));
}

// Generator for DEFINE_STATE
inline std::unique_ptr<rebgn::Statement> generate_define_state_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string state_name = "";
    if (code.ident()) {
        state_name = ctx.ident_table.at(code.ident()->value());
    }
    return expr_stmt(lit("// Define State: " + state_name));
}

// Generator for END_STATE
inline std::unique_ptr<rebgn::Statement> generate_end_state_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End State"));
}

// Generator for DEFINE_BIT_FIELD
inline std::unique_ptr<rebgn::Statement> generate_define_bit_field_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string field_name = "";
    if (code.ident()) {
        field_name = ctx.ident_table.at(code.ident()->value());
    }
    std::string belong_name = "";
    if (code.belong()) {
        belong_name = ctx.ident_table.at(code.belong()->value());
    }
    std::string type_name = "";
    if (code.type()) {
        type_name = ctx.ident_table.at(code.type()->ref.value());
    }
    return expr_stmt(lit("// Define Bit Field: " + field_name + ", Belong: " + belong_name + ", Type: " + type_name));
}

// Generator for END_BIT_FIELD
inline std::unique_ptr<rebgn::Statement> generate_end_bit_field_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    return expr_stmt(lit("// End Bit Field"));
}

// Generator for DEFINE_ENCODER
inline std::unique_ptr<rebgn::Statement> generate_define_encoder_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string left_ref_str = (code.left_ref() ? std::to_string(code.left_ref()->value()) : "null");
    std::string right_ref_str = (code.right_ref() ? std::to_string(code.right_ref()->value()) : "null");
    return expr_stmt(lit("// Define Encoder: Left Ref: " + left_ref_str + ", Right Ref: " + right_ref_str));
}

// Generator for DEFINE_DECODER
inline std::unique_ptr<rebgn::Statement> generate_define_decoder_statement(const bm2::Context& ctx, size_t code_idx, Flags& flags) {
    const auto& code = ctx.bm.code[code_idx];
    std::string left_ref_str = (code.left_ref() ? std::to_string(code.left_ref()->value()) : "null");
    std::string right_ref_str = (code.right_ref() ? std::to_string(code.right_ref()->value()) : "null");
    return expr_stmt(lit("// Define Decoder: Left Ref: " + left_ref_str + ", Right Ref: " + right_ref_str));
}

// Function to initialize the map (call once)
inline void initialize_ast_generators() {
    // Expression generators are handled by generate_expression_from_code
    // Statement generators are handled by generate_statement_from_code
}

// Function to get the AST generator for a given AbstractOp
// This function is no longer directly used for dispatching, but kept for compatibility if needed
inline rebgn::AstGeneratorFunc get_ast_generator(rebgn::AbstractOp op) {
    // This function's role has been largely replaced by generate_expression_from_code
    // and generate_statement_from_code
    return nullptr; 
}

} // namespace rebgn