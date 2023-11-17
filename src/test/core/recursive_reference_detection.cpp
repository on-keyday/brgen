/*license*/

#include <gtest/gtest.h>
#include <core/middle/typing.h>
#include <core/middle/type_attribute.h>
#include <core/ast/parse.h>

using namespace brgen;

std::shared_ptr<ast::Program> parse_and_typing(auto text) {
    File file;
    make_file_from_text<std::string>(file, text);
    ast::Context c;
    auto s = c.enter_stream(&file, [&](ast::Stream& s) {
        ast::Parser p{s};
        return p.parse();
    });
    [&] {
        ASSERT_TRUE(s);
    }();
    auto p = (*s);
    middle::Typing typing;
    auto r = typing.typing(p);
    [&] {
        ASSERT_TRUE(s);
    }();
    return p;
}

TEST(RecursiveDetection, DetectSimple) {
    auto r = parse_and_typing(R"(
format A:
    a :A
)");
    middle::TypeAttribute attr;
    attr.check_recursive_reference(r);
    ASSERT_FALSE(r->struct_type->recursive);
    auto& fields = r->struct_type->fields;
    ASSERT_EQ(fields.size(), 1);
    auto fmt = ast::cast_to<ast::Format>(fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->recursive);
}
