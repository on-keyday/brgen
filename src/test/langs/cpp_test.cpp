/*license*/
#include "langs/cpp/cpp_lang.h"
#include "../ast_test_component.h"
#include "core/typing/typing.h"
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto& a, auto& i, auto) {
        auto err = typing::typing_with_error(a, i);
        if (err) {
            ASSERT_TRUE(::testing::AssertionFailure() << *err);
        }
        writer::TreeWriter w{"global", nullptr};
        cpp_lang::entry(w, a);
        Debug d;
        d.string(w.out().out());
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("cpp_test_result.json");
    return res;
}
