/*license*/
#include <core/writer/section.h>

#include <gtest/gtest.h>
using namespace brgen;

TEST(SectionWriter, RootWriter) {
    auto root = writer::root();
    auto a = root->lookup(".");
    auto b = root->lookup("/");
    ASSERT_EQ(a, root);
    ASSERT_EQ(a, b);
}

TEST(SectionWriter, ParentWriter) {
    auto root = writer::root();
    auto ch1 = root->add_section("child1").value();
    auto ch2 = root->add_section(".").value();  // anonymous
    auto ch1_parent = ch1->lookup("..");
    auto ch1_root = ch1->lookup("/");
    auto anonymous = ch1->add_section("..").value();
    auto anonymous_parent = anonymous->lookup("..");
    ASSERT_EQ(ch1_parent, root);
    ASSERT_EQ(ch1_root, root);
    ASSERT_EQ(ch1_parent, ch2->lookup(".."));
    ASSERT_EQ(anonymous_parent, root);
}

TEST(SectionWriter, Writing) {
    writer::Writer w;
    auto root = writer::root();
    auto global = root->add_section("global").value();
    auto decl = global->add_section("decl").value();
    decl->foot().line();
    auto def = global->add_section("def").value();
    def->foot().line();

    auto m = root->add_section("main", true).value();
    m->head().writeln("int main() {");
    m->foot().writeln("}");

    auto statement = m->add_section(".").value();
    statement->head().writeln(R"(printf("hello world");)");
    statement->lookup("/global/def")->add_section(".").value()->head().writeln("extern int printf(const char*,...);");

    root->flush(w);

    fprintf(stdout, "%s", w.out().c_str());
}
