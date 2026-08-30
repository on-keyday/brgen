/*license*/
#pragma once
#include "../parse/parse.h"

#include <core/common/file.h>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// 前段の駆動。.bgn を読んで木を組み、解析の段を順に回す。
//
// この列 (import -> bind/scope -> type -> evaluate -> requires -> layout) は
// どこから使っても同じでなければならないのに、以前は使う側それぞれが写して
// いた。写しは独立に育つ: 実際、往復試験だけ型付けと定数畳み込みが逆順に
// なっていた。段を足すときに直す場所もそのぶん増える。ここ 1 本にする。
//
// 使う側:
//   backend/entry.hpp, tool/dump.cpp, tool/corpus.cpp, tool/wire_test.cpp,
//   bench/coverage.cpp, bench/fn_fallible.cpp

namespace brgen::nast {

    // 段。until で「どこまで回すか」を指定する。名前解決までしか要らない
    // 道具 (bench/fn_fallible) が型付けの費用を払う理由はない。
    enum class Stage {
        open,      // ファイルを開いて読む
        parse,     // 字句 + 構文
        import_,   // import の解決 (以降は読み込んだ Module も対象)
        bind,      // 束縛 + スコープ解決
        type,      // 型付け
        evaluate,  // 定数畳み込み
        require,   // 入出力要求の推論
        layout,    // union の重ね合わせ
    };

    inline constexpr Stage last_stage = Stage::layout;

    struct AnalyzeOption {
        ParseOption parse{};
        Stage until = last_stage;
        // 段が 1 つ終わるたびに呼ぶ。計測用 (nast_corpus)。
        std::function<void(Stage)> on_stage_done{};
    };

    // 各段が数えたもの。道具が表示に使う。
    struct AnalyzeStats {
        std::size_t imports_resolved = 0;
        std::size_t imports_failed = 0;
        std::size_t names_resolved = 0;
        std::size_t names_unresolved = 0;
        std::size_t constants = 0;
    };

    // 前段の成果物一式。ノードは arena の添字でしかないので、これが生きて
    // いる間だけ有効。1 つにまとめてあるのは、arena だけ先に捨てるといった
    // 壊し方を避けるため。
    struct Program {
        FileSet files;
        Arena arena;
        SideTables tables;
        // 診断。致命的でないもの (警告、error_tolerant のときの構文誤り) も入る。
        LocationError err;
        // 入口のファイルと、import で読み込んだものも含む全 Module。
        std::vector<Node<Module>> modules;
        Node<Module> root;
        lexer::FileIndex main_file{};
        AnalyzeStats stats;
    };

    enum class AnalyzeResult {
        ok,
        cannot_open,
        cannot_read,
        parse_failed,
    };

    // 解析が始まりさえしなかったのか、木が組めなかったのかは呼び出し側で
    // 出し分けたいので、bool ではなくこれを返す。parse より後の段は診断を
    // err に積むだけで止まらない (壊れた入力も扱うため)。
    const char* describe(AnalyzeResult r);

    // path を開いて解析する。
    AnalyzeResult analyze(Program& p, std::string_view path, const AnalyzeOption& opt = {});

    // 既に p.files に入っているものを解析する。標準入力から読む場合や、
    // FileSet の設定 (utf mode など) を先に済ませたい場合はこちら。
    AnalyzeResult analyze_loaded(Program& p, lexer::FileIndex file, const AnalyzeOption& opt = {});

    // 最初の致命的な診断を文言にする。無ければ空。
    std::string first_error(Program& p);

}  // namespace brgen::nast
