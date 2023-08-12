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
using namespace brgen;
auto& cerr = utils::wrap::cerr_wrap();
namespace brgen::ast {
    struct AstTest : ::testing::Test {
        std::shared_ptr<ast::Program> program;
    };

    TEST_F(AstTest, ParserTest) {
    }
}  // namespace brgen::ast

std::shared_ptr<ast::Program> test_file(std::string_view name, Continuation cont) {
    std::string file_name = "./test/ast_step/";
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
}

AstList test_ast(Continuation cont) {
    AstList f;
    f.push_back(std::async(std::launch::async, test_file, "step1.bgn", cont));
    f.push_back(std::async(std::launch::async, test_file, "step2.bgn", cont));
    f.push_back(std::async(std::launch::async, test_file, "step3.bgn", cont));
    f.push_back(std::async(std::launch::async, test_file, "step4.bgn", cont));
    f.push_back(std::async(std::launch::async, test_file, "step5.bgn", cont));
    f.push_back(std::async(std::launch::async, test_file, "step6.bgn", cont));
    f.push_back(std::async(std::launch::async, test_file, "step7.bgn", cont));
    return f;
}

void save_result(Debug& d, const char* file) {
    std::ofstream ofs(file);
    ofs << d.buf;
}