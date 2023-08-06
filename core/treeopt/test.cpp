/*license*/
#include "../ast/test_component.h"
#include "call_extract.h"

void test_call_extract(AstList& c) {
    for (auto& f : c) {
        if (auto got = f.get()) {
            auto& f = *got;
            vm::CallHolder h;
            vm::extract_call(h, f);
            ast::Debug d;
            f->debug(d);
            cerr << d.buf << "\n";
        }
    }
}

int main() {
    auto c = test_ast(false);
    test_call_extract(c);
}
