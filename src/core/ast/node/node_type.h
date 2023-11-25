/*license*/
#pragma once
#include <core/common/expected.h>
#include <string_view>
#include <algorithm>
#include <array>

namespace brgen::ast {
    enum class NodeType {
        program,
        comment,
        comment_group,

        expr = 0x010000,

        binary,
        unary,
        cond,
        ident,
        call,
        if_,
        member_access,
        paren,
        index,
        match,
        range,

        // translated
        tmp_var,
        block_expr,
        import_,
        cast,

        literal = 0x110000,
        int_literal,
        bool_literal,
        str_literal,

        input,
        output,
        config,

        stmt = 0x020000,
        loop,
        indent_block,
        match_branch,
        union_candidate,
        return_,
        break_,
        continue_,

        // translated
        assert,
        implicit_yield,

        member = 0x220000,

        field,

        format,
        state,
        enum_,
        enum_member,
        function,
        builtin_function,

        type = 0x040000,
        int_type,
        ident_type,
        int_literal_type,
        str_literal_type,
        void_type,
        bool_type,
        array_type,
        function_type,
        struct_type,
        struct_union_type,
        union_type,
        range_type,
        enum_type,
        bit_group_type,
    };

    /*
    constexpr const char* node_type_str[]{
        "program",
        "expr",
        "binary",
        "unary",
        "cond",
        "ident",
        "call",
        "if",
        "member_access",
        "paren",
        "index",
        "match",
        "range",
        "tmp_var",
        "block_expr",
        "import",
        "literal",
        "int_literal",
        "bool_literal",
        "str_literal",
        "input",
        "output",
        "config",
        "stmt",
        "loop",
        "indent_block",
        "match_branch",
        "return",
        "break",
        "continue",
        "assert",
        "implicit_yield",
        "member",
        "field",
        "format",
        "function",
        "type",
        "int_type",
        "ident_type",
        "int_literal_type",
        "str_literal_type",
        "void_type",
        "bool_type",
        "array_type",
        "function_type",
        "struct_type",
        "struct_union_type",
        "cast",
        "comment",
        "comment_group",
        "union_type",
        "union_candidate",
        "range_type",
        "enum",
        "enum_member",
        "enum_type",
        "bit_group_type",
        "state",
        "builtin_function",
    };

    constexpr NodeType node_type_array[] = {
        NodeType::program,
        NodeType::expr,
        NodeType::binary,
        NodeType::unary,
        NodeType::cond,
        NodeType::ident,
        NodeType::call,
        NodeType::if_,
        NodeType::member_access,
        NodeType::paren,
        NodeType::index,
        NodeType::match,
        NodeType::range,
        NodeType::tmp_var,
        NodeType::block_expr,
        NodeType::import_,
        NodeType::literal,
        NodeType::int_literal,
        NodeType::bool_literal,
        NodeType::str_literal,
        NodeType::input,
        NodeType::output,
        NodeType::config,
        NodeType::stmt,
        NodeType::loop,
        NodeType::indent_block,
        NodeType::match_branch,
        NodeType::return_,
        NodeType::break_,
        NodeType::continue_,
        NodeType::assert,
        NodeType::implicit_yield,
        NodeType::member,
        NodeType::field,
        NodeType::format,
        NodeType::function,
        NodeType::type,
        NodeType::int_type,
        NodeType::ident_type,
        NodeType::int_literal_type,
        NodeType::str_literal_type,
        NodeType::void_type,
        NodeType::bool_type,
        NodeType::array_type,
        NodeType::function_type,
        NodeType::struct_type,
        NodeType::struct_union_type,
        NodeType::cast,
        NodeType::comment,
        NodeType::comment_group,
        NodeType::union_type,
        NodeType::union_candidate,
        NodeType::range_type,
        NodeType::enum_,
        NodeType::enum_member,
        NodeType::enum_type,
        NodeType::bit_group_type,
        NodeType::state,
        NodeType::builtin_function,
    };
    */

    constexpr std::pair<NodeType, const char*> node_type_str_array[] = {
        {NodeType::program, "program"},
        {NodeType::expr, "expr"},
        {NodeType::binary, "binary"},
        {NodeType::unary, "unary"},
        {NodeType::cond, "cond"},
        {NodeType::ident, "ident"},
        {NodeType::call, "call"},
        {NodeType::if_, "if"},
        {NodeType::member_access, "member_access"},
        {NodeType::paren, "paren"},
        {NodeType::index, "index"},
        {NodeType::match, "match"},
        {NodeType::range, "range"},
        {NodeType::tmp_var, "tmp_var"},
        {NodeType::block_expr, "block_expr"},
        {NodeType::import_, "import"},
        {NodeType::literal, "literal"},
        {NodeType::int_literal, "int_literal"},
        {NodeType::bool_literal, "bool_literal"},
        {NodeType::str_literal, "str_literal"},
        {NodeType::input, "input"},
        {NodeType::output, "output"},
        {NodeType::config, "config"},
        {NodeType::stmt, "stmt"},
        {NodeType::loop, "loop"},
        {NodeType::indent_block, "indent_block"},
        {NodeType::match_branch, "match_branch"},
        {NodeType::return_, "return"},
        {NodeType::break_, "break"},
        {NodeType::continue_, "continue"},
        {NodeType::assert, "assert"},
        {NodeType::implicit_yield, "implicit_yield"},
        {NodeType::member, "member"},
        {NodeType::field, "field"},
        {NodeType::format, "format"},
        {NodeType::function, "function"},
        {NodeType::type, "type"},
        {NodeType::int_type, "int_type"},
        {NodeType::ident_type, "ident_type"},
        {NodeType::int_literal_type, "int_literal_type"},
        {NodeType::str_literal_type, "str_literal_type"},
        {NodeType::void_type, "void_type"},
        {NodeType::bool_type, "bool_type"},
        {NodeType::array_type, "array_type"},
        {NodeType::function_type, "function_type"},
        {NodeType::struct_type, "struct_type"},
        {NodeType::struct_union_type, "struct_union_type"},
        {NodeType::cast, "cast"},
        {NodeType::comment, "comment"},
        {NodeType::comment_group, "comment_group"},
        {NodeType::union_type, "union_type"},
        {NodeType::union_candidate, "union_candidate"},
        {NodeType::range_type, "range_type"},
        {NodeType::enum_, "enum"},
        {NodeType::enum_member, "enum_member"},
        {NodeType::enum_type, "enum_type"},
        {NodeType::bit_group_type, "bit_group_type"},
        {NodeType::state, "state"},
        {NodeType::builtin_function, "builtin_function"},
    };

    constexpr std::array<std::pair<NodeType, const char*>, std::size(node_type_str_array)> sorted_node_type_str_array = [] {
        std::array<std::pair<NodeType, const char*>, std::size(node_type_str_array)> a;
        for (int i = 0; i < std::size(node_type_str_array); i++) {
            a[i] = node_type_str_array[i];
        }
        std::sort(a.begin(), a.end(), [](auto a, auto b) { return static_cast<int>(a.first) < static_cast<int>(b.first); });
        return a;
    }();

    constexpr int mapNodeTypeToValue_2(NodeType type) {
        auto low = std::lower_bound(sorted_node_type_str_array.begin(), sorted_node_type_str_array.end(), type, [](auto a, auto b) { return static_cast<int>(a.first) < static_cast<int>(b); });
        if (low == sorted_node_type_str_array.end() || low->first != type) {
            return -1;
        }
        return static_cast<int>(low - sorted_node_type_str_array.begin());
    }

    constexpr either::expected<NodeType, const char*> mapValueToNodeType_2(int value) {
        if (value < 0 || value >= std::size(node_type_str_array)) {
            return either::unexpected{"invalid value"};
        }
        return sorted_node_type_str_array[value].first;
    }

    /*
    constexpr int mapNodeTypeToValue(NodeType type) {
        switch (type) {
            case NodeType::program:
                return 0;
            case NodeType::expr:
                return 1;
            case NodeType::binary:
                return 2;
            case NodeType::unary:
                return 3;
            case NodeType::cond:
                return 4;
            case NodeType::ident:
                return 5;
            case NodeType::call:
                return 6;
            case NodeType::if_:
                return 7;
            case NodeType::member_access:
                return 8;
            case NodeType::paren:
                return 9;
            case NodeType::index:
                return 10;
            case NodeType::match:
                return 11;
            case NodeType::range:
                return 12;
            case NodeType::tmp_var:
                return 13;
            case NodeType::block_expr:
                return 14;
            case NodeType::import_:
                return 15;
            case NodeType::literal:
                return 16;
            case NodeType::int_literal:
                return 17;
            case NodeType::bool_literal:
                return 18;
            case NodeType::str_literal:
                return 19;
            case NodeType::input:
                return 20;
            case NodeType::output:
                return 21;
            case NodeType::config:
                return 22;
            case NodeType::stmt:
                return 23;
            case NodeType::loop:
                return 24;
            case NodeType::indent_block:
                return 25;
            case NodeType::match_branch:
                return 26;
            case NodeType::return_:
                return 27;
            case NodeType::break_:
                return 28;
            case NodeType::continue_:
                return 29;
            case NodeType::assert:
                return 30;
            case NodeType::implicit_yield:
                return 31;
            case NodeType::member:
                return 32;
            case NodeType::field:
                return 33;
            case NodeType::format:
                return 34;
            case NodeType::function:
                return 35;
            case NodeType::type:
                return 36;
            case NodeType::int_type:
                return 37;
            case NodeType::ident_type:
                return 38;
            case NodeType::int_literal_type:
                return 39;
            case NodeType::str_literal_type:
                return 40;
            case NodeType::void_type:
                return 41;
            case NodeType::bool_type:
                return 42;
            case NodeType::array_type:
                return 43;
            case NodeType::function_type:
                return 44;
            case NodeType::struct_type:
                return 45;
            case NodeType::struct_union_type:
                return 46;
            case NodeType::cast:
                return 47;
            case NodeType::comment:
                return 48;
            case NodeType::comment_group:
                return 49;
            case NodeType::union_type:
                return 50;
            case NodeType::union_candidate:
                return 51;
            case NodeType::range_type:
                return 52;
            case NodeType::enum_:
                return 53;
            case NodeType::enum_member:
                return 54;
            case NodeType::enum_type:
                return 55;
            case NodeType::bit_group_type:
                return 56;
            case NodeType::state:
                return 57;
            case NodeType::builtin_function:
                return 58;
            default:
                return -1;
        }
    }

    constexpr either::expected<NodeType, const char*> mapValueToNodeType(int value) {
        switch (value) {
            case 0:
                return NodeType::program;
            case 1:
                return NodeType::expr;
            case 2:
                return NodeType::binary;
            case 3:
                return NodeType::unary;
            case 4:
                return NodeType::cond;
            case 5:
                return NodeType::ident;
            case 6:
                return NodeType::call;
            case 7:
                return NodeType::if_;
            case 8:
                return NodeType::member_access;
            case 9:
                return NodeType::paren;
            case 10:
                return NodeType::index;
            case 11:
                return NodeType::match;
            case 12:
                return NodeType::range;
            case 13:
                return NodeType::tmp_var;
            case 14:
                return NodeType::block_expr;
            case 15:
                return NodeType::import_;
            case 16:
                return NodeType::literal;
            case 17:
                return NodeType::int_literal;
            case 18:
                return NodeType::bool_literal;
            case 19:
                return NodeType::str_literal;
            case 20:
                return NodeType::input;
            case 21:
                return NodeType::output;
            case 22:
                return NodeType::config;
            case 23:
                return NodeType::stmt;
            case 24:
                return NodeType::loop;
            case 25:
                return NodeType::indent_block;
            case 26:
                return NodeType::match_branch;
            case 27:
                return NodeType::return_;
            case 28:
                return NodeType::break_;
            case 29:
                return NodeType::continue_;
            case 30:
                return NodeType::assert;
            case 31:
                return NodeType::implicit_yield;
            case 32:
                return NodeType::member;
            case 33:
                return NodeType::field;
            case 34:
                return NodeType::format;
            case 35:
                return NodeType::function;
            case 36:
                return NodeType::type;
            case 37:
                return NodeType::int_type;
            case 38:
                return NodeType::ident_type;
            case 39:
                return NodeType::int_literal_type;
            case 40:
                return NodeType::str_literal_type;
            case 41:
                return NodeType::void_type;
            case 42:
                return NodeType::bool_type;
            case 43:
                return NodeType::array_type;
            case 44:
                return NodeType::function_type;
            case 45:
                return NodeType::struct_type;
            case 46:
                return NodeType::struct_union_type;
            case 47:
                return NodeType::cast;
            case 48:
                return NodeType::comment;
            case 49:
                return NodeType::comment_group;
            case 50:
                return NodeType::union_type;
            case 51:
                return NodeType::union_candidate;
            case 52:
                return NodeType::range_type;
            case 53:
                return NodeType::enum_;
            case 54:
                return NodeType::enum_member;
            case 55:
                return NodeType::enum_type;
            case 56:
                return NodeType::bit_group_type;
            case 57:
                return NodeType::state;
            case 58:
                return NodeType::builtin_function;
            default:
                return either::unexpected{"invalid value"};
        }
    }
    */

    constexpr const char* node_type_to_string(NodeType type) {
        auto key = mapNodeTypeToValue_2(type);
        if (key == -1) {
            return "unknown";
        }
        return sorted_node_type_str_array[key].second;
    }

    constexpr auto node_type_count = std::size(node_type_str_array);

    constexpr either::expected<NodeType, const char*> string_to_node_type(std::string_view key) {
        for (int i = 0; i < node_type_count; i++) {
            if (key == node_type_str_array[i].second) {
                return mapValueToNodeType_2(i);
            }
        }
        return either::unexpected{key.data()};
    }

    namespace internal {
        constexpr auto check_func() {
            for (int i = 0; i < node_type_count; i++) {
                auto v = mapNodeTypeToValue_2(mapValueToNodeType_2(i).value());
                if (v != i) {
                    [](auto... a) { throw "not matched"; }(v, i);
                }
                v = mapNodeTypeToValue_2(string_to_node_type(node_type_str_array[i].second).value());
                if (v != i) {
                    [](auto... a) { throw "not matched"; }(v, i);
                }
            }
            return true;
        }
        static_assert(check_func());
    }  // namespace internal

}  // namespace brgen::ast
