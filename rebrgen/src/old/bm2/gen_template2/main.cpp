#include <iostream>
#include "flags.hpp"
#include "ast.hpp"
#include "code_gen_map.hpp"
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include "../../bm/binary_module.hpp"
#include "../../bm2/context.hpp"
#include "../../bm2/output.hpp"
#include <binary/writer.h>
#include <vector>

namespace rebgn {
    bool write_code_template(futils::wrap::UtfOut& w, Flags& flags) {
        if (flags.cmd_options.lang_name.empty()) {
            std::cerr << "error: lang option is required" << std::endl;
            return false;
        }

        initialize_ast_generators();

        AstRenderer renderer;
        std::unique_ptr<Expression> generated_expr;
        std::unique_ptr<Statement> generated_stmt;

        // Dummy BinaryModule
        rebgn::BinaryModule bm;
        bm.code.reserve(30);  // Reserve some space

        // Populate identifiers.refs and ident_indexes.refs for dummy data
        // This simulates the data that would be loaded into Context
        rebgn::StringRef string_ref_456;
        string_ref_456.code = rebgn::Varint(456);
        string_ref_456.string.data = "test_string";
        bm.identifiers.refs.push_back(string_ref_456);

        rebgn::IdentIndex ident_index_456;
        ident_index_456.ident = rebgn::Varint(456);
        ident_index_456.index = rebgn::Varint(1);  // Index of IMMEDIATE_STRING
        bm.ident_indexes.refs.push_back(ident_index_456);

        rebgn::StringRef string_ref_789;
        string_ref_789.code = rebgn::Varint(789);
        string_ref_789.string.data = "my_variable_name";
        bm.identifiers.refs.push_back(string_ref_789);

        rebgn::IdentIndex ident_index_789;
        ident_index_789.ident = rebgn::Varint(789);
        ident_index_789.index = rebgn::Varint(18);  // Index of DEFINE_VARIABLE
        bm.ident_indexes.refs.push_back(ident_index_789);

        // Add IdentIndex for IMMEDIATE_INT at index 2
        rebgn::IdentIndex ident_index_2;
        ident_index_2.ident = rebgn::Varint(2);
        ident_index_2.index = rebgn::Varint(2);
        bm.ident_indexes.refs.push_back(ident_index_2);

        // Add IdentIndex for IMMEDIATE_INT at index 3
        rebgn::IdentIndex ident_index_3;
        ident_index_3.ident = rebgn::Varint(3);
        ident_index_3.index = rebgn::Varint(3);
        bm.ident_indexes.refs.push_back(ident_index_3);

        // Add IdentIndex for IMMEDIATE_INT at index 5
        rebgn::IdentIndex ident_index_5;
        ident_index_5.ident = rebgn::Varint(5);
        ident_index_5.index = rebgn::Varint(5);
        bm.ident_indexes.refs.push_back(ident_index_5);

        // Add IdentIndex for IMMEDIATE_INT at index 7
        rebgn::IdentIndex ident_index_7;
        ident_index_7.ident = rebgn::Varint(7);
        ident_index_7.index = rebgn::Varint(7);
        bm.ident_indexes.refs.push_back(ident_index_7);

        // Add IdentIndex for IMMEDIATE_INT at index 9
        rebgn::IdentIndex ident_index_9;
        ident_index_9.ident = rebgn::Varint(9);
        ident_index_9.index = rebgn::Varint(9);
        bm.ident_indexes.refs.push_back(ident_index_9);

        // Add IdentIndex for IMMEDIATE_INT at index 11
        rebgn::IdentIndex ident_index_11;
        ident_index_11.ident = rebgn::Varint(11);
        ident_index_11.index = rebgn::Varint(11);
        bm.ident_indexes.refs.push_back(ident_index_11);

        // Add IdentIndex for IMMEDIATE_INT at index 13
        rebgn::IdentIndex ident_index_13;
        ident_index_13.ident = rebgn::Varint(13);
        ident_index_13.index = rebgn::Varint(13);
        bm.ident_indexes.refs.push_back(ident_index_13);

        // Add IdentIndex for ASSERT at index 20
        rebgn::IdentIndex ident_index_20;
        ident_index_20.ident = rebgn::Varint(20);
        ident_index_20.index = rebgn::Varint(20);
        bm.ident_indexes.refs.push_back(ident_index_20);

        // Add IdentIndex for LENGTH_CHECK at index 21
        rebgn::IdentIndex ident_index_21;
        ident_index_21.ident = rebgn::Varint(21);
        ident_index_21.index = rebgn::Varint(21);
        bm.ident_indexes.refs.push_back(ident_index_21);

        // For Param, we need to create a StringRef for the string literal
        rebgn::StringRef error_string_ref;
        error_string_ref.code = rebgn::Varint(220);
        error_string_ref.string.data = "Error Message";
        bm.identifiers.refs.push_back(error_string_ref);

        rebgn::Param error_param;
        error_param.refs.push_back(rebgn::Varint(220));  // Reference to the string literal

        // Dummy Context setup
        std::vector<unsigned char> buffer;                                // Buffer for dummy_writer
        futils::binary::writer dummy_writer{futils::view::wvec(buffer)};  // Create a dummy writer
        bm2::Output dummy_output;                                         // Create a dummy output
        auto dummy_escape_ident = [](bm2::Context& ctx, std::uint64_t id, std::string& s) { /* do nothing */ };
        bm2::Context ctx(dummy_writer, bm, dummy_output, "r", "w", "this", dummy_escape_ident);

        // 0: IMMEDIATE_INT 123
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(123));

        // 1: IMMEDIATE_STRING "test_string" (ident 456)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_STRING;
        bm.code.back().ident(rebgn::Varint(456));

        // 2: IMMEDIATE_INT 10 (for binary op)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(10));

        // 3: IMMEDIATE_INT 20 (for binary op)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(20));

        // 4: BINARY (add) left_ref=2, right_ref=3
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::BINARY;
        bm.code.back().bop(rebgn::BinaryOp::add);
        bm.code.back().left_ref(rebgn::Varint(2));   // refers to code[2] (IMMEDIATE_INT 10)
        bm.code.back().right_ref(rebgn::Varint(3));  // refers to code[3] (IMMEDIATE_INT 20)

        // 5: IMMEDIATE_INT 5 (for unary op)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(5));

        // 6: UNARY (minus_sign) ref=5
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::UNARY;
        bm.code.back().uop(rebgn::UnaryOp::minus_sign);
        bm.code.back().ref(rebgn::Varint(5));  // refers to code[5] (IMMEDIATE_INT 5)

        // 7: IMMEDIATE_INT 1 (for if condition and loop condition)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(1));

        // 8: IF (condition ref=7)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IF;
        bm.code.back().ref(rebgn::Varint(7));  // refers to code[7] (IMMEDIATE_INT 1)

        // 9: IMMEDIATE_INT 100 (then block content)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(100));

        // 10: ELIF (condition ref=7)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::ELIF;
        bm.code.back().ref(rebgn::Varint(7));  // refers to code[7] (IMMEDIATE_INT 1)

        // 11: IMMEDIATE_INT 200 (elif then block content)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(200));

        // 12: ELSE
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::ELSE;

        // 13: IMMEDIATE_INT 300 (else block content)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::IMMEDIATE_INT;
        bm.code.back().int_value(rebgn::Varint(300));

        // 14: END_IF
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_IF;

        // 15: LOOP_INFINITE
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::LOOP_INFINITE;

        // 16: LOOP_CONDITION (condition ref=7)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::LOOP_CONDITION;
        bm.code.back().ref(rebgn::Varint(7));  // refers to code[7] (IMMEDIATE_INT 1)

        // 17: RETURN_TYPE (no expression for now)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::RETURN_TYPE;

        // 18: DEFINE_VARIABLE (ident 789, initializer ref=0) - refers to IMMEDIATE_INT 123
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_VARIABLE;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().ref(rebgn::Varint(2));  // refers to code[2] (IMMEDIATE_INT 10)

        // Test with IMMEDIATE_INT
        generated_expr = generate_expression_from_code(ctx, 0, flags);
        w << "// Immediate Int Test\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with IMMEDIATE_STRING
        generated_expr = generate_expression_from_code(ctx, 1, flags);
        w << "// Immediate String Test\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with BINARY operation (ADD)
        generated_expr = generate_expression_from_code(ctx, 4, flags);
        w << "// Binary Operation Test (ADD)\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with UNARY operation (MINUS_SIGN)
        generated_expr = generate_expression_from_code(ctx, 6, flags);
        w << "// Unary Operation Test (MINUS_SIGN)\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // Test with IF-ELIF-ELSE structure
        generated_stmt = generate_statement_from_code(ctx, 8, flags);
        w << "// IF-ELIF-ELSE Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with LOOP_INFINITE
        generated_stmt = generate_statement_from_code(ctx, 15, flags);
        w << "// Infinite Loop Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with LOOP_CONDITION
        generated_stmt = generate_statement_from_code(ctx, 16, flags);
        w << "// Conditional Loop Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with RETURN_TYPE
        generated_stmt = generate_statement_from_code(ctx, 17, flags);
        w << "// Return Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with DEFINE_VARIABLE
        generated_stmt = generate_statement_from_code(ctx, 18, flags);
        w << "// Variable Declaration Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 19: PROPERTY_ASSIGN (left_ref=2, right_ref=3)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::PROPERTY_ASSIGN;
        bm.code.back().left_ref(rebgn::Varint(2));   // refers to code[2] (IMMEDIATE_INT 10)
        bm.code.back().right_ref(rebgn::Varint(3));  // refers to code[3] (IMMEDIATE_INT 20)

        // Test with PROPERTY_ASSIGN
        generated_expr = generate_expression_from_code(ctx, 19, flags);
        w << "// Property Assign Test\n";
        w << renderer.render(generated_expr);
        w << "\n\n";

        // 20: ASSERT (ref=7)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::ASSERT;
        bm.code.back().ref(rebgn::Varint(7));  // refers to code[7] (IMMEDIATE_INT 1)

        // Test with ASSERT
        generated_stmt = generate_statement_from_code(ctx, 20, flags);
        w << "// Assert Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 21: LENGTH_CHECK (left_ref=2, right_ref=3)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::LENGTH_CHECK;
        bm.code.back().left_ref(rebgn::Varint(2));   // refers to code[2] (IMMEDIATE_INT 10)
        bm.code.back().right_ref(rebgn::Varint(3));  // refers to code[3] (IMMEDIATE_INT 20)

        // Test with LENGTH_CHECK
        generated_stmt = generate_statement_from_code(ctx, 21, flags);
        w << "// Length Check Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 22: EXPLICIT_ERROR (param="Error Message")
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::EXPLICIT_ERROR;
        bm.code.back().param(error_param);

        // Test with EXPLICIT_ERROR
        generated_stmt = generate_statement_from_code(ctx, 22, flags);
        w << "// Explicit Error Statement Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 23: DEFINE_PROGRAM (ident 789 - my_variable_name)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_PROGRAM;
        bm.code.back().ident(rebgn::Varint(789));

        // 24: END_PROGRAM
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_PROGRAM;

        // Test with DEFINE_PROGRAM
        generated_stmt = generate_statement_from_code(ctx, 23, flags);
        w << "// Define Program Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_PROGRAM
        generated_stmt = generate_statement_from_code(ctx, 24, flags);
        w << "// End Program Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 25: DEFINE_FORMAT (ident 456 - test_string)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_FORMAT;
        bm.code.back().ident(rebgn::Varint(456));

        // 26: END_FORMAT
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_FORMAT;

        // Test with DEFINE_FORMAT
        generated_stmt = generate_statement_from_code(ctx, 25, flags);
        w << "// Define Format Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_FORMAT
        generated_stmt = generate_statement_from_code(ctx, 26, flags);
        w << "// End Format Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 27: DEFINE_FIELD (ident 789 - my_variable_name, belong 456 - test_string, type 220 - Error Message)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_FIELD;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().belong(rebgn::Varint(456));
        rebgn::StorageRef field_type_ref;
        field_type_ref.ref = rebgn::Varint(220);
        bm.code.back().type(field_type_ref);

        // Test with DEFINE_FIELD
        generated_stmt = generate_statement_from_code(ctx, 27, flags);
        w << "// Define Field Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 28: DEFINE_PROPERTY (ident 789 - my_variable_name, belong 456 - test_string)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_PROPERTY;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().belong(rebgn::Varint(456));

        // 29: END_PROPERTY
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_PROPERTY;

        // Test with DEFINE_PROPERTY
        generated_stmt = generate_statement_from_code(ctx, 28, flags);
        w << "// Define Property Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_PROPERTY
        generated_stmt = generate_statement_from_code(ctx, 29, flags);
        w << "// End Property Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 30: DEFINE_FUNCTION (ident 789 - my_variable_name, belong 456 - test_string, func_type 0 - normal)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_FUNCTION;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().belong(rebgn::Varint(456));
        bm.code.back().func_type(rebgn::FunctionType::FREE);

        // 31: END_FUNCTION
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_FUNCTION;

        // Test with DEFINE_FUNCTION
        generated_stmt = generate_statement_from_code(ctx, 30, flags);
        w << "// Define Function Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_FUNCTION
        generated_stmt = generate_statement_from_code(ctx, 31, flags);
        w << "// End Function Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 32: DEFINE_ENUM (ident 789 - my_variable_name, type 220 - Error Message)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_ENUM;
        bm.code.back().ident(rebgn::Varint(789));
        rebgn::StorageRef enum_type_ref;
        enum_type_ref.ref = rebgn::Varint(220);
        bm.code.back().type(enum_type_ref);

        // 33: END_ENUM
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_ENUM;

        // Test with DEFINE_ENUM
        generated_stmt = generate_statement_from_code(ctx, 32, flags);
        w << "// Define Enum Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_ENUM
        generated_stmt = generate_statement_from_code(ctx, 33, flags);
        w << "// End Enum Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 34: DEFINE_ENUM_MEMBER (ident 789 - my_variable_name, left_ref 2 - 10, right_ref 3 - 20)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_ENUM_MEMBER;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().left_ref(rebgn::Varint(2));
        bm.code.back().right_ref(rebgn::Varint(3));

        // Test with DEFINE_ENUM_MEMBER
        generated_stmt = generate_statement_from_code(ctx, 34, flags);
        w << "// Define Enum Member Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 35: DEFINE_UNION (ident 789 - my_variable_name, belong 456 - test_string)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_UNION;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().belong(rebgn::Varint(456));

        // 36: END_UNION
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_UNION;

        // Test with DEFINE_UNION
        generated_stmt = generate_statement_from_code(ctx, 35, flags);
        w << "// Define Union Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_UNION
        generated_stmt = generate_statement_from_code(ctx, 36, flags);
        w << "// End Union Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 37: DEFINE_UNION_MEMBER (ident 789 - my_variable_name, belong 456 - test_string)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_UNION_MEMBER;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().belong(rebgn::Varint(456));

        // 38: END_UNION_MEMBER
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_UNION_MEMBER;

        // Test with DEFINE_UNION_MEMBER
        generated_stmt = generate_statement_from_code(ctx, 37, flags);
        w << "// Define Union Member Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_UNION_MEMBER
        generated_stmt = generate_statement_from_code(ctx, 38, flags);
        w << "// End Union Member Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 39: DEFINE_STATE (ident 789 - my_variable_name)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_STATE;
        bm.code.back().ident(rebgn::Varint(789));

        // 40: END_STATE
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_STATE;

        // Test with DEFINE_STATE
        generated_stmt = generate_statement_from_code(ctx, 39, flags);
        w << "// Define State Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_STATE
        generated_stmt = generate_statement_from_code(ctx, 40, flags);
        w << "// End State Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // 41: DEFINE_BIT_FIELD (ident 789 - my_variable_name, belong 456 - test_string, type 220 - Error Message)
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::DEFINE_BIT_FIELD;
        bm.code.back().ident(rebgn::Varint(789));
        bm.code.back().belong(rebgn::Varint(456));
        rebgn::StorageRef bit_field_type_ref;
        bit_field_type_ref.ref = rebgn::Varint(220);
        bm.code.back().type(bit_field_type_ref);

        // 42: END_BIT_FIELD
        bm.code.emplace_back();
        bm.code.back().op = rebgn::AbstractOp::END_BIT_FIELD;

        // Test with DEFINE_BIT_FIELD
        generated_stmt = generate_statement_from_code(ctx, 41, flags);
        w << "// Define Bit Field Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        // Test with END_BIT_FIELD
        generated_stmt = generate_statement_from_code(ctx, 42, flags);
        w << "// End Bit Field Test\n";
        w << renderer.render(generated_stmt);
        w << "\n\n";

        return true;
    }
}  // namespace rebgn

auto& cout = futils::wrap::cout_wrap();

int Main(Flags& flags, futils::cmdline::option::Context& ctx) {
    if (!rebgn::write_code_template(cout, flags)) {
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    Flags flags;
    return futils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, futils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}