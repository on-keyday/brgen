#include "../backend/knobs.hpp"
#include "../backend/defaults.hpp"
#include "core/common/file.h"
#include <print>
struct LanguageConfig {
    static constexpr auto lang_name = "test";
};

int main() {
    brgen::nast::Arena a;
    brgen::nast::backend::Knobs knobs;
    brgen::nast::backend::BaseContext<brgen::nast::CodeWriter> bc{.a = a, .n = knobs};
    using Context = brgen::nast::backend::Context<brgen::nast::CodeWriter, LanguageConfig>;
    LanguageConfig l;
    auto conf = bc.to_context(l);
    using namespace brgen::nast;
    knobs.bind_Module(conf, [](Context& c, Node<Module> m) -> brgen::result<CodeWriter> {
        CodeWriter w;
        w.writeln("module {");
        for (auto& stmt : m.ref(c.b.a)->statements) {
            MAYBE(x, c.visit(stmt));
            w.write(std::move(x));
        }
        w.writeln("}");
        return w;
    });
    auto invoked = conf.b.visit(mod.id());
    if (!invoked) {
        brgen::FileSet fs;
        auto src = brgen::to_source_error(fs)(invoked.error());
        std::print("{}", src.to_string());
    }
    else {
        std::print("{}", invoked->to_string());
    }
}