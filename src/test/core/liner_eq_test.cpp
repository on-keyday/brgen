/*license*/
#pragma once

#include <core/ast/tool/tool.h>
#include <gtest/gtest.h>
#include <core/ast/parse.h>

TEST(Tool, LinerEquation) {
    using namespace brgen;
    ast::tool::LinerResolver r;
    auto parse_expr = [&](auto&& text) -> std::shared_ptr<ast::Program> {
        File f;
        make_file_from_text(f, text);
        ast::Context c;
        auto e = c.enter_stream(&f, [](ast::Stream& s) {
            ast::Parser p{s};
            return p.parse();
        });
        auto program = e.value();
        ASSERT_TRUE(program->elements.front() && ast::as<ast::Expr>(program->elements.front()));
        return program;
    };
    auto e = parse_expr("x * (2 + 3)");
    auto expr = ast::cast_to<ast::Expr>(e->elements.front());
    auto r1 = r.resolve(expr);
    ASSERT_TRUE(r1);
}
