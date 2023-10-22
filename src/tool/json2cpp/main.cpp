/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include "../common/print.h"
#include <core/ast/json.h>
#include <file/file_view.h>
#include "gen.h"
#include <binary/float.h>
#include <cfenv>

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
    bool spec = false;
    void bind(utils::cmdline::option::Context& ctx) {
        bind_help(ctx);
        ctx.VarBool(&spec, "s", "spec mode");
    }
};
auto& cout = utils::wrap::cout_wrap();

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    prefix_loc() = "json2cpp: ";
    if (flags.spec) {
        cout << R"({
            "pass_by": "file",
            "langs": ["cpp"],
            "suffix": ".hpp"
        })";
        return 0;
    }
    if (flags.args.empty()) {
        print_error("no input file");
        return 1;
    }
    if (flags.args.size() > 1) {
        print_error("too many input files");
        return 1;
    }
    auto name = flags.args[0];
    utils::file::View view;
    if (!view.open(name)) {
        print_error("cannot open file ", name);
        return 1;
    }
    auto js = utils::json::parse<utils::json::JSON>(view);
    if (js.is_undef()) {
        print_error("cannot parse json file ", name);
        return 1;
    }
    auto files = js.at("files");
    if (!files) {
        print_error("cannot find files field ", name);
        return 1;
    }
    if (!files->is_array()) {
        print_error("files field is not array ", name);
        return 1;
    }
    auto f = js.at("ast");
    if (!f) {
        print_error("cannot find ast field ", name);
        return 1;
    }
    brgen::ast::JSONConverter c;
    auto res = c.decode(*f);
    if (!res) {
        print_error("cannot decode json file: ", res.error().locations[0].msg);
        return 1;
    }
    json2cpp::Generator g(brgen::ast::cast_to<brgen::ast::Program>(*res));
    auto res2 = g.generate();
    if (!res2) {
        auto& err = res2.error();
        auto loc = err.loc;
        std::string name;
        if (loc.file == 0) {
            name = "<unknown file>";
        }
        else {
            auto l = files->at(loc.file - 1);
            if (!l || !l->is_string()) {
                name = "<unknown file>";
            }
            else {
                l->as_string(name);
            }
        }
        print_error("cannot generate code: ", err.msg, " at ", name, ":", loc.line, ":", loc.col);
        return 1;
    }
    cout << g.code.out();
    return 0;
}

void nan() {
    constexpr utils::binary::SingleFloat f1 = 2143289344u;               // qNAN
    constexpr utils::binary::SingleFloat f2 = 0x80000000 | 2143289344u;  // -qNAN
    constexpr auto mask = f1.exponent_mask;
    constexpr bool is_nan = f1.is_nan();
    constexpr bool is_quiet_nan = f1.is_quiet_nan();
    constexpr bool is_nan2 = f2.is_nan();
    constexpr bool sign = f1.sign();
    constexpr bool sign2 = f2.sign();
    constexpr bool test1 = f2.exponent() == f2.exponent_max;
    constexpr auto frac_value = f1.fraction();
    constexpr auto cmp = f1 == f2;
    constexpr auto cmp2 = f1.to_float() == f2.to_float();
    constexpr auto cmp3 = f1.to_float() == f1.to_float();
    constexpr auto cmp4 = f1.to_int() == f1.to_int();
    constexpr utils::binary::DoubleFloat dl = 9221120237041090560;
    constexpr auto v = dl.biased_exponent();
    constexpr auto v2 = utils::binary::DoubleFloat{0}.biased_exponent();
    constexpr auto v3 = f1.is_canonical_nan();
    constexpr auto v4 = f1.is_arithmetic_nan();
    constexpr auto v5 = f2.is_denormalized();
    constexpr auto v6 = f2.is_normalized();
    constexpr auto f3 = [&] {
        auto copy = f1;
        copy.make_quiet_signaling();
        return copy;
    }();
    fexcept_t pre, post1, post2;
    std::fegetexceptflag(&pre, FE_ALL_EXCEPT);
    constexpr auto sNAN = f3.to_float() + f3.to_float();
    std::fegetexceptflag(&post1, FE_ALL_EXCEPT);
    std::fesetexceptflag(&pre, FE_ALL_EXCEPT);
    auto qNAN = f1.to_float() + f1.to_float();
    std::fegetexceptflag(&post2, FE_ALL_EXCEPT);
    utils::binary::SingleFloat fsNAN = sNAN, fqNAN = qNAN;
}

int main(int argc, char** argv) {
    Flags flags;
    return utils::cmdline::templ::parse_or_err<std::string>(
        argc, argv, flags, [](auto&& str, bool err) { cerr << str; },
        [](Flags& flags, utils::cmdline::option::Context& ctx) {
            return Main(flags, ctx);
        });
}
