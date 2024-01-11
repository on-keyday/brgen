/*license*/
#pragma once
#include "statement.h"

namespace brgen::ast {

    struct BuiltinMember : Member {
        define_node_type(NodeType::builtin_member);

       protected:
        BuiltinMember(lexer::Loc l, NodeType nt)
            : Member(l, nt) {}
    };

    struct BuiltinFunction : BuiltinMember {
        define_node_type(NodeType::builtin_function);
        std::shared_ptr<FunctionType> func_type;

        BuiltinFunction(lexer::Loc l)
            : BuiltinMember(l, NodeType::builtin_function) {}

        BuiltinFunction()
            : BuiltinMember({}, NodeType::builtin_function) {}

        void dump(auto&& field_) {
            BuiltinMember::dump(field_);
            sdebugf(func_type);
        }
    };

    struct BuiltinField : BuiltinMember {
        define_node_type(NodeType::builtin_field);
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> default_value;
        bool assignable = false;

        BuiltinField(lexer::Loc l)
            : BuiltinMember(l, NodeType::builtin_field) {}

        BuiltinField()
            : BuiltinMember({}, NodeType::builtin_field) {}

        void dump(auto&& field_) {
            BuiltinMember::dump(field_);
            sdebugf(field_type);
        }
    };

    struct BuiltinObject : BuiltinMember {
        define_node_type(NodeType::builtin_object);
        // Builtin Field, Function, or Object
        std::vector<std::shared_ptr<BuiltinMember>> members;
        std::shared_ptr<StructType> struct_type;

        BuiltinObject(lexer::Loc l)
            : BuiltinMember(l, NodeType::builtin_object) {}

        BuiltinObject()
            : BuiltinMember({}, NodeType::builtin_object) {}

        void dump(auto&& field_) {
            BuiltinMember::dump(field_);
            sdebugf(members);
        }
    };

}  // namespace brgen::ast
