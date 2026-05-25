# pure VARIANT 型 (common_type=nil) の declaration 生成は backend 責務

## 日付

- 判断時期: 2026-05-24
- 文書化: 2026-05-24

## 判断

EBM の VARIANT 型 (common_type=nil) について、ebmgen は型名 (`Variant{id}`) を
emit する以上のことはしない。enum / tagged union / interface など、
言語ごとの適切な declaration は backend の `variant_type_custom` で生成する。

## 動機

- **write once generate any language code**: VARIANT の表現は言語間差が
  非常に大きい (Rust enum / Zig union(enum) / C++ std::variant /
  Go interface / Python typing.Union / ...)。ebmgen が特定の形を強制せず、
  backend に表現を委ねるほうが新言語対応の柔軟性が高い。
- 言語によっては VARIANT 自体を実体化せず、上位の STRUCT_UNION (Match の
  分岐) でしか露出させない選択もある。これも backend で完結する。

## 具体例

- ebmgen の default Type_VARIANT visitor は `Variant{id}` 文字列を返すだけ。
- ebm2go `variant_type_custom`: common_type=nil なら `any` (interface) に
  マップ、配下を `tmp{type_id}` で type assert する規約を持つ。
- ebm2rust `variant_type_custom`: `pub enum Variant{id} { V0(T0), V1(T1), ... }`
  + get_v0 / get_mut_v0 accessor を decl_toplevel に push。declared_variants
  set で重複生成を防ぐ。
- ebm2zig `variant_type_custom`: `pub const Variant{id} = union(enum) { v0: T0, ... };`
  を decl_toplevel に push。
- ebm2cpp は `std::variant<std::monostate, T0, T1, ...>` を `struct_union_type_custom`
  経由で組み立て、common_type=nil の VARIANT も型名のみ参照される場面では
  この form と integration する (= top-level declaration は不要、参照時に
  std::variant 文字列を展開)。

## これは X を意味しない

- 「ebmgen は VARIANT について何も情報を出さない」ではない。Type_VARIANT として
  EBM 上に明確にあり、members や common_type は backend が参照できる。
- backend が VARIANT を出さないことは可能だが、その場合は signature 等で
  `Variant{id}` 名で参照される箇所が **未定義** にならないよう、
  backend 側で別の対応 (member type に分解、Box<dyn ...> 等) が必要。
- common_type が non-nil の VARIANT (== 全 member が同じ underlying type に
  代表される場合) はこの判断の対象外で、default visitor が common_type を
  そのまま emit する。

## 代替案

- ebmgen 側で「VARIANT は struct/enum 同等の Statement として宣言を持つ」
  形に変える。却下理由: 言語間差が大きすぎて 1 つの abstract declaration
  にまとめると逆に backend で再解釈が必要、結局 backend に責務が戻る。
- backend ごとに `variant_type_custom` を必ず実装する義務 (= default で
  名前だけ返すのを廃止)。却下理由: 新言語立ち上げ時の hook 設定漏れが
  「型名は出るが declaration なし」という比較的わかりやすい compile error
  になるほうが debug 容易。
