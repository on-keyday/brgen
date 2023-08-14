/*license*/
#include "ast_test_component.h"
#include <core/typing/typing.h>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto& a, Input& input, std::string_view file) {
        ::testing::AssertionResult result = ::testing::AssertionSuccess();
        auto err = typing::typing_with_error(a, input);
        if (err) {
            result = ::testing::AssertionFailure() << *err;
        }
        ASSERT_TRUE(result);
        Debug d;
        d.value(a);
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("typing_test_result.json");
    return res;
}