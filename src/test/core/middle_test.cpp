/*license*/
#include "ast_test_component.h"
#include <core/middle/middle_test.h>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_test_handler([](auto a, auto in, auto fs) {
        LocationError warns;
        middle::test::apply_middle(warns, a)
            .transform_error(to_source_error(fs))
            .value();
        JSONWriter d;
        d.value(a);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("middle_test_result.json");
    return res;
}
