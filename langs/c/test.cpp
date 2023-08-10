/*license*/
#include "c_lang.h"
#include "core/ast/test_component.h"
using namespace brgen;

int main() {
    auto asts = test_ast(false);
    for (auto& g : asts) {
        auto got = g.get();
        if (got) {
            writer::TreeWriter w{"global", nullptr};
            c_lang::entry(w, *got);
            cerr << w.code().out() << "\n";
        }
    }
}