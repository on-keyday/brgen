/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <file/file_view.h>
#include <wrap/cout.h>
#include <comb2/composite/range.h>
#include <comb2/composite/comment.h>
#include <comb2/composite/number.h>
#include <comb2/basic/group.h>
#include <comb2/tree/simple_node.h>
#include <comb2/tree/branch_table.h>
#include <code/code_writer.h>
#include <json/json_export.h>
#include <json/parse.h>
#include <cmdline/template/parse_and_err.h>
#include <code/src_location.h>
using namespace utils::comb2;

namespace cps = utils::comb2::composite;

constexpr auto tag_struct = "struct";
constexpr auto tag_field = "field";
constexpr auto tag_type = "type";
constexpr auto tag_name = "name";
constexpr auto tag_type_suffix = "type_suffix";
constexpr auto tag_fields = "fields";

constexpr auto tag_enum = "enum";
constexpr auto tag_value = "value";

namespace parse {
    using namespace ops;
    constexpr auto ident = cps::c_ident;
    constexpr auto comment = cps::c_comment | cps::cpp_comment;
    constexpr auto space = *(cps::space | cps::tab | cps::eol | comment);
    constexpr auto type = str(tag_type, ident);
    constexpr auto number = cps::hex_integer | cps::dec_integer;
    constexpr auto type_suffix = str(tag_type_suffix, lit("[") & space & ~(not_(lit(']')) & any) & space & +lit("]"));
    constexpr auto field = group(tag_field, type& space & -str(tag_name, ident) & space & -type_suffix & space & +lit(";"));
    constexpr auto name = str(tag_name, ident);
    constexpr auto struct_ = group(tag_struct, lit("struct") & space & -name & space & +lit("{") &
                                                   group(tag_fields, *(space& field)) & space & +lit("}") & space & -name & space & +lit(";") & space);

    constexpr auto enum_field = group(tag_field, str(tag_name, ident) & space & -(lit("=") & space & +str(tag_value, number | ident)) & space & -lit(","));
    constexpr auto enum_ = group(tag_enum, lit("enum") & space & -name & space & +lit("{") &
                                               group(tag_fields, *(space& enum_field)) & space & +lit("}") & space & -name & space & +lit(";") & space);

    constexpr auto file = space & *(-(lit("typedef") & space) & (struct_ | enum_) & space) & space & +eos;

    constexpr auto test_parse() {
        auto ctx = utils::comb2::test::TestContext{};
        auto seq = utils::make_ref_seq("struct A { int a; int b; }; struct B { int c; }; enum C { a, b, c };");
        if (file(seq, ctx, 0) != Status::match) {
            return false;
        }
        return true;
    }

    static_assert(test_parse());

}  // namespace parse
auto& cout = utils::wrap::cout_wrap();

struct Ctx : utils::comb2::tree::BranchTable {
    void error_seq(auto& seq, auto&&... err) {
        cout << "error: ";
        (cout << ... << err);
        cout << "\n";
        std::string src;
        utils::code::write_src_loc(src, seq);
        cout << src << "\n";
    }
};

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string_view> args;
    std::string json_file;
    void bind(utils::cmdline::option::Context& c) {
        bind_help(c);
        c.VarString(&json_file, "json", "json file to replace type name", "FILE");
    }
};

int Main(Flags& flags, utils::cmdline::option::Context&) {
    utils::json::JSON map;
    if (flags.args.size() != 1) {
        cout << "usage: ctobgn [options] FILE\n";
        return 1;
    }
    utils::file::View input;
    if (auto o = input.open(flags.args[0]); !o) {
        std::string err;
        o.error().error(err);
        cout << "error: cannot open file: " << err << "\n";
        return 1;
    }
    if (!flags.json_file.empty()) {
        utils::file::View view;
        if (view.open(flags.json_file)) {
            utils::json::parse(view, map);  // ignore error
        }
        else {
            cout << "warning: cannot open json file: " << flags.json_file << "\n";
        }
    }
    Ctx ctx;
    auto seq = utils::make_ref_seq(input);
    auto status = parse::file(seq, ctx, 0);
    if (status != Status::match) {
        cout << "error: parse error\n";
        return 1;
    }
    auto root = utils::comb2::tree::node::collect(ctx.root_branch);
    utils::code::CodeWriter<std::string> w;
    for (auto& struct_ : root->children) {
        auto m = utils::comb2::tree::node::as_group(struct_);
        assert(m);
        assert(m->tag == tag_struct || m->tag == tag_enum);
        assert(m->children.size() == 2 || m->children.size() == 3);
        std::string struct_name;
        size_t offset = 0;
        if (m->children[offset]->tag == tag_name) {
            auto name = utils::comb2::tree::node::as_tok(m->children[offset]);
            assert(name);
            assert(name->tag == tag_name);
            struct_name = name->token;
            offset++;
        }
        auto fields = utils::comb2::tree::node::as_group(m->children[offset]);
        assert(fields);
        assert(fields->tag == tag_fields);
        offset++;
        if (m->children.size() > offset && m->children[offset]->tag == tag_name) {
            auto name = utils::comb2::tree::node::as_tok(m->children[offset]);
            assert(name);
            assert(name->tag == tag_name);
            struct_name = name->token;
            offset++;
        }
        if (struct_->tag == tag_struct) {
            w.writeln("format ", struct_name, ":");
            auto indent = w.indent_scope();
            for (auto& field : fields->children) {
                auto f = utils::comb2::tree::node::as_group(field);
                assert(f);
                assert(f->tag == tag_field);
                assert(f->children.size() == 2 || f->children.size() == 3);
                auto type = utils::comb2::tree::node::as_tok(f->children[0]);
                assert(type);
                assert(type->tag == tag_type);
                auto name = utils::comb2::tree::node::as_tok(f->children[1]);
                assert(name);
                assert(name->tag == tag_name);
                std::string suffix;
                if (f->children.size() == 3) {
                    auto s = utils::comb2::tree::node::as_tok(f->children[2]);
                    assert(s);
                    assert(s->tag == tag_type_suffix);
                    suffix = s->token;
                }
                std::string type_name = type->token;
                if (auto a = map.at(type_name)) {
                    type_name = a->force_as_string<std::string>();
                }
                w.writeln(name->token, " :", suffix, type_name);
            }
        }
        else if (struct_->tag == tag_enum) {
            /*
            enum Name:
                field1 = value1
                field2
            */
            w.writeln("enum ", struct_name, ":");
            auto indent = w.indent_scope();
            for (auto& field : fields->children) {
                auto f = utils::comb2::tree::node::as_group(field);
                assert(f);
                assert(f->tag == tag_field);
                assert(f->children.size() == 1 || f->children.size() == 2);
                auto name = utils::comb2::tree::node::as_tok(f->children[0]);
                assert(name);
                assert(name->tag == tag_name);
                std::string value;
                if (f->children.size() == 2) {
                    auto v = utils::comb2::tree::node::as_tok(f->children[1]);
                    assert(v);
                    assert(v->tag == tag_value);
                    value = v->token;
                }
                w.write(name->token);
                if (!value.empty()) {
                    w.write(" = ", value);
                }
                w.writeln();
            }
        }

        w.writeln();
    }
    cout << w.out() << "\n";
    return 0;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cout << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
