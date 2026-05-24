# emit_struct_methods を granular helper に分割する

## 判断

`ebmcodegen::util::emit_struct_methods` を以下の 3 つに分割し、
backend が必要な組み合わせを自由に呼べるようにする:

- `emit_struct_properties`: PROPERTY accessor だけ
- `emit_struct_codec`: encode_fn + decode_fn
- `emit_struct_user_methods`: methods container 内のユーザー定義 method

後方互換のため `emit_struct_methods` は call-all wrapper として残す。

## 日付

- 判断時期: 2026-05-24（ebm2rust の inner-anon accessor relocation 実装中）
- 文書化: 2026-05-24

## 動機

- **write once generate any language code**: anon inner skip / outer aggregate
  のような non-trivial な hoisting を組むときに、property のみ抽出して別 scope
  に emit する必要が出る。call-all 一体の helper では対応不可能。

## 具体例

- ebm2rust `Statement_STRUCT_DECL_class.hpp`: anon inner struct は
  `emit_struct_codec` + `emit_struct_user_methods` のみ呼んで properties skip、
  outer struct は `emit_struct_methods` (call-all) + descendants の
  `emit_struct_properties` を append。
- ebm2cpp `struct_decl_custom`: 同パターンを Phase 別 (DeclarationOnly /
  FunctionBodyOnly) に適用。
- 元の `emit_struct_methods` を直接呼ぶ backend (default Statement_STRUCT_DECL の
  経路含む) は何も変更不要。

## これは X を意味しない

- backend が自由に順序を入れ替えていいというわけではない。多くの backend では
  call-all wrapper のまま使うのが正解。granular 版は **特定の構造的要請が
  ある backend** のみ使う。
- emit_struct_methods 内の semantics を変えたわけではない。元のロジックを
  3 つに分けただけ。

## 代替案

- 各 backend が自前で properties / codec / methods loop を書き直す。
  却下理由: 重複コード + 後から default 側を改修したときに backend 側に
  反映漏れが発生。
- emit_struct_methods に「skip_properties=true」のような bool flag を増やす。
  却下理由: flag が増えるほど combinatoric に複雑化、3 つの直交した hook と
  して分離するほうが clean。
