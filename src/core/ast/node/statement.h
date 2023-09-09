/*license*/
#pragma once
#include "base.h"
#include "expr.h"

namespace brgen::ast {

    struct Format : Stmt {
        define_node_type(NodeType::fmt);
        std::string ident;
        std::shared_ptr<IndentScope> scope;
        std::weak_ptr<Format> belong;
        Format(lexer::Loc l)
            : Stmt(l, NodeType::fmt) {}

        // for decode
        Format()
            : Stmt({}, NodeType::fmt) {}

        std::string ident_path(const char* sep = "_") {
            if (auto parent = belong.lock()) {
                return parent->ident_path() + sep + ident;
            }
            return ident;
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

    struct For : Stmt {
        define_node_type(NodeType::for_);
        std::shared_ptr<IndentScope> block;

        For(lexer::Loc l)
            : Stmt(l, NodeType::for_) {}

        For()
            : Stmt({}, NodeType::for_) {}

        void dump(auto&& field) {
            Stmt::dump(field);
            field("block", block);
        }
    };

}  // namespace brgen::ast
