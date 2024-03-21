/*license*/
#include "ast_test_component.h"
#include <core/middle/typing.h>
#include <core/ast/traverse.h>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_test_handler([](auto& a, File* input, FileSet& fs) {
        brgen::LocationError err;
        middle::analyze_type(a, err).transform_error(to_source_error(fs)).value();
        JSONWriter d;
        d.value(*a);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("typing_test_result.json");
    return res;
}