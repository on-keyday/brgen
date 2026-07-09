# composite union の fold と型の 2 ビュー

## 日付

2026-07-09

## 判断

common_type を持つ union をコード生成する際、EBM は **2 つの型ビュー**を提示する。コードジェネレーターはこれらを別々に描画しなければならない。

- **STRUCT_UNION (storage view)**: `related_field` を持つ物理表現。arm は STRUCT。列挙型 (`Variant{id}` enum) + `get_vN` で描画する。
- **VARIANT (value view)**: arm の value 型を common_type で統一した論理表現。`related_field` を**持たない**。common_type primitive として描画する。

さらに、backing field が `PREFIXED_UNION_PRIMITIVE` composite に fold される union は、単一 storage word に畳み、arm アクセスを composite getter/setter に flatten する(ebm2go の `bulk_primitive` 機構と同一)。非 fold の plain common_type union はこれを行わず enum 機構を保持する。

## 動機

- **write once generate any language code**: ebm2go が既にこの fold を行っており、ebm2rust を揃えることで生成コードの構造を言語間で一致させる。
- QUIC VarInt(prefix で 6/14/30/62-bit を選ぶ)のような prefix-selected union を、Rust variant enum に展開するのではなく Go と同じく単一の packed storage word に畳む。
- この 2 ビューの区別を見落とすと「片方を直すともう片方が壊れる」ループに陥る(実装時に websocket ⇄ quic で往復した)。後続の言語実装者が同じ罠を踏まないよう記録する。

## 具体例

fold 可否の**判別子**は VARIANT 側には存在しない。STRUCT_UNION の `related_field` → `field_decl` → `composite_field` が `PREFIXED_UNION_PRIMITIVE` かで判定する(field の `inner_composite == true`)。

- **fold する**: QUIC VarInt (`field 84` → STRUCT_UNION 23, composite 59 = PREFIXED_UNION_PRIMITIVE)。`VarInt { tmp59: u64 }` に畳まれ、`value()` は `tmp131()` 経由で読む。
- **fold しない**: websocket `ExtendedPayloadLength` (16/64-bit を別々保持, `inner_composite == false`)。`Variant62` enum + `get_vN` を保持。value view の VARIANT は common_type (u64) で描画。

### ebm2rust の修正箇所群(後続の参照点)

| ファイル / hook | 役割 |
|---|---|
| `visitor/includes.hpp` | `is_foldable_composite_kind` / `get_composite_field` / `related_field_is_folded_composite`(fold 判別子)/ `physical_field`(arm→composite storage への path 定数) |
| `visitor/Visitor_before.hpp` | `bulk_primitive` set(fold union の arm 型 ID を保持) |
| `visitor/Statement_INIT_CHECK_class.hpp` | composite union の variant guard を抑止し、arm 型を `bulk_primitive` に登録 |
| `visitor/Expression_MEMBER_ACCESS_before_class.hpp` | arm read を `(self.<getter>() as <arm_type>)` に flatten |
| `entry_before_class.hpp` `assignment_custom` | arm write を `self.set_<setter>((v) as <storage>)?` に flatten |
| `visitor/Expression_TYPE_CAST_class.hpp` | target が common-typed union の cast を common_type に書き換え(`as Variant{id}` / E0605 回避) |
| `entry_before_class.hpp` `variant_type_custom` | common_type VARIANT は `pass`(default が common_type を描画)。enum を吐くのは `common_type == nil` の pure variant のみ |
| `entry_before_class.hpp` `struct_union_type_custom` | `related_field` が fold composite のときだけ common_type を描画(gate) |
| `entry_before_class.hpp` `function_definition_start_wrapper` | COMPOSITE_GETTER/SETTER の return/param を common_type に lowering。1-bit は bool public + `_raw` (u8) internal |
| `visitor/Statement_STRUCT_DECL_class.hpp` | 畳んだ composite の getter/setter を struct の impl に emit(variant-arm は arm 自身の impl) |
| `visitor/Statement_FUNCTION_DECL_before_class.hpp` | property getter が composite の address を取る場合 `ptr_to_owned`(Option<&T>→Option<T>) |

commit: `4f1ebcd0`(Phase 3 = PREFIXED_UNION)。Phase 1/2 = top-level / variant-arm の BULK_PRIMITIVE fold。

## これは X を意味しない

- **すべての common_type union を fold するわけではない**。fold は backing field が `PREFIXED_UNION_PRIMITIVE` composite のときのみ。plain union(distinct-width arm を別々保持)は enum + `get_vN` を維持する。
- **VARIANT を常に enum にするわけではない**。common_type を持つ VARIANT は value view なので common_type primitive に描画する。enum を吐くのは storage view (STRUCT_UNION) と pure variant (common_type==nil) のみ。

## 代替案（あれば）

- **VARIANT type を type_custom で直接 common_type にレンダリング**: 却下。VARIANT の value view と STRUCT_UNION の storage view を区別できず websocket の enum を壊す。default `Type_VARIANT` が既に common_type を描画するため `pass` で足りる。
- **member の primitiveness で fold を判別**: 却下。websocket の非 fold union も arm が primitive (UINT16/64) なので判別できない。`related_field` の composite kind が唯一信頼できる判別子。
