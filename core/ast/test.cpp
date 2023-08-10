#include "test_component.h"
#include <fstream>
using namespace brgen;

int main() {
    auto f = test_ast();
    Debug d;
    size_t failed = 0;
    d.array([&](auto&& field) {
        for (auto& out : f) {
            auto o = out.get();
            if (!o) {
                field([&](Debug& d) {
                    d.null();
                });
                failed++;
                continue;
            }
            field([&](Debug& d) {
                o->get()->debug(d);
            });
        }
    });
    save_result(d, "./test/ast_test_result.json");
    if (failed == 0) {
        cerr << "PASS";
    }
    else {
        cerr << failed << " test failed";
    }
}