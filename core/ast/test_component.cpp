/*license*/
#define AST_TEST_COMPONENT_EXPORTS
#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>
#include "test_component.h"
#include <fstream>
using namespace brgen;
auto& cerr = utils::wrap::cerr_wrap();

static std::optional<std::shared_ptr<ast::Program>> test_file(std::string_view name, bool debug) {
    std::string file_name = "./core/step/";
    file_name += name.data();
    brgen::FileList files;
    auto fd = files.add(file_name);
    if (auto code = std::get_if<std::error_code>(&fd)) {
        cerr << utils::wrap::packln("error:", code->message());
        return std::nullopt;
    }
    ast::Context ctx;
    auto input = files.get_input(std::get<brgen::lexer::FileIndex>(fd));
    std::shared_ptr<ast::Program> prog;
    auto err = ctx.enter_stream(std::move(*input), [&](ast::Stream& s) {
        prog = ast::parse(s);
    });
    if (err != std::nullopt) {
        cerr << err->to_string(file_name) + "\n";
        return std::nullopt;
    }
    if (debug) {
        Debug debug;
        prog->debug(debug);
        cerr << debug.buf + "\n";
    }
    return prog;
}

AstList test_ast(bool debug) {
    AstList f;
    f.push_back(std::async(std::launch::async, test_file, "step1.bgn", debug));
    f.push_back(std::async(std::launch::async, test_file, "step2.bgn", debug));
    f.push_back(std::async(std::launch::async, test_file, "step3.bgn", debug));
    f.push_back(std::async(std::launch::async, test_file, "step4.bgn", debug));
    f.push_back(std::async(std::launch::async, test_file, "step5.bgn", debug));
    f.push_back(std::async(std::launch::async, test_file, "step6.bgn", debug));
    f.push_back(std::async(std::launch::async, test_file, "step7.bgn", debug));
    return f;
}

void save_result(Debug& d, const char* file) {
    std::ofstream ofs(file);
    ofs << d.buf;
}