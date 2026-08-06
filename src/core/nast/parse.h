/*license*/
#pragma once
#include "nodes.h"
#include "stream.h"

namespace brgen::nast {
    struct ParseOption {
        bool collect_comments = false;
        bool error_tolerant = false;
    };

    // 構文木は arena に積まれる。呼び出し側が arena を所有し、
    // 戻り値はその中のルートを指す Node。
    Node<Module> parse(Arena& a, Stream& stream, LocationError* err_or_warn, ParseOption option = {});
}  // namespace brgen::nast
