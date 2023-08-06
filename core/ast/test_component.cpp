/*license*/
#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>
#include "test_component.h"

auto& cerr = utils::wrap::cerr_wrap();

static std::optional<std::shared_ptr<ast::Program>> test_file(std::string_view name, bool debug) {
    utils::file::View view;
    std::string file_name = "./core/step/";
    file_name += name.data();
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
    if (debug) {
        ast::Debug debug;
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
