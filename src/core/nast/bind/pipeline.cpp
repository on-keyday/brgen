/*license*/
#include "pipeline.h"

#include "binder.hpp"
#include "endian_scope.hpp"
#include "evaluator.hpp"
#include "import_resolver.hpp"
#include "receiver.hpp"
#include "requires.hpp"
#include "scope_resolver.hpp"
#include "type_size.hpp"
#include "typer.hpp"
#include "union_layout.hpp"

namespace brgen::nast {

    const char* describe(AnalyzeResult r) {
        switch (r) {
            case AnalyzeResult::ok:
                return "ok";
            case AnalyzeResult::cannot_open:
                return "cannot open file";
            case AnalyzeResult::cannot_read:
                return "cannot read file";
            case AnalyzeResult::parse_failed:
                return "parse error";
        }
        return "unknown";
    }

    std::string first_error(Program& p) {
        std::string msg;
        to_source_error(p.files)(p.err).for_each_error([&](auto& m, bool warn) {
            if (!warn && msg.empty()) {
                msg = std::string(m);
            }
        });
        return msg;
    }

    namespace {
        // until より後ろは回さない。
        bool wants(const AnalyzeOption& opt, Stage s) {
            return static_cast<int>(s) <= static_cast<int>(opt.until);
        }

        void done(const AnalyzeOption& opt, Stage s) {
            if (opt.on_stage_done) {
                opt.on_stage_done(s);
            }
        }
    }  // namespace

    AnalyzeResult analyze(Program& p, std::string_view path, const AnalyzeOption& opt) {
        auto loaded = p.files.add_file(std::string(path));
        if (!loaded) {
            return AnalyzeResult::cannot_open;
        }
        return analyze_loaded(p, *loaded, opt);
    }

    AnalyzeResult analyze_loaded(Program& p, lexer::FileIndex file, const AnalyzeOption& opt) {
        p.main_file = file;
        auto* input = p.files.get_input(file);
        if (!input) {
            return AnalyzeResult::cannot_read;
        }
        done(opt, Stage::open);
        if (!wants(opt, Stage::parse)) {
            return AnalyzeResult::ok;
        }

        Context ctx;
        auto parsed = ctx.enter_stream(input, [&](Stream& s) {
            return parse(p.arena, s, &p.err, opt.parse);
        });
        if (!parsed) {
            // 呼び出し側が診断をまとめて扱えるよう、失敗も同じ入れ物に積む。
            auto& from = parsed.error();
            p.err.locations.insert(p.err.locations.end(), from.locations.begin(), from.locations.end());
            done(opt, Stage::parse);
            return AnalyzeResult::parse_failed;
        }
        p.root = *parsed;
        done(opt, Stage::parse);

        // ここから先は診断を積むだけで止まらない。壊れた入力でも、通った
        // ところまでの型や解決結果は使いものになる (lsp / dump がそれを見る)。
        if (!wants(opt, Stage::import_)) {
            p.modules = {p.root};
            return AnalyzeResult::ok;
        }
        // import は束縛より先。読み込んだ Module も同じ扱いで回す。
        bind::ImportResolver importer{p.arena, p.tables, p.files, p.err, opt.parse};
        importer.resolve(p.root);
        p.modules = importer.modules;
        p.stats.imports_resolved = importer.resolved;
        p.stats.imports_failed = importer.failed;
        done(opt, Stage::import_);

        if (!wants(opt, Stage::bind)) {
            return AnalyzeResult::ok;
        }
        bind::ScopeResolver resolver{p.arena, p.tables, p.err};
        for (auto& mod : p.modules) {
            bind::Binder binder{p.arena, p.err, p.tables};
            binder.bind(mod);
            resolver.resolve(mod);
        }
        p.stats.names_resolved = resolver.resolved;
        p.stats.names_unresolved = resolver.unresolved;
        // レシーバの実体化は名前解決の後。解決先が決まっていないと、その参照が
        // field を指すのか、関数の中のローカルなのかが分からない。
        bind::MaterializeReceiver receiver{p.arena, p.tables};
        for (auto& mod : p.modules) {
            receiver.run(mod);
        }
        p.stats.receivers = receiver.materialized;
        done(opt, Stage::bind);

        if (!wants(opt, Stage::type)) {
            return AnalyzeResult::ok;
        }
        // 型付けは名前解決の後。Reference の型は解決先から取る。
        bind::Typer typer{p.arena, p.tables, p.err};
        for (auto& mod : p.modules) {
            typer.run(mod);
        }
        done(opt, Stage::type);

        if (!wants(opt, Stage::evaluate)) {
            return AnalyzeResult::ok;
        }
        // 定数畳み込みは型に依存しない (Resolution だけ使う) が、段としては後ろ。
        bind::Evaluator evaluator{p.arena, p.tables, p.err};
        for (auto& mod : p.modules) {
            evaluator.run(mod);
        }
        p.stats.constants = evaluator.evaluated;
        done(opt, Stage::evaluate);

        if (!wants(opt, Stage::endian)) {
            return AnalyzeResult::ok;
        }
        // バイト順は書かれた位置で決まるので、木の並び順に歩く。
        bind::EndianScope endian_scope{p.arena, p.tables, p.err};
        endian_scope.run(p.modules);
        p.stats.endian_fields = endian_scope.analyzed;
        p.stats.endian_dynamic = endian_scope.dynamic;
        done(opt, Stage::endian);

        if (!wants(opt, Stage::size)) {
            return AnalyzeResult::ok;
        }
        // 幅は型の性質なので、型付けと畳み込みが済んでいれば決まる。
        bind::SizeAnalysis sizes{p.arena, p.tables, p.err};
        sizes.run();
        p.stats.sized_types = sizes.analyzed;
        done(opt, Stage::size);

        if (!wants(opt, Stage::require)) {
            return AnalyzeResult::ok;
        }
        bind::RequiresInference requires_{p.arena, p.tables, typer};
        requires_.run(p.modules);
        done(opt, Stage::require);

        if (!wants(opt, Stage::layout)) {
            return AnalyzeResult::ok;
        }
        bind::UnionLayoutAnalysis layout{p.arena, p.tables, typer, p.err};
        layout.run();
        done(opt, Stage::layout);

        return AnalyzeResult::ok;
    }

}  // namespace brgen::nast
