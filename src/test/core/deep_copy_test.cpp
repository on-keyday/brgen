/*license*/
#include <gtest/gtest.h>
#include <filesystem>
#include <tool/src2json/test.h>
#include <core/ast/node/deep_copy.h>
#include <unordered_map>
#include <env/env.h>
#include <env/env_sys.h>
namespace fs = std::filesystem;

std::shared_ptr<brgen::ast::Program> load(const fs::path& path) {
    brgen::FileSet fs;
    auto ok = fs.add_file(path.generic_u8string());
    if (!ok) {
        ok.throw_error();
    }
    std::shared_ptr<brgen::ast::Program> prog;
    [&] {
        ASSERT_EQ(*ok, 1);
        ASSERT_NO_THROW(prog = brgen::test::src2json(fs));
    }();
    return prog;
}

auto load_paths() {
    std::vector<fs::path> paths;
    std::map<std::string, std::string> p;
    p["BASE_PATH"] = futils::env::sys::env_getter().get_or<std::string>("BASE_PATH", ".");
    std::string base_path;
    futils::env::expand(base_path, "${BASE_PATH}/example/", futils::env::expand_map<std::string>(p));
    auto recursive = [&](auto&& recursive, const fs::path& path) -> void {
        fs::directory_iterator it, end;
        ASSERT_NO_THROW(it = fs::directory_iterator(path));
        for (; it != end; ++it) {
            if (it->is_directory()) {
                recursive(recursive, it->path());
            }
            if (it->is_regular_file()) {
                if (it->path().extension() != ".bgn") {
                    continue;
                }
                paths.push_back(it->path());
            }
        }
    };
    recursive(recursive, base_path);
    return paths;
}

struct DeepCopyTest : public ::testing::TestWithParam<fs::path> {
};

INSTANTIATE_TEST_SUITE_P(
    DeepCopyTestSuite,
    DeepCopyTest,
    ::testing::ValuesIn(load_paths()));

TEST_P(DeepCopyTest, DeepCopy) {
    auto path = GetParam();
    auto prog = load(path);
    ASSERT_NE(prog, nullptr);
    using NodeMap = std::unordered_map<std::shared_ptr<brgen::ast::Node>, std::shared_ptr<brgen::ast::Node>>;
    using ScopeMap = std::unordered_map<std::shared_ptr<brgen::ast::Scope>, std::shared_ptr<brgen::ast::Scope>>;
    ASSERT_TRUE((brgen::ast::test::test_single_deep_copy<NodeMap, ScopeMap>(prog, [](auto&& a, auto&& b, const char* which, size_t index) {
        if (index == -1) {
            fprintf(stderr, "Error at %s\n", which);
        }
        else {
            fprintf(stderr, "Error at %s[%zu]\n", which, index);
        }
    })));
}