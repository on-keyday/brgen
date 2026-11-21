/*license*/
#pragma once
#include "../node/nodes.h"

// レシーバの実体化。名前解決のあと、field を指す裸の `Reference` を
// `MemberAccess{base: Self, member: 同じ Ident}` に差し替える。
//
// **なぜ木を書き換えるか。** 原文にレシーバは書かれていないが、生成コードでは
// `t.Len` のように要る。以前は綴る側が「参照の解決先が Field なら前置する」で
// 足していたが、その判定はスコープの情報を持たない側での再導出で、実際に
// 一度外している (関数の中で宣言された `y :u8` も同じ `Field` なので
// `(*this).y` になっていた)。判定はスコープを知っている側で 1 度だけ行い、
// 結果を木に残す。
//
// 「書き換えない」は lowering の規約 (lowering/lowering.hpp、
// docs/exit_and_reversibility.md の規則 1) で、フロントエンドの禁止ではない。
// parse も引数の `x = y` を NamedArgument に、文位置の `config.X = v` を
// Metadata に書き換えている。
//
// **裸と修飾の区別は残る。** `Self` は .bgn の構文に無く、この段以外では
// 作られないので、`available(x)` は base が Self、`available(a.b.x)` は base が
// それ以外、で見分けられる。EBM が区別を失ったのは裸も修飾も同じ
// `MEMBER_ACCESS{base: SELF}` にして印を残さなかったからで、実体化そのものが
// 原因ではない (ebmgen は変換前の AST を見に戻っている)。
//
// **Ident は作り直さず持ち回す。** `Resolution` 表のキーは `Reference` ではなく
// 中の `Ident` なので、同じノードを `MemberAccess.member` に移せば解決先は
// そのまま生きる。分岐の中で宣言された field と format 直下の union field は
// 同じ名前で別の宣言を指すので、名前で引き直すとこの区別が消える。
//
// **腕の経路までは作らない。** `Self` が言うのは「どの format のインスタンスか」
// までで、分岐の field を腕の struct に入れるかどうかは格納戦略 =
// バックエンドの選択 (docs/size_and_lowering.md)。

namespace brgen::nast::bind {

    struct MaterializeReceiver {
        Arena& a;
        SideTables& tables;

        // 差し替えた参照の数。計測用。
        std::size_t materialized = 0;

        void run(Node<Module> mod);
    };

}  // namespace brgen::nast::bind
