/*license*/
#include "../ast/test_component.h"
#include "extract_call.h"

void test_extract_call(AstList& c) {
    ast::Debug d;
    d.array([&](auto&& field) {
        for (auto& f : c) {
            if (auto got = f.get()) {
                auto& f = *got;
                treeopt::ExtractContext h;
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
    test_extract_call(c);
}
