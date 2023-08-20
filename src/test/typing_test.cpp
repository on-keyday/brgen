/*license*/
#include "ast_test_component.h"
#include <core/typing/typing.h>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto& a, File* input, FileSet& fs) {
        auto result = typing::typing_with_error(a);
        if (!result) {
            ASSERT_TRUE(::testing::AssertionFailure() << result.error());
        }
        Debug d;
        d.value(a);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("typing_test_result.json");
    return res;
}