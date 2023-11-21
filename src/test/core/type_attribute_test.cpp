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
    attr.recursive_reference(r);
    ASSERT_FALSE(r->struct_type->recursive);
    auto& fields = r->struct_type->fields;
    ASSERT_EQ(fields.size(), 1);
    auto fmt = ast::cast_to<ast::Format>(fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->recursive);
}

TEST(RecursiveDetection, DetectNested) {
    auto r = parse_and_typing(R"(
format A:
    a :B

format B:
    b :A

format C:
    c :A
)");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    ASSERT_FALSE(r->struct_type->recursive);
    auto& fields = r->struct_type->fields;
    ASSERT_EQ(fields.size(), 3);
    auto fmt = ast::cast_to<ast::Format>(fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->recursive);
    auto fmt2 = ast::cast_to<ast::Format>(fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_TRUE(fmt2->body->struct_type->recursive);
    auto fmt3 = ast::cast_to<ast::Format>(fields[2]);
    ASSERT_TRUE(fmt3);
    ASSERT_FALSE(fmt3->body->struct_type->recursive);
}

TEST(IntSet, IntSetSimple) {
    auto r = parse_and_typing(R"(
format A:
    hello :u8
    world :u16
    is    :u32
    cheap :u64
    )");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    ASSERT_FALSE(r->struct_type->is_int_set);
}

TEST(IntSet, IntSetNested) {
    auto r = parse_and_typing(R"(
format A:
    fix :[30]u8

format B:
    a :A
    b :u8

format C:
    not_ :[..]u8

format D:
    len :u8
    data :[len]u8
    )");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    ASSERT_FALSE(r->struct_type->is_int_set);
    ASSERT_EQ(r->struct_type->fields.size(), 4);
    auto fmt = ast::as<ast::Format>(r->struct_type->fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->is_int_set);
    ASSERT_EQ(fmt->body->struct_type->fields.size(), 1);
    auto field = ast::as<ast::Field>(fmt->body->struct_type->fields[0]);
    ASSERT_TRUE(field);
    ASSERT_TRUE(field->field_type->is_int_set);

    auto fmt2 = ast::as<ast::Format>(r->struct_type->fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_TRUE(fmt2->body->struct_type->is_int_set);
    ASSERT_EQ(fmt2->body->struct_type->fields.size(), 2);
    auto field2 = ast::as<ast::Field>(fmt2->body->struct_type->fields[0]);
    ASSERT_TRUE(field2);
    ASSERT_TRUE(field2->field_type->is_int_set);
    auto field3 = ast::as<ast::Field>(fmt2->body->struct_type->fields[1]);
    ASSERT_TRUE(field3);
    ASSERT_TRUE(field3->field_type->is_int_set);

    auto fmt3 = ast::as<ast::Format>(r->struct_type->fields[2]);
    ASSERT_TRUE(fmt3);
    ASSERT_FALSE(fmt3->body->struct_type->is_int_set);
    ASSERT_EQ(fmt3->body->struct_type->fields.size(), 1);
    auto field4 = ast::as<ast::Field>(fmt3->body->struct_type->fields[0]);
    ASSERT_TRUE(field4);
    ASSERT_FALSE(field4->field_type->is_int_set);

    auto fmt4 = ast::as<ast::Format>(r->struct_type->fields[3]);
    ASSERT_TRUE(fmt4);
    ASSERT_FALSE(fmt4->body->struct_type->is_int_set);
    ASSERT_EQ(fmt4->body->struct_type->fields.size(), 2);
    auto field5 = ast::as<ast::Field>(fmt4->body->struct_type->fields[0]);
    ASSERT_TRUE(field5);
    ASSERT_TRUE(field5->field_type->is_int_set);
    auto field6 = ast::as<ast::Field>(fmt4->body->struct_type->fields[1]);
    ASSERT_TRUE(field6);
    ASSERT_FALSE(field6->field_type->is_int_set);
}
