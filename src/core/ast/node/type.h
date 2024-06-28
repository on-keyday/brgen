/*license*/
#pragma once
#include "base.h"
#include "literal.h"
#include <binary/log2i.h>
#include <vector>
#include <map>
#include "statement.h"
#include <number/bit_size.h>

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

    struct FloatType : Type {
        define_node_type(NodeType::float_type);
        Endian endian = Endian::unspec;
        // if bit_size is 32, 64, it is common supported
        bool is_common_supported = false;

        FloatType(lexer::Loc l, size_t bit_size, Endian endian, bool is_explicit = false)
            : Type(l, NodeType::float_type), endian(endian) {
            this->is_explicit = is_explicit;
            this->bit_size = bit_size;
            is_common_supported = bit_size == 32 || bit_size == 64;
        }

        // for decode
        FloatType()
            : Type({}, NodeType::float_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(endian);
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
        if (!futils::number::parse_integer(str, value)) {
            return std::nullopt;
        }
        if (value == 0) {  // u0 is not valid
            return std::nullopt;
        }
        desc.bit_size = value;
        return desc;
    }

    struct FloatTypeDesc {
        size_t bit_size = 0;
        Endian endian = Endian::unspec;
    };

    inline std::optional<FloatTypeDesc> is_float_type(std::string_view str) {
        FloatTypeDesc desc;
        if (str.starts_with("fb") || str.starts_with("fl")) {
            desc.endian = str[1] == 'b' ? Endian::big : Endian::little;
            str = str.substr(2);
        }
        else if (str.starts_with("f")) {
            str = str.substr(1);
        }
        else {
            return std::nullopt;
        }
        // Check if the string starts with 'u' or 'b' and has a valid unsigned integer
        size_t value = 0;
        if (!futils::number::parse_integer(str, value)) {
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

        std::uint8_t get_aligned_bit() const {
            if (bit_size) {
                return aligned_bit(*bit_size);
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
                this->bit_size = futils::binary::log2i(*data);
                if (this->bit_size == 0) {
                    this->bit_size = 1;  // least 1 bit
                }
            }
            else {  // try arbitrary size
                std::vector<std::uint64_t> buffer;
                auto seq = futils::make_ref_seq(ty->value);
                auto radix = futils::number::has_prefix(seq);
                if (radix == 0) {
                    radix = 10;
                }
                else {
                    seq.consume(2);
                }
                size_t bit_size = 0;
                futils::number::bit_size(bit_size, buffer, seq, radix);
                this->bit_size = bit_size;
            }
        }

        // for decode
        IntLiteralType()
            : Type({}, NodeType::int_literal_type) {}
    };

    struct StrLiteralType : Type {
        define_node_type(NodeType::str_literal_type);
        std::weak_ptr<StrLiteral> base;
        std::shared_ptr<StrLiteral> strong_ref;  // only for explicit type

        StrLiteralType(std::shared_ptr<StrLiteral>&& str, bool is_explicit = false)
            : Type(str->loc, NodeType::str_literal_type) {
            this->is_explicit = is_explicit;
            base = str;
            if (is_explicit) {
                strong_ref = std::move(str);
            }
        }

        // for decode
        StrLiteralType()
            : Type({}, NodeType::str_literal_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base);
            sdebugf(strong_ref);
        }
    };

    struct RegexLiteralType : Type {
        define_node_type(NodeType::regex_literal_type);
        std::weak_ptr<RegexLiteral> base;
        std::shared_ptr<RegexLiteral> strong_ref;  // only for explicit type

        RegexLiteralType(std::shared_ptr<RegexLiteral>&& str, bool is_explicit = false)
            : Type(str->loc, NodeType::regex_literal_type) {
            this->is_explicit = is_explicit;
            base = str;
            if (is_explicit) {
                strong_ref = std::move(str);
            }
        }

        // for decode
        RegexLiteralType()
            : Type({}, NodeType::regex_literal_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base);
            sdebugf(strong_ref);
        }
    };

    struct Ident;

    struct IdentType : Type {
        define_node_type(NodeType::ident_type);
        std::shared_ptr<Ident> ident;
        // import_ref is reference to imported module
        // import_ref refers `a` of `a.b`
        std::shared_ptr<MemberAccess> import_ref;
        std::weak_ptr<Type> base;
        IdentType(lexer::Loc l, std::shared_ptr<Ident>&& token)
            : Type(l, NodeType::ident_type), ident(std::move(token)) {
            this->is_explicit = true;
        }

        IdentType(lexer::Loc l, const std::shared_ptr<Ident>& token, std::shared_ptr<Type>&& base)
            : Type(l, NodeType::ident_type), ident(token), base(std::move(base)) {
            this->is_explicit = true;
        }

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(import_ref);
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

    struct MetaType : Type {
        define_node_type(NodeType::meta_type);

        MetaType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::meta_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        MetaType()
            : Type({}, NodeType::meta_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
        }
    };

    // for argument type
    struct OptionalType : Type {
        define_node_type(NodeType::optional_type);
        define_node_description("optional type. this type is used only for argument type.");
        std::shared_ptr<Type> base_type;

        OptionalType(lexer::Loc l, std::shared_ptr<Type>&& base, bool is_explicit = false)
            : Type(l, NodeType::optional_type), base_type(std::move(base)) {
            this->is_explicit = is_explicit;
        }

        // for decode
        OptionalType()
            : Type({}, NodeType::optional_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(base_type);
        }
    };

    struct ArrayType : Type {
        define_node_type(NodeType::array_type);
        std::shared_ptr<ast::Expr> length;
        std::optional<size_t> length_value;
        lexer::Loc end_loc;
        std::shared_ptr<ast::Type> element_type;
        // if is_bytes is true, it is byte array
        bool is_bytes = false;

        ArrayType(lexer::Loc l, std::shared_ptr<ast::Expr>&& len, lexer::Loc end, std::shared_ptr<ast::Type>&& base, bool is_explicit = false)
            : Type(l, NodeType::array_type), length(std::move(len)), end_loc(end), element_type(std::move(base)) {
            this->is_explicit = is_explicit;
            if (element_type->node_type == NodeType::int_type) {
                is_bytes = element_type->bit_size == 8;
            }
        }

        // for decode
        ArrayType()
            : Type({}, NodeType::array_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(end_loc);
            sdebugf(element_type);
            sdebugf(length);
            sdebugf(length_value);
            sdebugf(is_bytes);
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
        size_t fixed_header_size = 0;
        size_t fixed_tail_size = 0;

        StructType(lexer::Loc l)
            : Type(l, NodeType::struct_type) {}

        StructType()
            : Type({}, NodeType::struct_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(fields);
            sdebugf(base);
            sdebugf(recursive);
            sdebugf(fixed_header_size);
            sdebugf(fixed_tail_size);
        }

        std::shared_ptr<Member> lookup(std::string_view key) {
            return lookup([&](auto& f) {
                return f->ident && f->ident->ident == key;
            });
        }

        std::shared_ptr<Member> lookup(auto&& cond)
            requires std::is_invocable_v<decltype(cond), std::shared_ptr<Member>&>
        {
            for (auto& f : fields) {
                if (cond(f)) {
                    return f;
                }
            }
            return nullptr;
        }
    };

    struct StructUnionType : Type {
        define_node_type(NodeType::struct_union_type);
        std::weak_ptr<Expr> base;                  // match or if expression
        std::shared_ptr<Expr> cond;                // match condition
        std::vector<std::shared_ptr<Expr>> conds;  // size must equal to structs.size()
        std::vector<std::shared_ptr<StructType>> structs;
        std::vector<std::weak_ptr<Field>> union_fields;
        // conditions are exhaustive
        // for example,
        // match u1(a):
        //    1 => data :u8
        //    0 => data :u16
        // are exhaustive
        bool exhaustive = false;

        StructUnionType(lexer::Loc l)
            : Type(l, NodeType::struct_union_type) {}

        StructUnionType()
            : Type({}, NodeType::struct_union_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(cond);
            sdebugf(conds);
            sdebugf(structs);
            sdebugf(base);
            sdebugf(union_fields);
            sdebugf(exhaustive);
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
        std::shared_ptr<Type> common_type;
        std::vector<std::shared_ptr<Field>> member_candidates;

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(cond);
            sdebugf(candidates);
            sdebugf(base_type);
            sdebugf(common_type);
            sdebugf(member_candidates);
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

    struct GenericType : Type {
        define_node_type(NodeType::generic_type);
        std::weak_ptr<Member> belong;

        GenericType(lexer::Loc l, bool is_explicit = false)
            : Type(l, NodeType::generic_type) {
            this->is_explicit = is_explicit;
        }

        // for decode
        GenericType()
            : Type({}, NodeType::generic_type) {}

        void dump(auto&& field_) {
            Type::dump(field_);
            sdebugf(belong);
        }
    };

}  // namespace brgen::ast
