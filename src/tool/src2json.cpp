/*license*/
#include <cmdline/template/help_option.h>
#include <cmdline/template/parse_and_err.h>
#include <wrap/cout.h>
#include <core/ast/parse.h>
#include <core/ast/json.h>
#include <wrap/iocommon.h>
#include <console/ansiesc.h>
#include <future>

struct Flags : utils::cmdline::templ::HelpOption {
    std::vector<std::string> args;
};
auto& cout = utils::wrap::cout_wrap();
auto& cerr = utils::wrap::cerr_wrap();
namespace cse = utils::console::escape;
void print_error(auto&&... msg) {
    cerr << "src2json: ";
    if (cerr.is_tty()) {
        cerr << cse::letter_color_code<9>;
    }
    cerr << "error: ";
    if (cerr.is_tty()) {
        cerr << cse::letter_color<cse::ColorPalette::white>;
    }
    (cerr << ... << msg) << "\n";
    if (cerr.is_tty()) {
        cerr << cse::color_reset;
    }
}

void print_warning(auto&&... msg) {
    cerr << "src2json: ";
    if (cerr.is_tty()) {
        cerr << cse::letter_color<cse::ColorPalette::yellow>;
    }
    cerr << "warning: ";
    if (cerr.is_tty()) {
        cerr << cse::letter_color<cse::ColorPalette::white>;
    }
    (cerr << ... << msg) << "\n";
    if (cerr.is_tty()) {
        cerr << cse::color_reset;
    }
}

auto do_parse(brgen::File* file) {
    brgen::ast::Context c;
    return c.enter_stream(file, [&](brgen::ast::Stream& s) {
        return std::make_pair(brgen::ast::Parser{s}.parse(), file);
    });
}

int Main(Flags& flags, utils::cmdline::option::Context& ctx) {
    utils::wrap::out_virtual_terminal = true;

    if (flags.args.size() == 0) {
        print_error("no input file");
        return -1;
    }

    brgen::FileSet files;
    using R = brgen::result<std::pair<std::shared_ptr<brgen::ast::Program>, brgen::File*>>;
    std::vector<std::future<R>> result;

    for (auto& name : flags.args) {
        auto ok = files.add(name);
        if (!ok) {
            if (ok.error().category() == std::generic_category()) {
                print_warning("cannot open duplicated file ", name, " at one parser");
            }
            else {
                print_warning("cannot open file ", name, " code=", ok.error());
            }
            continue;
        }
        auto input = files.get_input(*ok);
        if (!input) {
            print_warning("cannot open file  ", name);
            continue;
        }

        result.push_back(std::async(do_parse, input));
    }
    brgen::Debug d;
    bool has_error = false;
    {
        auto field = d.array();
        for (auto& r : result) {
            field([&] {
                auto field = d.object();
                brgen::ast::JSONConverter c;
                auto g = r.get().transform_error(brgen::to_source_error(files));
                if (!g) {
                    field("file", g.error().errs[0].file);
                    field("ast", nullptr);
                    field("error", g.error().to_string());
                    g.error().for_each_error([&](std::string_view msg, bool warn) {
                        if (warn) {
                            print_warning(msg);
                        }
                        else {
                            has_error = true;
                            print_error(msg);
                        }
                    });
                    return;
                }
                auto path = g->second->path().generic_u8string();
                field("file", (const char*)path.c_str());
                c.encode(g->first);
                field("ast", c.obj);
                field("error", nullptr);
            });
        }
    }
    if (!cout.is_tty() || !has_error) {
        cout << d.out();
    }
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
