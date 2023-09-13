/*license*/
#include <langs/cpp/cpp_lang.h>
#include "../core/ast_test_component.h"
#include <core/middle/middle.h>
#include <gtest/gtest.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_test_handler([](auto& a, auto i, auto fs) {
        LocationError warns;
        middle::apply_middle(warns, a)
            .transform_error(to_source_error(fs))
            .value();
        cpp_lang::Context ctx;
        ctx.config.test_main = true;
        // ctx.config.insert_bit_pos_debug_code = true;
        auto w = cpp_lang::entry(ctx, a).transform_error(to_source_error(fs)).value();
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
    save_result("cpp_test_result.json");
    return res;
}
