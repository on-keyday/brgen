/*license*/
#include "langs/go/go_lang.h"
#include "../ast_test_component.h"
#include "core/typing/typing.h"
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_handler([](auto& a, auto i, auto fs) {
        typing::typing_with_error(a).transform_error(to_source_error(fs)).value();
        go_lang::Context ctx;
        ctx.config.test_main = true;
        auto w = go_lang::entry(ctx, a).transform_error(to_source_error(fs)).value();
        Debug d;
        {
            auto field = d.object();
            field("sections", [&] {
                auto field = d.array();
                w->dump_section([&](std::string_view path) {
                    field(path);
                });
            });
            d.set_utf_escape(true);
            field("code", w->flush());
        }
        add_result(std::move(d));
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("go_test_result.json");
    return res;
}
