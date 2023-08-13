#include "ast_test_component.h"
#include <fstream>
#include <gtest/gtest.h>
using namespace brgen;
std::vector<Debug> debugs;
int main(int argc, char** argv) {
    set_handler([](auto prog, auto& i, auto name) {
        Debug d;
        d.value(prog);
        debugs.push_back(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS() == 0) {
        Debug d;
        d.value(debugs);
        save_result(d, "ast_test_result.json");
    }
    return 1;
}
