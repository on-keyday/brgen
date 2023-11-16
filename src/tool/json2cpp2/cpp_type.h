
#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <core/ast/ast.h>

namespace j2cp2 {
    namespace ast = brgen::ast;
    enum class BuiltinType {
        uint8,
        uint16,
        uint32,
        uint64,
        uint128,
    };

    enum class TypeKind {
        Builtin,
        BitFlags,
        Bytes,
        Vector,
        BuiltinArray,
        Struct,
    };

    struct Type {
        const TypeKind kind;

       protected:
        Type(TypeKind kind)
            : kind(kind) {}
    };

    struct Integer : public Type {
        BuiltinType base_type;
        bool is_signed;

        Integer()
            : Type(TypeKind::Builtin) {}
    };

    struct Bit {
        std::uint8_t begin_from;
        std::uint8_t end_at;
    };

    struct BitFlags : public Type {
        BuiltinType base_type;
        std::vector<Bit> bits;

        BitFlags()
            : Type(TypeKind::BitFlags) {}
    };

    struct Bytes : Type {
        Bytes()
            : Type(TypeKind::Bytes) {}
    };

    struct Vector : Type {
        std::shared_ptr<Type> element_type;

        Vector()
            : Type(TypeKind::Vector) {}
    };

    struct BuiltinArray : Type {
        BuiltinType element_type;
        std::uint32_t size;

        BuiltinArray()
            : Type(TypeKind::BuiltinArray) {}
    };

    struct Field {
        std::string name;
        std::shared_ptr<Type> type;
        std::shared_ptr<ast::Field> base;
    };

    struct Struct : Type {
        std::vector<std::shared_ptr<Field>> fields;
        std::shared_ptr<ast::StructType> base;

        Struct()
            : Type(TypeKind::Struct) {}
    };

    struct Union : Type {
        std::vector<std::shared_ptr<Struct>> fields;
        std::shared_ptr<ast::StructUnionType> base;

        Union()
            : Type(TypeKind::Struct) {}
    };

}  // namespace j2cp2
