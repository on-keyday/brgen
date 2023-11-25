/*license*/
#pragma once
#include "base.h"
#include "literal.h"
#include <binary/log2i.h>
#include <vector>
#include <map>
#include "statement.h"

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

    enum class Endian {
        unspec,
        big,
        little,

    };

    constexpr const char* endian_str[] = {
        "unspec",
        "big",
        "little",
        nullptr,
    };

    constexpr auto endian_count = std::size(endian_str) - 1;

    constexpr expected<Endian, const char*> endian_from_str(std::string_view str) {
        if (str == "big") {
            return Endian::big;
        }
        else if (str == "little") {
            return Endian::little;
        }
        else if (str == "unspec") {
            return Endian::unspec;
        }
        return unexpect("invalid endian");
    }

    constexpr void as_json(Endian e, auto&& j) {
        j.string(endian_str[static_cast<std::uint8_t>(e)]);
    }

    struct IntType : Type {
        define_node_type(NodeType::int_type);

        Endian endian = Endian::unspec;
        bool is_signed = false;
        // if bit_size is 8, 16, 32, 64, it is common supported
        bool is_common_supported = false;

        IntType(lexer::Loc l, size_t bit_size, Endian endian, bool is_signed, bool is_explicit = false)
            : Type(l, NodeType::int_type), endian(endian), is_signed(is_signed) {
            this->is_explicit = is_explicit;
            this->bit_size = bit_size;
            is_common_supported = bit_size == 8 || bit_size == 16 || bit_size == 32 || bit_size == 64;
        }

        // for decode
        IntType()
            : Type({}, NodeType::int_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(endian);
            sdebugf(is_signed);
            sdebugf(is_common_supported);
        }
    };

    struct IntTypeDesc {
        size_t bit_size = 0;
        Endian endian = Endian::unspec;
        bool is_signed = false;
    };

    inline std::optional<IntTypeDesc> is_int_type(std::string_view str) {
        IntTypeDesc desc;
        if (str.starts_with("ub") || str.starts_with("ul") ||
            str.starts_with("sb") || str.starts_with("sl")) {
            desc.endian = str[1] == 'b' ? Endian::big : Endian::little;
            desc.is_signed = str[0] == 's';
            str = str.substr(2);
        }
        else if (str.starts_with("u") || str.starts_with("s")) {
            desc.is_signed = str[0] == 's';
            str = str.substr(1);
        }
        else {
            return std::nullopt;
        }
        // Check if the string starts with 'u' or 'b' and has a valid unsigned integer
        size_t value = 0;
        if (!utils::number::parse_integer(str, value)) {
            return std::nullopt;
        }
        if (value == 0) {  // u0 is not valid
            return std::nullopt;
        }
        desc.bit_size = value;
        return desc;
    }

    struct IntLiteralType : Type {
        define_node_type(NodeType::int_literal_type);
        std::weak_ptr<IntLiteral> base;

        std::optional<std::uint8_t> get_bit_size() const {
            return bit_size;
        }

        std::uint8_t get_aligned_bit() const {
            if (auto s = get_bit_size()) {
                return aligned_bit(*s);
            }
            return 0;
        }

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base);
        }

        IntLiteralType(const std::shared_ptr<IntLiteral>& ty, bool is_explicit = false)
            : Type(ty->loc, NodeType::int_literal_type), base(ty) {
            this->is_explicit = is_explicit;
            auto data = ty->parse_as<std::uint64_t>();
            if (data) {
                this->bit_size = utils::binary::log2i(*data);
            }
        }

        // for decode
        IntLiteralType()
            : Type({}, NodeType::int_literal_type) {}
    };

    struct StrLiteralType : Type {
        define_node_type(NodeType::str_literal_type);
        std::weak_ptr<StrLiteral> base;

        StrLiteralType(std::shared_ptr<StrLiteral>&& str, bool is_explicit = false)
            : Type(str->loc, NodeType::str_literal_type), base(std::move(str)) {
            this->is_explicit = is_explicit;
        }

        // for decode
        StrLiteralType()
            : Type({}, NodeType::str_literal_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base);
        }
    };

    struct Ident;

    struct IdentType : Type {
        define_node_type(NodeType::ident_type);
        std::shared_ptr<Ident> ident;
        std::weak_ptr<Type> base;
        IdentType(lexer::Loc l, std::shared_ptr<Ident>&& token)
            : Type(l, NodeType::ident_type), ident(std::move(token)) {
            this->is_explicit = true;
        }

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(ident);
            sdebugf(base);
        }

        // for decode
        IdentType()
            : Type({}, NodeType::ident_type) {}
    };

    struct VoidType : Type {
        define_node_type(NodeType::void_type);

        constexpr VoidType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::void_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        constexpr VoidType()
            : Type({}, NodeType::void_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
        }
    };

    struct BoolType : Type {
        define_node_type(NodeType::bool_type);
        constexpr BoolType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::bool_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        constexpr BoolType()
            : Type({}, NodeType::bool_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
        }
    };

    struct ArrayType : Type {
        define_node_type(NodeType::array_type);
        std::shared_ptr<ast::Expr> length;
        lexer::Loc end_loc;
        std::shared_ptr<ast::Type> base_type;

        ArrayType(lexer::Loc l, std::shared_ptr<ast::Expr>&& len, lexer::Loc end, std::shared_ptr<ast::Type>&& base, bool is_explicit = false)
            : Type(l, NodeType::array_type), length(std::move(len)), end_loc(end), base_type(std::move(base)) {
            this->is_explicit = is_explicit;
        }

        // for decode
        ArrayType()
            : Type({}, NodeType::array_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(end_loc);
            sdebugf(base_type);
            sdebugf(length);
        }
    };

    struct FunctionType : Type {
        define_node_type(NodeType::function_type);
        std::shared_ptr<Type> return_type;
        std::vector<std::shared_ptr<Type>> parameters;

        FunctionType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::function_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        FunctionType()
            : Type({}, NodeType::function_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(return_type);
            sdebugf(parameters);
        }
    };

    struct StructType : Type {
        define_node_type(NodeType::struct_type);
        std::vector<std::shared_ptr<Member>> fields;
        std::weak_ptr<Node> base;
        bool recursive = false;

        StructType(lexer::Loc l)
            : Type(l, NodeType::struct_type) {}

        StructType()
            : Type({}, NodeType::struct_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(fields);
            sdebugf(base);
            sdebugf(recursive);
        }

        std::shared_ptr<Member> lookup(std::string_view key) {
            for (auto& f : fields) {
                if (f->ident && f->ident->ident == key) {
                    return f;
                }
            }
            return nullptr;
        }
    };

    struct StructUnionType : Type {
        define_node_type(NodeType::struct_union_type);
        std::weak_ptr<Expr> base;
        std::vector<std::shared_ptr<StructType>> fields;
        std::vector<std::weak_ptr<Field>> union_fields;

        StructUnionType(lexer::Loc l)
            : Type(l, NodeType::struct_union_type) {}

        StructUnionType()
            : Type({}, NodeType::struct_union_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(fields);
            sdebugf(base);
            sdebugf(union_fields);
        }
    };

    struct UnionCandidate : Stmt {
        define_node_type(NodeType::union_candidate);
        std::weak_ptr<Expr> cond;
        std::weak_ptr<Field> field;

        UnionCandidate(lexer::Loc loc)
            : Stmt(loc, NodeType::union_candidate) {}

        UnionCandidate()
            : Stmt({}, NodeType::union_candidate) {}

        void dump(auto&& field_) {
            Node::dump(field_);
            sdebugf(cond);
            sdebugf(field);
        }
    };

    struct UnionType : Type {
        define_node_type(NodeType::union_type);
        std::weak_ptr<Expr> cond;
        std::vector<std::shared_ptr<UnionCandidate>> candidates;
        std::weak_ptr<StructUnionType> base_type;

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(cond);
            sdebugf(candidates);
            sdebugf(base_type);
        }

        UnionType(lexer::Loc loc)
            : Type(loc, NodeType::union_type) {}

        UnionType()
            : Type({}, NodeType::union_type) {}
    };

    struct RangeType : Type {
        define_node_type(NodeType::range_type);
        std::shared_ptr<Type> base_type;
        std::weak_ptr<Range> range;

        RangeType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::range_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        RangeType()
            : Type({}, NodeType::range_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base_type);
            sdebugf(range);
        }
    };

    struct EnumType : Type {
        define_node_type(NodeType::enum_type);
        std::weak_ptr<Enum> base;

        EnumType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::enum_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        EnumType()
            : Type({}, NodeType::enum_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base);
        }
    };

}  // namespace brgen::ast
