#include "ast_test_component.h"
#include <fstream>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_test_handler([](auto& prog, auto i, auto& fs) {
        Debug d;
        d.value(prog);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("ast_test_result.json");
    return res;
}
