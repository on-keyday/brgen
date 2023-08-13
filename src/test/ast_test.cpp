#include "ast_test_component.h"
#include <fstream>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto prog, auto& i, auto name) {
        Debug d;
        d.value(prog);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS() == 0) {
        save_result("ast_test_result.json");
    }
    return 1;
}
