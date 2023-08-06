/*license*/
#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>
#include <atomic>
#include <fstream>
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

std::string test() {
    std::vector<std::future<std::optional<std::unique_ptr<ast::Program>>>> f;
    f.push_back(std::async(std::launch::async, test_file, "./src/ast/step/step1.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/ast/step/step2.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/ast/step/step3.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/ast/step/step4.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/ast/step/step5.bgn"));
    f.push_back(std::async(std::launch::async, test_file, "./src/ast/step/step6.bgn"));
    ast::Debug d;
    d.array([&](auto&& field) {
        for (auto& out : f) {
            auto o = out.get();
            if (!o) {
                break;
            }
            field([&](ast::Debug& d) {
                o->get()->debug(d);
            });
        }
    });
    return d.buf;
}

int main() {
    auto result = test();
    if (failed == 0) {
        std::ofstream ofs("./test/ast_test_result.json");
        ofs << result;
        cerr << "PASS";
    }
    else {
        cerr << failed << " test failed";
    }
}