/*license*/
#include "c_lang.h"
#include "core/ast/test_component.h"

int main() {
    auto asts = test_ast(false);
    for (auto& g : asts) {
        auto got = g.get();
        if (got) {
            writer::TreeWriter w{nullptr};
            c_lang::entry(w, *got);
            cerr << w.code().out() << "\n";
        }
    }
}