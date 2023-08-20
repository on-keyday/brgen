/*license*/
#include "langs/cpp/cpp_lang.h"
#include "../ast_test_component.h"
#include "core/typing/typing.h"
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto& a, auto i, auto fs) {
        auto ok = typing::typing_with_error(a);
        if (!ok) {
            ASSERT_TRUE(::testing::AssertionFailure() << ok.error());
        }
        cpp_lang::Context ctx;
        cpp_lang::entry(ctx, a);
        ASSERT_TRUE(ctx.w);
        Debug d;
        d.string(ctx.w->flush());
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("cpp_test_result.json");
    return res;
}
