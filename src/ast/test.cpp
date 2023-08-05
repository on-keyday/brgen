#include "stream.h"
#include <file/file_view.h>
#include <wrap/cout.h>
#include <future>

std::optional<std::unique_ptr<ast::Program>> test_file(std::string_view name) {
    utils::file::View view;
    if (!view.open(name)) {
        utils::wrap::cerr_wrap() << name << " cannot open";
        return std::nullopt;
    }
    ast::Context ctx;
    auto seq = utils::make_ref_seq(view);
    std::unique_ptr<ast::Program> prog;
    auto err = ctx.enter_stream(seq, 1, [&](ast::Stream& s) {
        prog = ast::parse(s);
    });
    if (err != std::nullopt) {
        utils::wrap::cerr_wrap() << "error:" << err->err_token.token << "\n"
                                 << err->src << "\n";
        return std::nullopt;
    }
    return prog;
}

int main() {
    std::vector<std::future<std::optional<std::unique_ptr<ast::Program>>>> f;
    f.push_back(std::async(test_file, "./src/ast/step/step1.bgn"));
}