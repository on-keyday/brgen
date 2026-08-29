#include "../backend/context.hpp"

int main() {
    brgen::nast::Arena a;
    brgen::nast::backend::Knobs knobs;
    brgen::nast::backend::BaseContext bc{.a = a, .n = knobs};
    struct LanguageConfig {
    } l;
    using Context = brgen::nast::backend::Context<LanguageConfig>;
    Context conf{.b = bc, .l = l};
    using namespace brgen::nast;
    knobs.bind_Module(conf, [](Context& c, Node<Module> m) {
        return CodeWriter{};
    });
}