/*license*/
#pragma once
#include <core/common/expected.h>
#include <string_view>

namespace brgen::ast {
    enum class NodeType {
        program,
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

        literal = 0x090000,
        int_literal,
        bool_literal,
        str_literal,

        input,
        output,
        config,

        stmt = 0x020000,
        loop,
        field,
        format,
        indent_scope,
        function,
        match_branch,

        // translated
        assert,
        implicit_return,

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
        union_type,

    };

    constexpr const char* node_type_str[]{
        "program",
        "expr",
        "int_literal",
        "bool_literal",
        "input",
        "output",
        "binary",
        "unary",
        "cond",
        "ident",
        "call",
        "if",
        "member_access",
        "paren",
        "tmp_var",
        "block_expr",
        "stmt",
        "loop",
        "field",
        "format",
        "indent_scope",
        "assert",
        "implicit_return",
        "type",
        "int_type",
        "ident_type",
        "int_literal_type",
        "string_literal_type",
        "void_type",
        "bool_type",
        "array_type",
        "string_literal",
        "function",
        "index",
        "match",
        "match_branch",
        "config",
        "range",
        "import",
        "function_type",
        "struct_type",
        "union_type",
    };

    constexpr int mapNodeTypeToValue(NodeType type) {
        switch (type) {
            case NodeType::program:
                return 0;
            case NodeType::expr:
                return 1;
            case NodeType::int_literal:
                return 2;
            case NodeType::bool_literal:
                return 3;
            case NodeType::input:
                return 4;
            case NodeType::output:
                return 5;
            case NodeType::binary:
                return 6;
            case NodeType::unary:
                return 7;
            case NodeType::cond:
                return 8;
            case NodeType::ident:
                return 9;
            case NodeType::call:
                return 10;
            case NodeType::if_:
                return 11;
            case NodeType::member_access:
                return 12;
            case NodeType::paren:
                return 13;
            case NodeType::tmp_var:
                return 14;
            case NodeType::block_expr:
                return 15;
            case NodeType::stmt:
                return 16;
            case NodeType::loop:
                return 17;
            case NodeType::field:
                return 18;
            case NodeType::format:
                return 19;
            case NodeType::indent_scope:
                return 20;
            case NodeType::assert:
                return 21;
            case NodeType::implicit_return:
                return 22;
            case NodeType::type:
                return 23;
            case NodeType::int_type:
                return 24;
            case NodeType::ident_type:
                return 25;
            case NodeType::int_literal_type:
                return 26;
            case NodeType::str_literal_type:
                return 27;
            case NodeType::void_type:
                return 28;
            case NodeType::bool_type:
                return 29;
            case NodeType::array_type:
                return 30;
            case NodeType::str_literal:
                return 31;
            case NodeType::function:
                return 32;
            case NodeType::index:
                return 33;
            case NodeType::match:
                return 34;
            case NodeType::match_branch:
                return 35;
            case NodeType::config:
                return 36;
            case NodeType::range:
                return 37;
            case NodeType::import_:
                return 38;
            case NodeType::function_type:
                return 39;
            case NodeType::struct_type:
                return 40;
            case NodeType::union_type:
                return 41;
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
                return NodeType::int_literal;
            case 3:
                return NodeType::bool_literal;
            case 4:
                return NodeType::input;
            case 5:
                return NodeType::output;
            case 6:
                return NodeType::binary;
            case 7:
                return NodeType::unary;
            case 8:
                return NodeType::cond;
            case 9:
                return NodeType::ident;
            case 10:
                return NodeType::call;
            case 11:
                return NodeType::if_;
            case 12:
                return NodeType::member_access;
            case 13:
                return NodeType::paren;
            case 14:
                return NodeType::tmp_var;
            case 15:
                return NodeType::block_expr;
            case 16:
                return NodeType::stmt;
            case 17:
                return NodeType::loop;
            case 18:
                return NodeType::field;
            case 19:
                return NodeType::format;
            case 20:
                return NodeType::indent_scope;
            case 21:
                return NodeType::assert;
            case 22:
                return NodeType::implicit_return;
            case 23:
                return NodeType::type;
            case 24:
                return NodeType::int_type;
            case 25:
                return NodeType::ident_type;
            case 26:
                return NodeType::int_literal_type;
            case 27:
                return NodeType::str_literal_type;
            case 28:
                return NodeType::void_type;
            case 29:
                return NodeType::bool_type;
            case 30:
                return NodeType::array_type;
            case 31:
                return NodeType::str_literal;
            case 32:
                return NodeType::function;
            case 33:
                return NodeType::index;
            case 34:
                return NodeType::match;
            case 35:
                return NodeType::match_branch;
            case 36:
                return NodeType::config;
            case 37:
                return NodeType::range;
            case 38:
                return NodeType::import_;
            case 39:
                return NodeType::function_type;
            case 40:
                return NodeType::struct_type;
            case 41:
                return NodeType::union_type;
            default:
                return either::unexpected{"invalid value"};
        }
    }

    constexpr const char* node_type_to_string(NodeType type) {
        auto key = mapNodeTypeToValue(type);
        if (key == -1) {
            return "unknown";
        }
        return node_type_str[key];
    }

    constexpr either::expected<NodeType, const char*> string_to_node_type(std::string_view key) {
        constexpr auto max_value = std::size(node_type_str);
        for (int i = 0; i < max_value; i++) {
            if (key == node_type_str[i]) {
                return mapValueToNodeType(i);
            }
        }
        return either::unexpected{"not a node type"};
    }

    namespace internal {
        constexpr auto check_func() {
            constexpr auto max_value = std::size(node_type_str);
            for (int i = 0; i < max_value; i++) {
                auto v = mapNodeTypeToValue(mapValueToNodeType(i).value());
                if (v != i) {
                    [](auto... a) { throw "not matched"; }(v, i);
                }
                v = mapNodeTypeToValue(string_to_node_type(node_type_str[i]).value());
                if (v != i) {
                    [](auto... a) { throw "not matched"; }(v, i);
                }
            }
            return true;
        }
        static_assert(check_func());
    }  // namespace internal

}  // namespace brgen::ast
