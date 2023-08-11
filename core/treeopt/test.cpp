/*license*/
#include "../ast/test_component.h"
#include "extract_call.h"
using namespace brgen;

void test_extract_call(AstList& c) {
    Debug d;
    d.array([&](auto&& field) {
        for (auto& f : c) {
            if (auto got = f.get()) {
                auto& f = *got;
                field([&](Debug& d) {
                    f->debug(d);
                });
            }
        }
    });
    cerr << d.buf << "\n";
    save_result(d, "./test/treeopt_test_result.json");
}

int main() {
    auto c = test_ast([](auto& a, auto& in) {
        treeopt::ExtractContext h;
        treeopt::extract_call(h, a);
    });
    test_extract_call(c);
}
