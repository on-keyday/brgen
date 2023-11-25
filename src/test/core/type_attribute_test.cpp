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

format D:
    d :[10]D
)");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    ASSERT_FALSE(r->struct_type->recursive);
    auto& fields = r->struct_type->fields;
    ASSERT_EQ(fields.size(), 4);
    auto fmt = ast::cast_to<ast::Format>(fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->recursive);
    auto fmt2 = ast::cast_to<ast::Format>(fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_TRUE(fmt2->body->struct_type->recursive);
    auto fmt3 = ast::cast_to<ast::Format>(fields[2]);
    ASSERT_TRUE(fmt3);
    ASSERT_FALSE(fmt3->body->struct_type->recursive);
    auto fmt4 = ast::cast_to<ast::Format>(fields[3]);
    ASSERT_TRUE(fmt4);
    ASSERT_TRUE(fmt4->body->struct_type->recursive);
}

TEST(IntSet, IntSetSimple) {
    auto r = parse_and_typing(R"(
format A:
    hello :u8
    world :u16
    is    :u32
    cheap :u64

format B:
    a :u8
    )");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    ASSERT_FALSE(r->struct_type->is_int_set);
    ASSERT_EQ(r->struct_type->fields.size(), 2);
    auto fmt = ast::as<ast::Format>(r->struct_type->fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->is_int_set);

    ASSERT_EQ(fmt->body->struct_type->fields.size(), 4);
    auto field = ast::as<ast::Field>(fmt->body->struct_type->fields[0]);
    ASSERT_TRUE(field);
    ASSERT_TRUE(field->field_type->is_int_set);

    auto field2 = ast::as<ast::Field>(fmt->body->struct_type->fields[1]);
    ASSERT_TRUE(field2);
    ASSERT_TRUE(field2->field_type->is_int_set);

    auto field3 = ast::as<ast::Field>(fmt->body->struct_type->fields[2]);
    ASSERT_TRUE(field3);
    ASSERT_TRUE(field3->field_type->is_int_set);

    auto fmt2 = ast::as<ast::Format>(r->struct_type->fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_TRUE(fmt2->body->struct_type->is_int_set);

    ASSERT_EQ(fmt2->body->struct_type->fields.size(), 1);
    auto field4 = ast::as<ast::Field>(fmt2->body->struct_type->fields[0]);
    ASSERT_TRUE(field4);
    ASSERT_TRUE(field4->field_type->is_int_set);
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

format E:
    if true:
        a :u8
    else:
        b :u16
    )");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    ASSERT_FALSE(r->struct_type->is_int_set);
    ASSERT_EQ(r->struct_type->fields.size(), 5);
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

    auto fmt5 = ast::as<ast::Format>(r->struct_type->fields[3]);
    ASSERT_TRUE(fmt5);
    ASSERT_TRUE(fmt5->body->struct_type->is_int_set);

    ASSERT_EQ(fmt5->body->struct_type->fields.size(), 2);
    auto field7 = ast::as<ast::Field>(fmt5->body->struct_type->fields[0]);
    ASSERT_TRUE(field7);
    ASSERT_TRUE(field7->field_type->is_int_set);

    auto field8 = ast::as<ast::Field>(fmt5->body->struct_type->fields[1]);
    ASSERT_TRUE(field8);
    ASSERT_TRUE(field8->field_type->is_int_set);
}

TEST(IntSet, IntSetUnion) {
    auto r = parse_and_typing(R"(
format A:
    cond :u8
    if cond == 8:
        a :u8
    else:
        b :u16
)");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    ASSERT_FALSE(r->struct_type->is_int_set);
    ASSERT_EQ(r->struct_type->fields.size(), 1);
    auto fmt = ast::as<ast::Format>(r->struct_type->fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_TRUE(fmt->body->struct_type->is_int_set);
    ASSERT_EQ(fmt->body->struct_type->fields.size(), 4);
    auto field = ast::as<ast::Field>(fmt->body->struct_type->fields[0]);
    ASSERT_TRUE(field);
    ASSERT_TRUE(field->field_type->is_int_set);
    auto field2 = ast::as<ast::Field>(fmt->body->struct_type->fields[1]);
    ASSERT_TRUE(field2);
    ASSERT_TRUE(field2->field_type->is_int_set);
    auto field3 = ast::as<ast::Field>(fmt->body->struct_type->fields[2]);
    ASSERT_TRUE(field3);
    ASSERT_TRUE(field3->field_type->is_int_set);
    auto field4 = ast::as<ast::Field>(fmt->body->struct_type->fields[3]);
    ASSERT_TRUE(field4);
    ASSERT_TRUE(field4->field_type->is_int_set);
}

TEST(BitAlignment, BitAlignmentSimple) {
    auto r = parse_and_typing(R"(
format A:
    a :u8
    b :u16
    c :u32
    d :u64

format B:
    a :u7
    b :u8
    c :u1
    d :u15
    e :u2
)");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    attr.bit_alignment(r);
    ASSERT_EQ(r->struct_type->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(r->struct_type->fields.size(), 2);
    auto fmt = ast::as<ast::Format>(r->struct_type->fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_EQ(fmt->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt->body->struct_type->fields.size(), 4);
    auto field = ast::as<ast::Field>(fmt->body->struct_type->fields[0]);
    ASSERT_TRUE(field);
    ASSERT_EQ(field->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    auto field2 = ast::as<ast::Field>(fmt->body->struct_type->fields[1]);
    ASSERT_TRUE(field2);
    ASSERT_EQ(field2->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field2->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    auto field3 = ast::as<ast::Field>(fmt->body->struct_type->fields[2]);
    ASSERT_TRUE(field3);
    ASSERT_EQ(field3->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field3->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    auto field4 = ast::as<ast::Field>(fmt->body->struct_type->fields[3]);
    ASSERT_TRUE(field4);
    ASSERT_EQ(field4->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field4->field_type->bit_alignment, ast::BitAlignment::byte_aligned);

    auto fmt2 = ast::as<ast::Format>(r->struct_type->fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_EQ(fmt2->body->struct_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(fmt2->body->struct_type->fields.size(), 5);
    auto field5 = ast::as<ast::Field>(fmt2->body->struct_type->fields[0]);
    ASSERT_TRUE(field5);
    ASSERT_EQ(field5->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field5->field_type->bit_alignment, ast::BitAlignment::bit_7);
    auto field6 = ast::as<ast::Field>(fmt2->body->struct_type->fields[1]);
    ASSERT_TRUE(field6);
    ASSERT_EQ(field6->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field6->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    auto field7 = ast::as<ast::Field>(fmt2->body->struct_type->fields[2]);
    ASSERT_TRUE(field7);
    ASSERT_EQ(field7->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field7->field_type->bit_alignment, ast::BitAlignment::bit_1);
    auto field8 = ast::as<ast::Field>(fmt2->body->struct_type->fields[3]);
    ASSERT_TRUE(field8);
    ASSERT_EQ(field8->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field8->field_type->bit_alignment, ast::BitAlignment::bit_7);
    auto field9 = ast::as<ast::Field>(fmt2->body->struct_type->fields[4]);
    ASSERT_TRUE(field9);
    ASSERT_EQ(field9->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field9->field_type->bit_alignment, ast::BitAlignment::bit_2);
}

TEST(BitAlignment, BitAlignmentNested) {
    auto r = parse_and_typing(R"(
format A:
    a :u8
    b :u16
    c :u32
    d :u64

format B:
    a :u7
    b :A
    c :u1

format C:
    ackNum :u32
    dataOffset :u4
    reserved :u4
    CWE :u1 ECE :u1 URG :u1 ACK :u1 PSH :u1 RST :u1 SYN :u1 FIN :u1
    windowSize :u16
    a :[dataOffset * 4 - 20]u8
    )");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    attr.bit_alignment(r);
    ASSERT_EQ(r->struct_type->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(r->struct_type->fields.size(), 3);
    auto fmt = ast::as<ast::Format>(r->struct_type->fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_EQ(fmt->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt->body->struct_type->fields.size(), 4);
    ASSERT_EQ(fmt->body->struct_type->bit_size, 120);
    auto field = ast::as<ast::Field>(fmt->body->struct_type->fields[0]);
    ASSERT_TRUE(field);
    ASSERT_EQ(field->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field->field_type->bit_size, 8);
    auto field2 = ast::as<ast::Field>(fmt->body->struct_type->fields[1]);
    ASSERT_TRUE(field2);
    ASSERT_EQ(field2->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field2->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field2->field_type->bit_size, 16);
    auto field3 = ast::as<ast::Field>(fmt->body->struct_type->fields[2]);
    ASSERT_TRUE(field3);
    ASSERT_EQ(field3->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field3->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field3->field_type->bit_size, 32);
    auto field4 = ast::as<ast::Field>(fmt->body->struct_type->fields[3]);
    ASSERT_TRUE(field4);
    ASSERT_EQ(field4->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field4->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field4->field_type->bit_size, 64);

    auto fmt2 = ast::as<ast::Format>(r->struct_type->fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_EQ(fmt2->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt2->body->struct_type->fields.size(), 3);
    ASSERT_EQ(fmt2->body->struct_type->bit_size, 128);
    auto field5 = ast::as<ast::Field>(fmt2->body->struct_type->fields[0]);
    ASSERT_TRUE(field5);
    ASSERT_EQ(field5->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field5->field_type->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field5->field_type->bit_size, 7);
    auto field6 = ast::as<ast::Field>(fmt2->body->struct_type->fields[1]);
    ASSERT_TRUE(field6);
    ASSERT_EQ(field6->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field6->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field6->field_type->bit_size, 120);
    auto field7 = ast::as<ast::Field>(fmt2->body->struct_type->fields[2]);
    ASSERT_TRUE(field7);
    ASSERT_EQ(field7->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field7->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field7->field_type->bit_size, 1);

    auto fmt3 = ast::as<ast::Format>(r->struct_type->fields[2]);
    ASSERT_TRUE(fmt3);
    ASSERT_EQ(fmt3->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt3->body->struct_type->fields.size(), 13);
    ASSERT_EQ(fmt3->body->struct_type->bit_size, 0);
    auto field8 = ast::as<ast::Field>(fmt3->body->struct_type->fields[0]);
    ASSERT_TRUE(field8);
    ASSERT_EQ(field8->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field8->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field8->field_type->bit_size, 32);
    auto field9 = ast::as<ast::Field>(fmt3->body->struct_type->fields[1]);
    ASSERT_TRUE(field9);
    ASSERT_EQ(field9->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field9->field_type->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field9->field_type->bit_size, 4);
    auto field10 = ast::as<ast::Field>(fmt3->body->struct_type->fields[2]);
    ASSERT_TRUE(field10);
    ASSERT_EQ(field10->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field10->field_type->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field10->field_type->bit_size, 4);
    auto field11 = ast::as<ast::Field>(fmt3->body->struct_type->fields[3]);
    ASSERT_TRUE(field11);
    ASSERT_EQ(field11->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field11->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field11->field_type->bit_size, 1);
    auto field12 = ast::as<ast::Field>(fmt3->body->struct_type->fields[4]);
    ASSERT_TRUE(field12);
    ASSERT_EQ(field12->bit_alignment, ast::BitAlignment::bit_2);
    ASSERT_EQ(field12->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field12->field_type->bit_size, 1);
    auto field13 = ast::as<ast::Field>(fmt3->body->struct_type->fields[5]);
    ASSERT_TRUE(field13);
    ASSERT_EQ(field13->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(field13->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field13->field_type->bit_size, 1);
    auto field14 = ast::as<ast::Field>(fmt3->body->struct_type->fields[6]);
    ASSERT_TRUE(field14);
    ASSERT_EQ(field14->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field14->field_type->bit_alignment, ast::BitAlignment::bit_1);
    auto field15 = ast::as<ast::Field>(fmt3->body->struct_type->fields[7]);
    ASSERT_TRUE(field15);
    ASSERT_EQ(field15->bit_alignment, ast::BitAlignment::bit_5);
    ASSERT_EQ(field15->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field15->field_type->bit_size, 1);
    auto field16 = ast::as<ast::Field>(fmt3->body->struct_type->fields[8]);
    ASSERT_TRUE(field16);
    ASSERT_EQ(field16->bit_alignment, ast::BitAlignment::bit_6);
    ASSERT_EQ(field16->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field16->field_type->bit_size, 1);
    auto field17 = ast::as<ast::Field>(fmt3->body->struct_type->fields[9]);
    ASSERT_TRUE(field17);
    ASSERT_EQ(field17->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field17->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field17->field_type->bit_size, 1);
    auto field18 = ast::as<ast::Field>(fmt3->body->struct_type->fields[10]);
    ASSERT_TRUE(field18);
    ASSERT_EQ(field18->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field18->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field18->field_type->bit_size, 1);
    auto field19 = ast::as<ast::Field>(fmt3->body->struct_type->fields[11]);
    ASSERT_TRUE(field19);
    ASSERT_EQ(field19->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field19->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field19->field_type->bit_size, 16);
    auto field20 = ast::as<ast::Field>(fmt3->body->struct_type->fields[12]);
    ASSERT_TRUE(field20);
    ASSERT_EQ(field20->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field20->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field20->field_type->bit_size, 0);  // TODO(on-keyday): fix this
}

TEST(BitAlignment, BitAlignmentUnionEnum) {
    auto r = parse_and_typing(R"(
format A:
    cond :u8
    if cond == 8:
        a :u8
    else:
        b :u16
    
format B:
    a :u7
    b :A
    c :u1

format C:
    flag :u1
    match flag:
        0 => data :D
        1 => data :u7 

format D:
    data1 :u3
    data2 :u4

enum E:
    :u4
    a = 0
    b = 1
    c = 2
    d = 3

format F:
    a :u3
    b :E
    c :u4


    )");
    middle::TypeAttribute attr;
    attr.recursive_reference(r);
    attr.int_type_detection(r);
    attr.bit_alignment(r);
    ASSERT_EQ(r->struct_type->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(r->struct_type->fields.size(), 6);

    auto fmt = ast::as<ast::Format>(r->struct_type->fields[0]);
    ASSERT_TRUE(fmt);
    ASSERT_EQ(fmt->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt->body->struct_type->fields.size(), 4);
    ASSERT_EQ(fmt->body->struct_type->bit_size, 0);  // not decidable
    auto field = ast::as<ast::Field>(fmt->body->struct_type->fields[0]);
    ASSERT_TRUE(field);
    ASSERT_EQ(field->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field->field_type->bit_size, 8);
    auto field2 = ast::as<ast::Field>(fmt->body->struct_type->fields[1]);
    ASSERT_TRUE(field2);
    ASSERT_EQ(field2->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field2->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field2->field_type->bit_size, 0);  // not decidable
    auto field3 = ast::as<ast::Field>(fmt->body->struct_type->fields[2]);
    ASSERT_TRUE(field3);
    ASSERT_EQ(field3->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(field3->field_type->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(field3->field_type->bit_size, 8);
    auto field4 = ast::as<ast::Field>(fmt->body->struct_type->fields[3]);
    ASSERT_TRUE(field4);
    ASSERT_EQ(field4->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(field4->field_type->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(field4->field_type->bit_size, 16);

    auto fmt2 = ast::as<ast::Format>(r->struct_type->fields[1]);
    ASSERT_TRUE(fmt2);
    ASSERT_EQ(fmt2->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt2->body->struct_type->fields.size(), 3);
    ASSERT_EQ(fmt2->body->struct_type->bit_size, 0);  // not decidable
    auto field5 = ast::as<ast::Field>(fmt2->body->struct_type->fields[0]);
    ASSERT_TRUE(field5);
    ASSERT_EQ(field5->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field5->field_type->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field5->field_type->bit_size, 7);
    auto field6 = ast::as<ast::Field>(fmt2->body->struct_type->fields[1]);
    ASSERT_TRUE(field6);
    ASSERT_EQ(field6->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field6->field_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field6->field_type->bit_size, 0);  // not decidable
    auto field7 = ast::as<ast::Field>(fmt2->body->struct_type->fields[2]);
    ASSERT_TRUE(field7);
    ASSERT_EQ(field7->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field7->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field7->field_type->bit_size, 1);

    auto fmt3 = ast::as<ast::Format>(r->struct_type->fields[2]);
    ASSERT_TRUE(fmt3);
    ASSERT_EQ(fmt3->body->struct_type->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(fmt3->body->struct_type->fields.size(), 3);
    ASSERT_EQ(fmt3->body->struct_type->bit_size, 8);
    auto field8 = ast::as<ast::Field>(fmt3->body->struct_type->fields[0]);
    ASSERT_TRUE(field8);
    ASSERT_EQ(field8->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field8->field_type->bit_alignment, ast::BitAlignment::bit_1);
    ASSERT_EQ(field8->field_type->bit_size, 1);
    auto field9 = ast::as<ast::Field>(fmt3->body->struct_type->fields[1]);
    ASSERT_TRUE(field9);
    ASSERT_EQ(field9->bit_alignment, ast::BitAlignment::byte_aligned);
    ASSERT_EQ(field9->field_type->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field9->field_type->bit_size, 7);  // not decidable
    auto field10 = ast::as<ast::Field>(fmt3->body->struct_type->fields[2]);
    ASSERT_TRUE(field10);
    ASSERT_EQ(field10->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(field10->field_type->bit_alignment, ast::BitAlignment::not_target);
    ASSERT_EQ(field10->field_type->bit_size, 7);  // not decidable

    auto fmt4 = ast::as<ast::Format>(r->struct_type->fields[3]);
    ASSERT_TRUE(fmt4);
    ASSERT_EQ(fmt4->body->struct_type->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(fmt4->body->struct_type->fields.size(), 2);
    ASSERT_EQ(fmt4->body->struct_type->bit_size, 7);  // not decidable
    auto field11 = ast::as<ast::Field>(fmt4->body->struct_type->fields[0]);
    ASSERT_TRUE(field11);
    ASSERT_EQ(field11->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(field11->field_type->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(field11->field_type->bit_size, 3);
    auto field12 = ast::as<ast::Field>(fmt4->body->struct_type->fields[1]);
    ASSERT_TRUE(field12);
    ASSERT_EQ(field12->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field12->field_type->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field12->field_type->bit_size, 4);

    auto fmt5 = ast::as<ast::Format>(r->struct_type->fields[5]);
    ASSERT_TRUE(fmt5);
    ASSERT_EQ(fmt5->body->struct_type->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(fmt5->body->struct_type->fields.size(), 3);
    ASSERT_EQ(fmt5->body->struct_type->bit_size, 11);  // not decidable
    auto field13 = ast::as<ast::Field>(fmt5->body->struct_type->fields[0]);
    ASSERT_TRUE(field13);
    ASSERT_EQ(field13->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(field13->field_type->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(field13->field_type->bit_size, 3);
    auto field14 = ast::as<ast::Field>(fmt5->body->struct_type->fields[1]);
    ASSERT_TRUE(field14);
    ASSERT_EQ(field14->bit_alignment, ast::BitAlignment::bit_7);
    ASSERT_EQ(field14->field_type->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field14->field_type->bit_size, 4);
    auto field15 = ast::as<ast::Field>(fmt5->body->struct_type->fields[2]);
    ASSERT_TRUE(field15);
    ASSERT_EQ(field15->bit_alignment, ast::BitAlignment::bit_3);
    ASSERT_EQ(field15->field_type->bit_alignment, ast::BitAlignment::bit_4);
    ASSERT_EQ(field15->field_type->bit_size, 4);
}
