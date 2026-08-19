/*license*/
#pragma once
#include "../nodes.h"
#include "../parse.h"

#include <core/common/error.h>
#include <core/common/file.h>
#include <map>
#include <vector>

// config.import("...") が指すファイルを読んで、Import ノードにその Module を
// 結びつける (ImportResolution 表)。
//
// 元の src/core/middle/resolve_import.h に当たる段。src2json では
// resolve_import (src2json.cpp:440) が analyze_type (:492) より前に走り、
// メンバアクセスの解決は型解析側 (typing.cpp:1188) の仕事になっている。
// ここも同じ分担で、この段は「Module を繋ぐ」までしかやらない。
//
// 元の実装との違い:
//
//   - Call を Import に差し替えるのは parse で済んでいる (parse.cpp:846-866)。
//     path は文字列リテラルなのでその時点で確定する。
//   - 読み込んだ Module は同じ Arena に入れる。ImportResolution.module は
//     Node<Module> で、アリーナを跨ぐ id には意味が無いため。
//   - 同一ファイル判定は fs::equivalent ではなく FileSet の FileIndex で見る。
//     add_file が canonical 化と重複判定を済ませてくれるので、比較を持たない。
//
// 呼び出し側は resolve のあと modules を回して、Module ごとに Binder と
// ScopeResolver をかける。この段はスコープを作らない。

namespace brgen::nast::bind {

    struct ImportResolver {
        Arena& a;
        SideTables& tables;
        brgen::FileSet& files;
        brgen::LocationError& err;
        ParseOption option;

        // 読み込んだ順の Module。先頭は resolve に渡した root。
        std::vector<Node<Module>> modules;
        std::size_t resolved = 0;
        std::size_t failed = 0;

        void resolve(Node<Module> root);

        // 以下は実装の内側。
        // 今辿っている途中のファイル。循環はここに再び現れることで分かる。
        std::vector<lexer::FileIndex> stack_;
        // 読み終えたファイル。同じものを 2 回 import しても 1 回しか読まない。
        std::map<lexer::FileIndex, Node<Module>> done_;

        void walk(Node<Module> mod);
        void handle(Node<Import> import_);
    };

}  // namespace brgen::nast::bind
