/*license*/
#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>
#include <atomic>
std::atomic_uint failed;

auto& cerr = utils::wrap::cerr_wrap();

std::optional<std::unique_ptr<ast::Program>> test_file(std::string_view file_name) {
    auto d = utils::helper::defer([&] {
        failed++;
    });
    utils::file::View view;
    if (!view.open(file_name)) {
        cerr << utils::wrap::packln(file_name, " cannot open");
        return std::nullopt;
    }
    ast::Context ctx;
    auto seq = utils::make_ref_seq(view);
    std::unique_ptr<ast::Program> prog;
    auto err = ctx.enter_stream(seq, 1, [&](ast::Stream& s) {
        prog = ast::parse(s);
    });
    if (err != std::nullopt) {
        cerr << err->to_string(file_name) + "\n";
        return std::nullopt;
    }
    d.cancel();
    ast::Debug debug;
    prog->debug(debug);
    cerr << debug.buf + "\n";
    return prog;
}

void test() {
    std::vector<std::future<std::optional<std::unique_ptr<ast::Program>>>> f;
    f.push_back(std::async(test_file, "./src/ast/step/step1.bgn"));
    f.push_back(std::async(test_file, "./src/ast/step/step2.bgn"));
    f.push_back(std::async(test_file, "./src/ast/step/step3.bgn"));
}

int main() {
    test();
    if (failed == 0) {
        cerr << "PASS";
    }
    else {
        cerr << failed << " test failed";
    }
}