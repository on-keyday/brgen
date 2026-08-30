#include "../backend/knobs.hpp"
#include "../backend/defaults.hpp"
#include "core/common/file.h"
#include "../parse/unparse.h"
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
        {
            auto b = w.with_loc_scope(m);
            w.writeln("module {");
            {
                auto i = w.indent_scope();
                for (auto& stmt : m.ref(c.arena())->statements) {
                    MAYBE(x, c.visit(stmt));
                    w.write(std::move(x));
                }
            }
            w.writeln("}");
        }
        return w;
    });
    knobs.bind_Return(conf, [](Context& c, Node<Return> m) -> brgen::result<CodeWriter> {
        return CODELINE(brgen::nast::unparse_writer(c.arena(), m));
    });
    auto m = a.make<Module>();
    auto r = a.make<Return>();
    m->statements.push_back(r);
    auto lit = a.make<IntLiteral>();
    lit->value = "0";
    r->expr = lit;
    auto invoked = conf.b.visit(m.id());
    if (!invoked) {
        brgen::FileSet fs;
        auto src = brgen::to_source_error(fs)(invoked.error());
        std::print("{}", src.to_string());
    }
    else {
        std::print("{}", invoked->to_string());
    }
}