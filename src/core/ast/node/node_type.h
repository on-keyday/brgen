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
        field_argument,

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
        import_,
        cast,
        available,
        specify_endian,
        explicit_error,
        io_operation,

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
        scoped_statement,
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

        builtin_member = 0x222000,
        builtin_function,
        builtin_field,
        builtin_object,

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
    };

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
        {NodeType::state, "state"},
        {NodeType::builtin_function, "builtin_function"},
        {NodeType::available, "available"},
        {NodeType::scoped_statement, "scoped_statement"},
        {NodeType::field_argument, "field_argument"},
        {NodeType::specify_endian, "specify_endian"},
        {NodeType::explicit_error, "explicit_error"},
        {NodeType::io_operation, "io_operation"},
        {NodeType::builtin_member, "builtin_member"},
        {NodeType::builtin_field, "builtin_field"},
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
            if (key == sorted_node_type_str_array[i].second) {
                return sorted_node_type_str_array[i].first;
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
                v = mapNodeTypeToValue_2(string_to_node_type(sorted_node_type_str_array[i].second).value());
                if (v != i) {
                    [](auto... a) { throw "not matched"; }(v, i);
                }
            }
            return true;
        }
        static_assert(check_func());
    }  // namespace internal

}  // namespace brgen::ast
