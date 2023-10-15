/*license*/
/*license*/
#include "ast_test_component.h"
#include <core/middle/middle.h>
#include <gtest/gtest.h>
#include <core/ast/json.h>
#include <json/parse.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_test_handler([](auto a, auto in, auto fs) {
        LocationError warns;
        middle::apply_middle(warns, a)
            .transform_error(to_source_error(fs))
            .value();
        ast::JSONConverter m;
        m.encode(a);
        auto base = m.obj.out();
        add_result(std::move(m.obj));
        auto parsed = utils::json::parse<ast::JSON>(base);
        auto d = m.decode(parsed)
                     .transform_error(to_source_error(fs))
                     .value();
        m.encode(d);
        ASSERT_EQ(base, m.obj.out());
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    save_result("from_json_test_result.json");
    return res;
}
