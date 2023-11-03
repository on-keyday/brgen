/*license*/
#pragma once
#include "base.h"
#include "expr.h"
#include <vector>

namespace brgen::ast {

    struct StructType;

    struct Format : Member {
        define_node_type(NodeType::format);
        bool is_enum = false;
        // std::shared_ptr<Ident> ident;
        std::shared_ptr<IndentBlock> body;
        // std::shared_ptr<StructType> struct_type;
        Format(lexer::Loc l, bool is_enum)
            : Member(l, NodeType::format), is_enum(is_enum) {}

        // for decode
        Format()
            : Member({}, NodeType::format) {}

        void dump(auto&& field_) {
            Member::dump(field_);
            sdebugf(is_enum);
            // sdebugf(ident);
            sdebugf(body);
            // sdebugf(struct_type);
        }
    };

    struct Field : Member {
        define_node_type(NodeType::field);
        // std::shared_ptr<Ident> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> raw_arguments;
        std::list<std::shared_ptr<Expr>> arguments;

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
