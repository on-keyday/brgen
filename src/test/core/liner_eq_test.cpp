/*license*/

#include <core/ast/tool/tool.h>
#include <gtest/gtest.h>
#include <core/ast/parse.h>

TEST(Tool, LinerEquation) {
    using namespace brgen;
    ast::tool::LinerResolver r;
    auto parse_expr = [&](auto&& text) -> std::shared_ptr<ast::Program> {
        File f;
        make_file_from_text<std::string>(f, text);
        ast::Context c;
        auto e = c.enter_stream(&f, [](ast::Stream& s) {
            return ast::parse(s, nullptr);
        });
        auto program = e.value();
        auto element = program->elements.front();
        auto expr = ast::as<ast::Expr>(element);
        [&] {
            ASSERT_TRUE(element && expr);
        }();
        return program;
    };
    auto do_test = [&](auto input, auto expect) {
        auto e = parse_expr(input);
        auto expr = ast::cast_to<ast::Expr>(e->elements.front());
        auto r1 = r.resolve(expr);
        ASSERT_TRUE(r1);
        ast::tool::Stringer s;
        ASSERT_EQ(s.to_string(r.resolved), expect);
    };
    do_test("1", "1");
    do_test("1 + 2", "(1 + 2)");
    do_test("x", "tmp0");
    do_test("x + 1", "(tmp0 - 1)");
    do_test("x + 1 + 2", "((tmp0 - 2) - 1)");
    do_test("x + 2 * 3", "(tmp0 - (2 * 3))");
    do_test("2 * x + 3 + 4", "(((tmp0 - 4) - 3) / 2)");
    do_test("2 * 3 + x + 4 + 5", "(((tmp0 - 5) - 4) - (2 * 3))");
    do_test("2 * 3 + 4 + 5 + x", "(tmp0 - (((2 * 3) + 4) + 5))");
    do_test("x << 2 + 1", "((tmp0 - 1) >> 2)");
    do_test("(x << 2) * 1 + 2", "(((tmp0 - 2) / 1) >> 2)");
}
