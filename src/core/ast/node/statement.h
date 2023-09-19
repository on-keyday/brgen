/*license*/
#pragma once
#include "base.h"
#include "expr.h"
#include <vector>

namespace brgen::ast {

    struct Format : Stmt {
        define_node_type(NodeType::format);
        std::shared_ptr<Ident> ident;
        std::shared_ptr<IndentScope> body;
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
            field(sdebugf(body));
            field(sdebugf(belong));
        }
    };

    struct Field : Stmt {
        define_node_type(NodeType::field);
        std::shared_ptr<Ident> ident;
        lexer::Loc colon_loc;
        std::shared_ptr<Type> field_type;
        std::shared_ptr<Expr> raw_arguments;
        std::list<std::shared_ptr<Expr>> arguments;
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
            field(sdebugf(raw_arguments));
            field(sdebugf(arguments));
            field(sdebugf(belong));
        }
    };

    struct Loop : Stmt {
        define_node_type(NodeType::loop);
        std::shared_ptr<IndentScope> body;

        Loop(lexer::Loc l)
            : Stmt(l, NodeType::loop) {}

        Loop()
            : Stmt({}, NodeType::loop) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(body));
        }
    };

    struct Function : Stmt {
        define_node_type(NodeType::function);
        std::shared_ptr<Ident> ident;
        std::list<std::shared_ptr<Field>> parameters;
        std::shared_ptr<Type> return_type;
        std::weak_ptr<Format> belong;
        std::shared_ptr<IndentScope> body;

        Function(lexer::Loc l)
            : Stmt(l, NodeType::function) {}

        Function()
            : Stmt({}, NodeType::function) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field(sdebugf(ident));
            field(sdebugf(parameters));
            field(sdebugf(return_type));
            field(sdebugf(belong));
            field(sdebugf(body));
        }
    };

}  // namespace brgen::ast
