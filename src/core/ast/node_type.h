/*license*/
#pragma once
#include "../common/expected.h"

namespace brgen::ast {
    enum class NodeType {
        program,
        expr = 0x010000,
        int_literal,
        bool_literal,
        str_literal,

        input,
        output,

        binary,
        unary,
        cond,
        ident,
        call,
        if_,
        member_access,
        paren,

        // translated
        tmp_var,
        block_expr,

        stmt = 0x020000,
        for_,
        field,
        fmt,
        indent_scope,

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
        "if_",
        "member_access",
        "paren",
        "tmp_var",
        "block_expr",
        "stmt",
        "for_",
        "field",
        "fmt",
        "indent_scope",
        "assert",
        "implicit_return",
        "type",
        "int_type",
        "ident_type",
        "int_literal_type",
        "str_literal_type",
        "void_type",
        "bool_type",
        "array_type",
        "str_literal",
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
            case NodeType::for_:
                return 17;
            case NodeType::field:
                return 18;
            case NodeType::fmt:
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
                return NodeType::for_;
            case 18:
                return NodeType::field;
            case 19:
                return NodeType::fmt;
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