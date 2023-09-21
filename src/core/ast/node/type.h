/*license*/
#pragma once
#include "base.h"
#include "literal.h"
#include <binary/log2i.h>
#include <vector>
#include <map>

namespace brgen::ast {
    // types

    constexpr std::uint8_t aligned_bit(size_t bit) {
        if (bit <= 8) {
            return 8;
        }
        else if (bit <= 16) {
            return 16;
        }
        else if (bit <= 32) {
            return 32;
        }
        else if (bit <= 64) {
            return 64;
        }
        return 0;
    }

    struct IntType : Type {
        define_node_type(NodeType::int_type);
        std::string raw;
        size_t bit_size = 0;

        IntType(lexer::Loc l, std::string&& token, size_t bit_size)
            : Type(l, NodeType::int_type), raw(std::move(token)), bit_size(bit_size) {}

        // for decode
        IntType()
            : Type({}, NodeType::int_type) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(raw));
            field(sdebugf(bit_size));
        }
    };

    struct IntLiteralType : Type {
        define_node_type(NodeType::int_literal_type);
        std::weak_ptr<IntLiteral> base;
        mutable std::optional<std::uint8_t> bit_size;

        std::optional<std::uint8_t> get_bit_size() const {
            if (bit_size) {
                return bit_size;
            }
            auto got = base.lock();
            if (!got) {
                return std::nullopt;
            }
            auto p = got->parse_as<std::uint64_t>();
            if (!p) {
                return std::nullopt;
            }
            bit_size = utils::binary::log2i(*p);
            return bit_size;
        }

        std::uint8_t get_aligned_bit() const {
            if (auto s = get_bit_size()) {
                return aligned_bit(*s);
            }
            return 0;
        }

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(base));
        }

        IntLiteralType(const std::shared_ptr<IntLiteral>& ty)
            : Type(ty->loc, NodeType::int_literal_type), base(ty) {}

        // for decode
        IntLiteralType()
            : Type({}, NodeType::int_literal_type) {}
    };

    struct StrLiteralType : Type {
        define_node_type(NodeType::str_literal_type);
        std::weak_ptr<StrLiteral> base;

        StrLiteralType(std::shared_ptr<StrLiteral>&& str)
            : Type(str->loc, NodeType::str_literal_type), base(std::move(str)) {}

        // for decode
        StrLiteralType()
            : Type({}, NodeType::str_literal_type) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(base));
        }
    };

    struct Format;
    struct Ident;

    struct IdentType : Type {
        define_node_type(NodeType::ident_type);
        std::shared_ptr<Ident> ident;
        std::weak_ptr<Format> base;
        IdentType(lexer::Loc l, std::shared_ptr<Ident>&& token)
            : Type(l, NodeType::ident_type), ident(std::move(token)) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(ident));
            field(sdebugf(base));
        }

        // for decode
        IdentType()
            : Type({}, NodeType::ident_type) {}
    };

    struct VoidType : Type {
        define_node_type(NodeType::void_type);

        constexpr VoidType(lexer::Loc l)
            : Type(l, NodeType::void_type) {}

        // for decode
        constexpr VoidType()
            : Type({}, NodeType::void_type) {}

        void dump(auto&& field) {
            Type::dump(field);
        }
    };

    struct BoolType : Type {
        define_node_type(NodeType::bool_type);

        constexpr BoolType(lexer::Loc l)
            : Type(l, NodeType::bool_type) {}

        // for decode
        constexpr BoolType()
            : Type({}, NodeType::bool_type) {}

        void dump(auto&& field) {
            Type::dump(field);
        }
    };

    struct ArrayType : Type {
        define_node_type(NodeType::array_type);
        std::shared_ptr<ast::Expr> length;
        lexer::Loc end_loc;
        std::shared_ptr<ast::Type> base_type;

        ArrayType(lexer::Loc l, std::shared_ptr<ast::Expr>&& len, lexer::Loc end, std::shared_ptr<ast::Type>&& base)
            : Type(l, NodeType::array_type), length(std::move(len)), end_loc(end), base_type(std::move(base)) {}

        // for decode
        ArrayType()
            : Type({}, NodeType::array_type) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(end_loc));
            field(sdebugf(base_type));
            field(sdebugf(length));
        }
    };

    struct FunctionType : Type {
        define_node_type(NodeType::function_type);
        std::shared_ptr<Type> return_type;
        std::vector<std::shared_ptr<Type>> parameters;

        FunctionType(lexer::Loc l, std::shared_ptr<Type>&& ret, std::vector<std::shared_ptr<Type>>&& params)
            : Type(l, NodeType::function_type), return_type(std::move(ret)), parameters(std::move(params)) {}

        // for decode
        FunctionType()
            : Type({}, NodeType::function_type) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(return_type));
            field(sdebugf(parameters));
        }
    };

    struct MapType : Type {
        define_node_type(NodeType::map_type);
        std::map<std::string, std::shared_ptr<Node>> fields;

        MapType(lexer::Loc l, std::map<std::string, std::shared_ptr<Node>>&& fields)
            : Type(l, NodeType::map_type), fields(std::move(fields)) {}

        void dump(auto&& field) {
            Type::dump(field);
            field(sdebugf(fields));
        }
    };

}  // namespace brgen::ast
