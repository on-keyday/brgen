/*license*/
#include "ast_test_component.h"
#include "extract_call.h"
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto a, auto in, auto) {
        treeopt::ExtractContext h;
        treeopt::extract_call(h, a);
        Debug d;
        d.value(a);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("ast_test_result.json");
    return res;
}
