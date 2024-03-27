/*license*/

#include <tool/json2c/ctype.h>
#include <core/ast/ast.h>
#include <gtest/gtest.h>
#include <core/ast/parse.h>
#include <core/middle/typing.h>
#include <core/middle/type_attribute.h>
using namespace brgen;

std::shared_ptr<ast::Program> parse_and_typing(auto text) {
    File file;
    make_file_from_text<std::string>(file, text);
    ast::Context c;
    auto s = c.enter_stream(&file, [&](ast::Stream& s) {
        return ast::parse(s, nullptr);
    });
    [&] {
        ASSERT_TRUE(s);
    }();
    auto p = (*s);
    auto r = middle::analyze_type(p, nullptr);
    [&] {
        ASSERT_TRUE(s);
    }();
    middle::mark_recursive_reference(p);
    middle::detect_non_dynamic_type(p);
    middle::analyze_bit_size_and_alignment(p);
    return p;
}

TEST(CTypeBasic, CTypeTest) {
    auto p = parse_and_typing(R"(
format A:
    x :u8 # primitive
    y :[2]u8 # static array
    z :[x]u8 # dynamic array
    a :[x][3]u8 # dynamic array of static array
    b :[3][x]u8 # static array of dynamic array
    c :[x][x]u8 # dynamic array of dynamic array
    d :[x][3][x]u8 # dynamic array of static array of dynamic array       
    e :[3][x][4]u8 # static array of dynamic array of static array
)");
    ASSERT_TRUE(p && p->struct_type);
    ASSERT_EQ(p->struct_type->fields.size(), 1);
    auto f = ast::as<ast::Format>(p->struct_type->fields[0]);
    ASSERT_TRUE(f && f->body);
    ASSERT_EQ(f->body->elements.size(), 8);
    std::vector<std::string_view> expects{
        "uint8_t x",
        "uint8_t y[2]",
        "uint8_t* z",
        "uint8_t(* a)[3]",
        "uint8_t* b[3]",
        "uint8_t** c",
        "uint8_t*(* d)[3]",
        "uint8_t(* e[3])[4]",
    };
    for (auto i = 0; i < f->body->elements.size(); i++) {
        auto e = ast::as<ast::Field>(f->body->elements[i]);
        ASSERT_TRUE(e);
        auto typ = json2c::get_type(e->field_type);
        ASSERT_TRUE(typ);
        auto cmp = typ->to_string(e->ident->ident);
        ASSERT_EQ(cmp, expects[i]);
    }
}
