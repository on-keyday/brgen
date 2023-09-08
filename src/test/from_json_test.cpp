/*license*/
/*license*/
#include "ast_test_component.h"
#include <core/middle/middle.h>
#include <gtest/gtest.h>
#include <core/ast/from_json.h>
#include <json/parse.h>
using namespace brgen;

int main(int argc, char** argv) {
    set_test_handler([](auto a, auto in, auto fs) {
        LocationError warns;
        middle::apply_middle(warns, a)
            .transform_error(to_source_error(fs))
            .value();
        Debug d;
        d.value(a);
        auto parsed = utils::json::parse<ast::JSON>(d.out());
        ast::from_json(parsed).transform_error(to_source_error(fs)).value();
    });
    ::testing::InitGoogleTest(&argc, argv);
    auto res = RUN_ALL_TESTS();
    return res;
}
