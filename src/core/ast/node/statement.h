/*license*/
#pragma once
#include "base.h"
#include "expr.h"

namespace brgen::ast {

    struct Format : Stmt {
        define_node_type(NodeType::format);
        std::shared_ptr<Ident> ident;
        std::shared_ptr<IndentScope> scope;
        std::weak_ptr<Format> belong;
        Format(lexer::Loc l)
            : Stmt(l, NodeType::format) {}

        // for decode
        Format()
            : Stmt({}, NodeType::format) {}

        std::string ident_path(const char* sep = "_") {
            if (auto parent = belong.lock()) {
                return parent->ident_path() + sep + ident->ident;
            }
            return ident->ident;
        }

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(ident));
            field(sdebugf(scope));
            field(sdebugf(belong));
        }
    };

    struct Field : Stmt {
        define_node_type(NodeType::field);
        std::shared_ptr<Ident> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> arguments;
        std::weak_ptr<Format> belong;

        Field(lexer::Loc l)
            : Stmt(l, NodeType::field) {}

        // for decode
        Field()
            : Stmt({}, NodeType::field) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(ident));
            field(sdebugf(colon_loc));
            field(sdebugf(field_type));
            field(sdebugf(arguments));
            field(sdebugf(belong));
        }
    };

    struct Loop : Stmt {
        define_node_type(NodeType::loop);
        std::shared_ptr<IndentScope> block;

        Loop(lexer::Loc l)
            : Stmt(l, NodeType::loop) {}

        Loop()
            : Stmt({}, NodeType::loop) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(block));
        }
    };

    struct Function : Stmt {
        define_node_type(NodeType::function);
        std::shared_ptr<Ident> ident;
        std::vector<std::shared_ptr<Field>> arguments;
        std::weak_ptr<Format> belong;

        Function(lexer::Loc l)
            : Stmt(l, NodeType::function) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(ident));
            field(sdebugf(arguments));
            field(sdebugf(belong));
        }
    };

}  // namespace brgen::ast
