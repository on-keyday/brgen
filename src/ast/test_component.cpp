/*license*/
#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>
#include "test_component.h"

auto& cerr = utils::wrap::cerr_wrap();

static std::optional<std::shared_ptr<ast::Program>> test_file(std::string_view file_name) {
    utils::file::View view;
    if (!view.open(file_name)) {
        cerr << utils::wrap::packln(file_name, " cannot open");
        return std::nullopt;
    }
    ast::Context ctx;
    auto seq = utils::make_ref_seq(view);
    std::shared_ptr<ast::Program> prog;
    auto err = ctx.enter_stream(seq, 1, [&](ast::Stream& s) {
        prog = ast::parse(s);
    });
    if (err != std::nullopt) {
        cerr << err->to_string(file_name) + "\n";
        return std::nullopt;
    }
    ast::Debug debug;
    prog->debug(debug);
    cerr << debug.buf + "\n";
    return prog;
}

AstList test_ast() {
    AstList f;
    f.push_back(std::async(std::launch::async, test_file, "./src/step/step1.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/step/step2.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/step/step3.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/step/step4.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/step/step5.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/step/step6.bgn"));
    return f;
}
