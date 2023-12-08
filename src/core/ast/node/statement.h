/*license*/
#pragma once
#include "base.h"
#include "expr.h"
#include <vector>

namespace brgen::ast {

    struct StructType;
    struct Function;

    struct Format : Member {
        define_node_type(NodeType::format);
        std::shared_ptr<IndentBlock> body;
        std::weak_ptr<Function> encode_fn;
        std::weak_ptr<Function> decode_fn;
        std::vector<std::weak_ptr<Function>> cast_fns;
        Format(lexer::Loc l)
            : Member(l, NodeType::format) {}

        // for decode
        Format()
            : Member({}, NodeType::format) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(body);
            sdebugf(encode_fn);
            sdebugf(decode_fn);
            sdebugf(cast_fns);
        }
    };

    struct State : Member {
        define_node_type(NodeType::state);
        std::shared_ptr<IndentBlock> body;

        State(lexer::Loc l)
            : Member(l, NodeType::state) {}

        // for decode
        State()
            : Member({}, NodeType::state) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(body);
        }
    };

    struct EnumMember : Member {
        define_node_type(NodeType::enum_member);
        std::shared_ptr<Node> comment;
        std::shared_ptr<Expr> expr;

        EnumMember(lexer::Loc l)
            : Member(l, NodeType::enum_member) {}

        EnumMember()
            : Member({}, NodeType::enum_member) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(expr);
        }
    };

    struct EnumType;

    struct Enum : Member {
        define_node_type(NodeType::enum_);
        lexer::Loc colon_loc;
        scope_ptr scope;
        std::shared_ptr<Type> base_type;
        std::vector<std::shared_ptr<EnumMember>> members;
        std::shared_ptr<EnumType> enum_type;

        std::shared_ptr<EnumMember> lookup(std::string_view ident) {
            for (auto& m : members) {
                if (m->ident->ident == ident) {
                    return m;
                }
            }
            return nullptr;
        }

        Enum(lexer::Loc l)
            : Member(l, NodeType::enum_) {}

        Enum()
            : Member({}, NodeType::enum_) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(scope);
            sdebugf(colon_loc);
            sdebugf(base_type);
            sdebugf(members);
            sdebugf(enum_type);
        }
    };

    // Follow means which type of field is followed after
    enum class Follow {
        unknown,   // not analyzed or not a target
        end,       // end of struct
        fixed,     // fixed size (like [4]u8)
        constant,  // constant value (like "a")
        normal,    // otherwise
    };

    constexpr const char* follow_str[] = {
        "unknown",
        "end",
        "fixed",
        "constant",
        "normal",
        nullptr,
    };

    constexpr size_t follow_count = std::size(follow_str) - 1;

    constexpr void as_json(Follow f, auto& j) {
        j.string(follow_str[static_cast<int>(f)]);
    }

    constexpr std::optional<Follow> follow_from_str(std::string_view str) {
        for (size_t i = 0; i < follow_count; i++) {
            if (str == follow_str[i]) {
                return static_cast<Follow>(i);
            }
        }
        return std::nullopt;
    }

    struct Field : Member {
        define_node_type(NodeType::field);
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> raw_arguments;
        std::list<std::shared_ptr<Expr>> arguments;
        BitAlignment bit_alignment = BitAlignment::not_target;
        Follow follow = Follow::unknown;

        Field(lexer::Loc l)
            : Member(l, NodeType::field) {}

        // for decode
        Field()
            : Member({}, NodeType::field) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            // sdebugf(ident);
            sdebugf(colon_loc);
            sdebugf(field_type);
            sdebugf_omit(raw_arguments);
            sdebugf(arguments);
            sdebugf(bit_alignment);
            sdebugf(follow);
        }
    };

    struct StructUnionType;

    struct FunctionType;
    struct StructType;

    struct Function : Member {
        define_node_type(NodeType::function);
        std::list<std::shared_ptr<Field>> parameters;
        std::shared_ptr<Type> return_type;
        std::shared_ptr<IndentBlock> body;
        std::shared_ptr<FunctionType> func_type;
        bool is_cast = false;
        lexer::Loc cast_loc;

        Function(lexer::Loc l)
            : Member(l, NodeType::function) {}

        Function()
            : Member({}, NodeType::function) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(parameters);
            sdebugf(return_type);
            sdebugf(body);
            sdebugf(func_type);
            sdebugf(is_cast);
            sdebugf(cast_loc);
        }
    };

    struct BuiltinFunction : Member {
        define_node_type(NodeType::builtin_function);
        std::shared_ptr<FunctionType> func_type;

        BuiltinFunction(lexer::Loc l)
            : Member(l, NodeType::builtin_function) {}

        BuiltinFunction()
            : Member({}, NodeType::builtin_function) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(func_type);
        }
    };

    struct Loop : Stmt {
        define_node_type(NodeType::loop);
        scope_ptr cond_scope;
        std::shared_ptr<Expr> init;
        std::shared_ptr<Expr> cond;
        std::shared_ptr<Expr> step;
        std::shared_ptr<IndentBlock> body;

        Loop(lexer::Loc l)
            : Stmt(l, NodeType::loop) {}

        Loop()
            : Stmt({}, NodeType::loop) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(cond_scope);
            sdebugf(init);
            sdebugf(cond);
            sdebugf(step);
            sdebugf(body);
        }
    };

    struct Return : Stmt {
        define_node_type(NodeType::return_);
        std::shared_ptr<Expr> expr;

        Return(lexer::Loc l)
            : Stmt(l, NodeType::return_) {}

        Return()
            : Stmt({}, NodeType::return_) {}

        void dump(auto&& field_) {
            Stmt::dump(field_);
            sdebugf(expr);
        }
    };

    struct Break : Stmt {
        define_node_type(NodeType::break_);
        Break(lexer::Loc l)
            : Stmt(l, NodeType::break_) {}

        Break()
            : Stmt({}, NodeType::break_) {}
    };

    struct Continue : Stmt {
        define_node_type(NodeType::continue_);
        Continue(lexer::Loc l)
            : Stmt(l, NodeType::continue_) {}

        Continue()
            : Stmt({}, NodeType::continue_) {}
    };

}  // namespace brgen::ast
