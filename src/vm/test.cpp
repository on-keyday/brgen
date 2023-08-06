/*license*/
#include "../ast/test_component.h"
#include "vm.h"

void test_vm() {
    auto c = test_ast(false);
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
    test_vm();
}