/*license*/
#define AST_TEST_COMPONENT_EXPORTS
#include <core/ast/stream.h>
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>
#include "ast_test_component.h"
#include <core/ast/parse.h>
#include <fstream>
#include <gtest/gtest.h>
#include <env/env_sys.h>
#include <env/env.h>
#include <wrap/exepath.h>
using namespace brgen;
auto& cerr = utils::wrap::cerr_wrap();
namespace brgen::ast {

    ::testing::AssertionResult is_Index(either::expected<lexer::FileIndex, std::error_code>& v) {
        if (v) {
            return ::testing::AssertionSuccess();
        }
        else {
            return ::testing::AssertionFailure() << "with error " << v.error().message();
        }
    }

    ::testing::AssertionResult isNotError(std::optional<SourceEntry>& v, FileSet& files, lexer::FileIndex index) {
        if (!v) {
            return ::testing::AssertionSuccess();
        }
        else {
            return ::testing::AssertionFailure() << v->to_string();
        }
    }

    struct FileTest : ::testing::Test {
        FileSet files;

        void SetUp() override {
            std::string base_path;
            std::map<std::string, std::string> p;
            p["BASE_PATH"] = utils::env::sys::env_getter().get_or<std::string>("BASE_PATH", ".");
            utils::env::expand(base_path, "${BASE_PATH}/example/ast_step/", utils::env::expand_map<std::string>(p));
            auto add_file = [&](const char* file_name, lexer::FileIndex expect) {
                auto index = files.add_file(base_path + file_name);
                ASSERT_TRUE(is_Index(index));
                GTEST_ASSERT_EQ(*index, expect);
            };
            add_file("step1.bgn", 1);
            add_file("step2.bgn", 2);
            add_file("step3.bgn", 3);
            add_file("step4.bgn", 4);
            add_file("step5.bgn", 5);
            add_file("step6.bgn", 6);
            add_file("step7.bgn", 7);
            add_file("step8.bgn", 8);
            add_file("step9.bgn", 9);
        }
    };

    struct AstTest : FileTest, ::testing::WithParamInterface<lexer::FileIndex> {
    };

    static Continuation handler;

    TEST_P(AstTest, AstParseTest) {
        ast::Context ctx;
        auto input = files.get_input(GetParam());
        ASSERT_TRUE(::testing::AssertionResult(bool(input)) << input->path());
        auto prog = ctx.enter_stream(input, [&](ast::Stream& s) {
                           auto p = ast::Parser{s};
                           return p.parse();
                       })
                        .transform_error(to_source_error(files))
                        .transform_error(src_error_to_string);
        ASSERT_TRUE(::testing::AssertionResult(prog) << prog.error_or(""));
        if (handler) {
            ASSERT_NO_THROW((handler(*prog, input, files)));
        }
    }

    INSTANTIATE_TEST_SUITE_P(AstTests,
                             AstTest,
                             testing::Values(1, 2, 3, 4, 5, 6, 7, 8, 9));
}  // namespace brgen::ast

/*
std::shared_ptr<ast::Program> test_file(std::string_view name, Continuation cont) {
    ;
    file_name += name.data();
    brgen::FileList files;
    auto fd = files.add(file_name);
    if (auto code = std::get_if<std::error_code>(&fd)) {
        cerr << ("error:" + code->message());
        return nullptr;
    }
    ast::Context ctx;
    auto fp = std::get<brgen::lexer::FileIndex>(fd);
    auto input = files.get_input(fp);
    std::shared_ptr<ast::Program> prog;
    auto copy = *input;
    auto err = ctx.enter_stream(std::move(copy), [&](ast::Stream& s) {
        auto p = ast::Parser{s};
        prog = p.parse();
    });
    if (err != std::nullopt) {
        cerr << (err->to_string(file_name) + "\n");
        return nullptr;
    }
    auto path = *files.get_path(fp);
    cont(prog, *input, path.generic_string());
    return prog;
}*/

void set_test_handler(Continuation cont) {
    brgen::ast::handler = cont;
}

static std::vector<JSONWriter> debug;

void add_result(JSONWriter&& d) {
    debug.push_back(std::move(d));
}

void save_result(const char* file) {
    std::filesystem::path path = utils::wrap::get_exepath<std::u8string>();
    path = path.parent_path() / file;
#ifdef __linux__
    fprintf(stderr, "log file saved to %s", path.c_str());
#endif
    std::ofstream ofs(path.c_str());
    JSONWriter d;
    d.value(debug);
    ofs << d.out();
}