// 既定ハンドラが何を綴るかを見る入り口。
//
//   nast_backend                                 結線だけを合成した木で確かめる
//   nast_backend [--self S] [--sep X] <file.bgn> その入力の参照と member access を綴る
//
// 後者はレシーバの綴りを見るためのもの。既定は `--self '(*this)' --sep .`
// (C++ の形)。原文の field 参照は裸なので (`len`)、綴る側がレシーバを足して
// `(*this).len` になる。継ぎ目は深さに依らず同じ
// separator なので、`--self '(*this)' --sep .` と `--self self --sep .` の
// 違いは先頭だけに出る。`--sep '->'` にすると `self->a->b` になってしまう
// ことも、ここで見える (だから参照外しは spelling 側に畳む)。
#include "../backend/defaults.hpp"
#include "../backend/knobs.hpp"
#include "../bind/pipeline.h"
#include "../node/util.h"
#include "../parse/unparse.h"
#include "core/common/file.h"
#include <print>
#include <string_view>

struct LanguageConfig {
    static constexpr auto lang_name = "test";
};

using namespace brgen::nast;
using EmitContext = backend::Context<CodeWriter, LanguageConfig>;

namespace {

    // 合成した木を 1 つ通して、custom bind と既定ハンドラの結線を確かめる。
    int smoke() {
        Arena a;
        SideTables tables;
        backend::Knobs<CodeWriter> knobs;
        backend::BaseContext<CodeWriter> bc{.a = a, .t = tables, .n = knobs};
        LanguageConfig l;
        auto conf = bc.to_context(l);
        knobs.bind_Module(conf, [](EmitContext& c, Node<Module> m) -> expected<CodeWriter> {
            CodeWriter w;
            {
                auto b = w.with_loc_scope(m);
                w.writeln("module {");
                {
                    auto i = w.indent_scope();
                    for (auto& stmt : m.ref(c.arena())->statements) {
                        MAYBE(x, c.visit(stmt));
                        w.write(std::move(x));
                    }
                }
                w.writeln("}");
            }
            return w;
        });
        knobs.bind_Return(conf, [](EmitContext& c, Node<Return> m) -> expected<CodeWriter> {
            return CODELINE(unparse_writer(c.arena(), m));
        });
        auto m = a.make<Module>();
        auto r = a.make<Return>();
        m->statements.push_back(r);
        auto lit = a.make<IntLiteral>();
        lit->value = "0";
        r->expr = lit;
        auto invoked = conf.b.visit(m.id());
        if (!invoked) {
            brgen::FileSet fs;
            std::print("{}", brgen::to_source_error(fs)(invoked.error().to_location_error(a)).to_string());
            return 1;
        }
        std::print("{}", invoked->to_string());
        return 0;
    }

    // 入力の式を既定ハンドラで綴る。左が原文の綴り、右が生成側の綴り。
    int spell(const char* path, const std::string& self, const std::string& sep) {
        Program p;
        if (auto r = analyze(p, path); r != AnalyzeResult::ok) {
            std::println(stderr, "{}: {}", path, describe(r));
            return 1;
        }
        auto& a = p.arena;
        backend::Knobs<CodeWriter> knobs;
        backend::BaseContext<CodeWriter> bc{.a = a, .t = p.tables, .n = knobs};
        LanguageConfig l;
        auto conf = bc.to_context(l);
        bc.c.Self.spelling = self;
        bc.c.MemberAccess.separator = sep;

        auto last = a.node_count();
        auto show = [&](NodeAny n) {
            auto out = conf.b.visit(n);
            if (!out) {
                return;
            }
            auto spelled = out->to_string();
            // 綴りが変わらないもの (レシーバの付かない参照) は並べても読めない
            // だけなので、変わったものだけ出す。
            auto src = unparse_node(a, n);
            if (spelled == src) {
                return;
            }
            std::println("{:<28} {}", src, spelled);
        };
        each_node<Reference>(a, last, [&](Node<Reference> r) { show(r); });
        each_node<MemberAccess>(a, last, [&](Node<MemberAccess> m) { show(m); });
        return 0;
    }

}  // namespace

int main(int argc, char** argv) {
    std::string self = "(*this)", sep = ".";
    const char* path = nullptr;
    for (int i = 1; i < argc; i++) {
        std::string_view arg = argv[i];
        if (arg == "--self" && i + 1 < argc) {
            self = argv[++i];
        }
        else if (arg == "--sep" && i + 1 < argc) {
            sep = argv[++i];
        }
        else if (arg.starts_with("--")) {
            std::println(stderr, "usage: nast_backend [--self <spelling>] [--sep <separator>] [<file.bgn>]");
            return 2;
        }
        else {
            path = argv[i];
        }
    }
    return path ? spell(path, self, sep) : smoke();
}
