/*license*/
#pragma once
#include "../node/nodes.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

// 条件式。`find <Kind> { ... }` の中身で、ebmgen のクエリ
// (`Statement { body.kind == "IF_STATEMENT" }`) に当たるもの。
//
//   find Field { type.kind == IntType and type.bit_size == 8 }
//   find MemberAccess { base.kind == Self }
//   find Ident { @Resolution.target.kind == Field }
//   find Format { body.elements[0].kind == Field }
//   find Any { line == 52 }
//
// 綴りの規則:
//
//   path    名前を `.` で辿る。`->` も同じ意味 (EBM の綴りで書いても通る)。
//           `[n]` は並びの n 番目、`.length` は並びの長さ。
//   頭      フィールド名のほか、どのノードにもある擬似フィールドが使える:
//           `id` `kind` `line` `col` `orphan` `text` (綴り直したもの)
//   `@表名` side table を引く。`@Resolution.target` のように続けられる。
//   比較    == != < <= > >= 。両辺が整数に見えれば数として、でなければ
//           文字列として比べる。文字列は引用符ありなしどちらでも書ける。
//           ノードを指す path は id (数) と種別名 (文字列) のどちらとも
//           比べられる — `type == 431` も `type == IntType` も通る。
//   論理    and or not (`&&` `||` `!` も可)、括弧。
//   比較を書かない path はそれ自体が条件になる (辿れれば真)。
//
// 値の綴りは `p` が出すものと同じ。印字と絞り込みで別の綴りを覚えなくて済む
// ように、どちらも node/printer.h の scalar_text を通す。

namespace brgen::nast::query {

    struct Session;

    struct Filter;
    using FilterPtr = std::shared_ptr<const Filter>;

    // 構文誤りは err に理由を入れて空を返す。
    FilterPtr parse_filter(std::string_view text, std::string& err);

    // その id が条件に当てはまるか。
    bool matches(const Session& s, std::uint32_t id, const Filter& e);

}  // namespace brgen::nast::query
