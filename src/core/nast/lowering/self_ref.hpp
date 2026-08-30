/*license*/
#pragma once
#include "lowering.hpp"

// 受け手 (this / self / レシーバ名) の扱い。
//
// **原文の木に受け手は無い。** `data :[len]u8` の `len` は裸の Reference で、
// 解決先が Field であることが Resolution 表に載っているだけ。生成コードでは
// `t.Len` のように受け手を付けないといけないが、「ここに要る」という印が
// 木のどこにも無い。
//
// EBM は変換の時点で `MEMBER_ACCESS{base: SELF}` に実体化して、綴りだけを
// 言語側に残している。ここも同じ形にする — 受け手が要ることは共通で、
// どう綴るかは言語ごと (`MEMBER_ACCESS` は共通化不適と測定済みのグループ)。
//
// **原文の式は書き換えない。** 複製すると中の名前が Resolution 表に載って
// いない別ノードになり、解決先を失う。原文から来た式については
// `receiver_field` で「その参照は受け手が要るか」を答えるだけにして、
// 実際に前置するのは綴る側。lowering が新しく作る参照 (`field_access`) には
// 最初から受け手を付ける。

namespace brgen::nast::lowering {

    // その field を指す式。`self.<名前>` の形で、名前の解決先も表に入れる。
    // lowering が field を指したいときはこれを使う。
    //
    // owner は受け手の型を付けるためだけのもの。field は持ち主を指していない
    // (FormatState が format -> fields の向きに持つ) ので呼ぶ側が渡す。
    // 渡さなくても綴りは出る。
    Node<Expr> field_access(Context& c, Node<Field> f, Node<Format> owner = nullref);

    // その参照が指しているのが field なら、その field。受け手が要るかどうかの
    // 判定に使う。原文の式を辿って綴る側が呼ぶ。
    Node<Field> receiver_field(Context& c, Node<Reference> ref);

}  // namespace brgen::nast::lowering
