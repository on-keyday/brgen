# derive_property で chain 全段の INIT_CHECK を生成する

## 日付

- 判断時期: 2026年5月（ebm2go の inline type-assert workaround からの転換時）
- 文書化: 2026-05-24

## 判断

`transform/derive_property.cpp` の property getter/setter body 構築で、
leaf field の parent_struct から property の parent_format まで
variant containment chain を辿り、各レベルで `handle_variant_alternative` を呼んで
INIT_CHECK を block 先頭に積む (outer-most first)。

## 動機

- **write once generate any language code**: encode/decode の IndentBlock 処理は
  再帰的に各 nested block 毎に handle_variant_alternative を呼ぶため自然に多段
  INIT_CHECK が並び、interface-mapped variant を持つ backend (Go の `any`) でも
  正しく動く。property accessor も同じ機構を共有させる。
- backend 側で per-call inline type assert (panic-on-fail + DRY 違反) を書かなくて済む。

## 具体例

- `derive_property.cpp` の getter / setter MATCH_BRANCH 構築箇所で、
  `field_decl->parent_struct` を起点に variant chain を walk:
  ```
  while (current_struct != prop.parent_format) {
      variant_ref = state.get_struct_variant_for_id(current_struct);
      if (!variant_ref) break;        // not part of variant chain — natural stop
      emit INIT_CHECK for current_struct;
      // descend to enclosing struct via variant.related_field.parent_struct
      current_struct = variant.related_field.parent_struct;
  }
  ```
- 集めた INIT_CHECK は outer-most が先頭に来るよう逆順で block に append、
  その後 `ret` を append。
- ebm2go の既存 `Statement_INIT_CHECK` visitor の `tmp{type_id}, ok := base.(*T)`
  パターンに乗ることで、`Expression_MEMBER_ACCESS` の variant_hold pattern
  (`tmp{type_id}.<field>`) が解決される。

## これは X を意味しない

- ebm2c のように INIT_CHECK 出力を skip して union direct access で展開する
  backend には影響なし (INIT_CHECK の有無に関わらず chain が成立)。
- ebmgen が backend ごとの interpretation を決めるわけではない。
  「variant chain の各段で narrowing が必要」という意味論 hint を出すだけで、
  実際の表現 (interface assert / pattern match / union access) は backend 責務。
- 「最適化」ではない。情報の補完 (encode/decode 同等の lowering 完備)。

## 代替案

- ebm2go の `Expression_MEMBER_ACCESS_before_class.hpp` で variant_hold pattern
  の代わりに inline `(l.tmp458.(*Tmp749)).tmp444` を emit する workaround
  (一時的に commit `e8e9d9b3` で導入後、本筋転換で revert)。
  却下理由: MEMBER_ACCESS の度に type assert を runtime 評価 (Go は CSE しない)、
  panic-on-fail で堅牢性低、DRY 違反。
- inner-accessor preamble で chain assert を全部入れる案。
  却下理由: 多段 nest で preamble に N 個並ぶ + backend ごとに同じ logic を
  書き直す重複が発生。ebmgen 側で 1 度書けば全 backend に波及する方が筋。
