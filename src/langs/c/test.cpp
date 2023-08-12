/*license*/
#include "c_lang.h"
#include "core/ast/test_component.h"
using namespace brgen;

int main(int argc, char** argv) {
    auto asts = test_ast([](auto& a, auto& i, auto) {
        writer::TreeWriter w{"global", nullptr};
        c_lang::entry(w, a);
        cerr << (w.code().out() + "\n");
    });
    for (auto& g : asts) {
        auto got = g.get();
        if (got) {
        }
    }
}
