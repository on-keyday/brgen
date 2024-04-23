/*license*/

#include <gtest/gtest.h>
#include <core/ast/ast.h>

using namespace brgen;

TEST(DeriveTest, Test1) {
    struct Dummy : ast::Member {
        Dummy()
            : ast::Member({}, ast::NodeType::member) {}
    };
    Dummy d;
    ASSERT_NE(ast::as<ast::Node>(&d), nullptr);
    ASSERT_NE(ast::as<ast::Stmt>(&d), nullptr);
    ASSERT_NE(ast::as<ast::Member>(&d), nullptr);
    

    struct Dummy2 : ast::Literal {
        Dummy2()
            : ast::Literal({}, ast::NodeType::literal) {}
    };

    Dummy2 d2;
    ASSERT_NE(ast::as<ast::Node>(&d2), nullptr);
    ASSERT_NE(ast::as<ast::Expr>(&d2), nullptr);
    ASSERT_NE(ast::as<ast::Literal>(&d2), nullptr);
}
