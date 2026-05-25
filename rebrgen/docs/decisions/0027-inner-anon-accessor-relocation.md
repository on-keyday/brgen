# inner-anon struct の property accessor を parent_format scope に集約する

## 日付

- 判断時期: 2026年5月（ebm2rust → ebm2cpp の wasm_src2json 修正作業中に確立）
- 文書化: 2026-05-24

## 判断

anon inner struct (`if 分岐` 等で自動生成される `name` nil な STRUCT_DECL) に属する
PROPERTY accessor は、その struct 自身の impl/class block には emit せず、
`parent_format` で示される外側 struct に集約して emit する。
method 名は parent_struct identifier を prefix して衝突回避する (`tmp318_max` 等)。

## 動機

- **write once generate any language code**: impl block / class block を持つ言語
  (Rust, C++) で、accessor の body は `self.<outer_field>` 等の外側参照を含むため、
  inner struct を receiver にすると field lookup が壊れる。outer に置けば
  body の lowered expression が無修正で動く。
- ebm2c のように free function 風命名で対応する言語と、receiver method を持つ言語
  での差を 1 つのパターンで吸収。

## 具体例

- `stub/util.hpp::collect_anon_inner_descendants` で variant containment chain を
  辿り、anon inner descendants を収集する共有 helper を提供。
- ebm2rust の `Statement_STRUCT_DECL_class.hpp` と
  ebm2cpp の `struct_decl_custom` (entry_before_class.hpp) でこの helper を使い、
  outer struct visit 時に descendants の properties をその impl/class 内へ append。
- 各 backend の `function_definition_start_wrapper` / `function_decl_custom` で
  `prop->parent_struct != prop->parent_format` のとき method 名に
  `<inner_struct_identifier>_` を prepend。
- `Expression_MEMBER_ACCESS` / `assignment_custom` でも同じ prefix を適用し、
  base を outer self に reset して call を直接 `self.tmp318_max()` に展開。

## これは X を意味しない

- anon inner struct 自体は別 type として残る (top-level に flat で declare)。
  accessor だけが outer に hoist される。
- prefix 命名 (`tmp318_max`) はあくまで言語側の collision 回避ルール。
  EBM の PROPERTY_DECL の identifier 自体は変えない。
- ebm2c はそもそも free function 風 (`tmp318_get_max(Limits*)`) で命名衝突
  しないので、この hoist パターンは適用していない。言語ごとに使うか決める。

## 代替案

- inner struct の impl block 内に accessor を出す元設計 (= ebm2rust が当初
  取っていた form)。却下理由: body 内の `self.tmp458` 等の outer field 参照が
  inner struct の self では解決不能。
- accessor の receiver を inner struct (`*Tmp318`) にする。却下理由: setter で
  outer 側の field を書き戻す必要がある場合 (variant tag 切り替え等) に
  outer field を update できない。
