/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <comb2/composite/range.h>
#include <comb2/composite/comment.h>
#include <comb2/composite/number.h>
#include <comb2/basic/group.h>
#include <comb2/tree/simple_node.h>
#include <comb2/tree/branch_table.h>
#include <file/file_view.h>
#include <code/src_location.h>
constexpr auto tag_ident = "ident";
constexpr auto tag_expr = "expr";
constexpr auto tag_field = "field";
constexpr auto tag_type = "type";
constexpr auto tag_struct = "struct";
constexpr auto tag_integer = "integer";

namespace parse {
    using namespace utils::comb2;
    namespace cps = utils::comb2::composite;
    using namespace ops;
    constexpr auto ident = cps::c_ident;
    constexpr auto tagged_ident = str(tag_ident, ident);
    constexpr auto comment = cps::c_comment | cps::cpp_comment;

    constexpr auto space = *(cps::space | cps::tab | cps::eol | comment);
    constexpr auto integer = str("integer", cps::hex_integer | cps::dec_integer);
    constexpr auto expr = method_proxy(expr);
    constexpr auto prim = (lit("(") & space & +expr & space & +lit(")")) | integer | +tagged_ident;
    constexpr auto pow = prim & space & -(str("pow", lit("^")) & space & +prim);
    constexpr auto sub = pow & space & -(str("sub", lit("-")) & space & +pow);
    constexpr auto expr_ = group("expr", sub);

    constexpr auto enum_value = group("enum_value", -tagged_ident& space& lit("(") & space & +expr & space & lit(")") & space & -lit(","));

    constexpr auto enum_ =
        group("enum",
              lit("enum") & space & space & +lit("{") &
                  *(space & enum_value) & space & +lit("}") & space & +tagged_ident & space & +lit(";") & space);
    constexpr auto type = method_proxy(type);
    constexpr auto select_case = group("select_case", lit("case") & space & +expr & space & +lit(":") & space & +type & space & +lit(";"));

    constexpr auto select_type = group("select_type", lit("select") & space & +lit("(") & space & +expr & space & +lit(")") & space & +lit('{') & *(space & select_case) & space & +lit('}'));

    constexpr auto array_suffix = group("array_suffix", lit("[") & space & +expr & space & +lit("]"));
    constexpr auto vector_suffix = group("vector_suffix", lit("<") & space & +expr & space & +lit("..") & space & +expr & +lit(">"));

    constexpr auto type_ = group(tag_type, select_type | tagged_ident);
    constexpr auto struct_field = group(tag_field, type& space & +tagged_ident & space & -(array_suffix | vector_suffix) & +lit(";"));

    constexpr auto struct_ = group(tag_struct,
                                   lit("struct") & space & +lit("{") &
                                       *(space & struct_field) & space & +lit("}") & space & +tagged_ident & space & +lit(";") & space);
    constexpr auto file = space & *(enum_ | struct_) & space & +eos;

    constexpr auto sample_text = R"(
/*
ASN payload of RSA PKCS1 v1.5 signature

OID = 30 07 06 05 2b 0e 03 02 1a
Comment = Sha1 (OID 1 3 14 3 2 26)
OID = 30 0b 06 09 60 86 48 01 65 03 04 02 04
Comment = SHA224 (OID 2 16 840 1 101 3 4 2 4)
OID = 30 0b 06 09 60 86 48 01 65 03 04 02 01
Comment = SHA256 (OID 2 16 840 1 101 3 4 2 1)
OID = 30 0b 06 09 60 86 48 01 65 03 04 02 02
Comment = SHA384 (OID 2 16 840 1 101 3 4 2 2)
OID = 30 0b 06 09 60 86 48 01 65 03 04 02 03
Comment = SHA512 (OID 2 16 840 1 101 3 4 2 3)
*/

enum { sha512_len(0x0440), (65535) } sha512_len;

struct {
  sha512_len l;
  opaque sha512_digest[64];
} sha2_512;

enum { sha384_len(0x0430), (65535) } sha384_len;

struct {
  sha384_len l;
  opaque sha384_digest[48];
} sha2_384;

enum { sha256_len(0x0420), (65535) } sha256_len;

struct {
  sha256_len l;
  opaque sha256_digest[32];
} sha2_256;

enum {
  sha2_oid(0x06096086),
  (2^32-1)
} sha2_start;

enum {
  sha2_mid(0x48016503),
  (2^32-1)
} sha2_mid;

enum {
  sha2_end(0x0402),
  (2^16-1)
} sha2_end;

enum {
  sha2_256(0x01),
  sha2_384(0x02),
  sha2_512(0x03),
  (255)
} sha2_version;

struct {
  sha2_start s;
  sha2_mid m;
  sha2_end e;
  sha2_version version;
  select(version){
    case sha2_256: sha2_256;
    case sha2_384: sha2_384;
    case sha2_512: sha2_512;
  } v;
} oid_sha2;

enum {
  sha1_oid(0x06052b0e),
  (2^32-1)
} sha1_prefix;

enum {
  sha1_oid_end(0x03021a04),
  (2^32-1)
} sha1_suffix;

enum {
  sha1_len(0x14),
  (255)
} sha1_len;

struct {
  sha1_prefix s;
  sha1_suffix e;
  sha1_len l;
  opaque sha1_digest[20];
} oid_sha1;

enum {
  start_sha1 (0x3007),
  start_sha2 (0x300b),
  (2^16-1)
} start_oid;

struct {
  start_oid tag;
  select(tag) {
    case start_sha1: oid_sha1;
    case start_sha2: oid_sha2;
  } v;
} oid_payload;

enum {
  struct_tag(0x30),
  (255)
} stag;

struct {
  stag struct_open;
  asn1_len len;
  oid_payload payload [len];
} pkcs1_sig;

struct {
  uint16 length;
  opaque label<7..255>;
  opaque context<0..255>;
} HkdfLabel;
    )";

    constexpr auto parse(auto&& view, auto&& ctx) {
        auto seq = utils::make_ref_seq(view);
        struct r {
            decl_method_proxy(expr);
            decl_method_proxy(type);
        } r_{expr_, type_};
        return file(seq, ctx, r_) == Status::match;
    }

    constexpr auto test_parse() {
        using namespace utils::comb2::test;
        TestContext ctx;
        return parse(sample_text, ctx);
    }

    static_assert(test_parse());
}  // namespace parse

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string_view> args;
    std::string json_file;
    void bind(utils::cmdline::option::Context& c) {
        bind_help(c);
        c.VarString(&json_file, "json", "json file to replace type name", "FILE");
    }
};
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

void print(const std::shared_ptr<utils::comb2::tree::node::Node>& n, size_t depth = 0) {
    for (size_t i = 0; i < depth; ++i) {
        cout << "  ";
    }
    if (n->tag) {
        cout << n->tag;
    }
    if (auto gr = as_group(n)) {
        cout << "\n";
        for (auto& c : gr->children) {
            print(c, n->tag ? depth + 1 : depth);
        }
    }
    else if (auto tok = as_tok(n)) {
        cout << " " << tok->token << "\n";
    }
}

int Main(Flags& flags, utils::cmdline::option::Context&) {
    if (flags.args.size() != 1) {
        cout << "usage: rfc2bgn [options] FILE\n";
        return 1;
    }
    utils::file::View input;
    if (auto o = input.open(flags.args[0]); !o) {
        std::string err;
        o.error().error(err);
        cout << "error: cannot open file: " << err << "\n";
        return 1;
    }
    Ctx ctx;
    if (!parse::parse(input, ctx)) {
        cout << "error: parse error\n";
        return 1;
    }
    auto g = utils::comb2::tree::node::collect(ctx.root_branch);
    if (!g) {
        cout << "error: collect error\n";
        return 1;
    }
    print(g);
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
