#include "../parse/parse.h"
#include <core/common/file.h>
#include <chrono>
#include <print>
#include <string>
#include <vector>
using namespace brgen::nast;
using clock_ = std::chrono::steady_clock;
static double ms(std::chrono::nanoseconds d) { return std::chrono::duration<double, std::milli>(d).count(); }
int main(int argc, char** argv) {
    std::vector<std::string> paths;
    for (int i = 1; i < argc; i++) paths.push_back(argv[i]);
    std::chrono::nanoseconds lex{}, full{};
    std::size_t nodes = 0;
    for (int r = 0; r < 3; r++) {
        std::chrono::nanoseconds l{}, fl{};
        for (auto& p : paths) {
            { brgen::FileSet fs; auto i = fs.add_file(p); if (!i) continue; auto* f = fs.get_input(*i);
              Arena a; Context ctx; auto t = clock_::now();
              (void)ctx.enter_stream(f, [&](Stream& s) -> Node<Module> { while (!s.eos()) s.consume(); return nullref; });
              l += clock_::now() - t; }
            { brgen::FileSet fs; auto i = fs.add_file(p); if (!i) continue; auto* f = fs.get_input(*i);
              Arena a; brgen::LocationError err; Context ctx; auto t = clock_::now();
              (void)ctx.enter_stream(f, [&](Stream& s) { return parse(a, s, &err, {}); });
              fl += clock_::now() - t; if (r == 0) nodes += a.node_count(); }
        }
        if (r == 0 || l < lex) lex = l;
        if (r == 0 || fl < full) full = fl;
    }
    std::println("lex only    {:8.1f} ms", ms(lex));
    std::println("lex + parse {:8.1f} ms", ms(full));
    std::println("parse alone {:8.1f} ms   ({} nodes)", ms(full - lex), nodes);
}
