/*license*/
#include "../ast/test_component.h"
#include "call_extract.h"

void test_call_extract(AstList& c) {
    ast::Debug d;
    d.array([&](auto&& field) {
        for (auto& f : c) {
            if (auto got = f.get()) {
                auto& f = *got;
                treeopt::CallHolder h;
                treeopt::extract_call(h, f);
                field([&](ast::Debug& d) {
                    f->debug(d);
                });
            }
        }
    });
    cerr << d.buf << "\n";
    save_result(d, "./test/treeopt_test_result.json");
}

int main() {
    auto c = test_ast(false);
    test_call_extract(c);
}
