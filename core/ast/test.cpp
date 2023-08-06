#include "test_component.h"
#include <fstream>

int main() {
    auto f = test_ast();
    ast::Debug d;
    size_t failed = 0;
    d.array([&](auto&& field) {
        for (auto& out : f) {
            auto o = out.get();
            if (!o) {
                field([&](ast::Debug& d) {
                    d.null();
                });
                failed++;
                continue;
            }
            field([&](ast::Debug& d) {
                o->get()->debug(d);
            });
        }
    });
    std::ofstream ofs("./test/ast_test_result.json");
    ofs << d.buf;
    if (failed == 0) {
        cerr << "PASS";
    }
    else {
        cerr << failed << " test failed";
    }
}