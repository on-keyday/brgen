# `*_visitor` hook を `*_custom` に統一する

## 日付

- 判断時期: dev-tips に明文化された方針。具体的な rename は段階的
  (2026-05-24 に composite_field_decl_visitor → composite_field_decl_custom)
- 文書化: 2026-05-24

## 判断

backend が default visitor の挙動を上書きする hook の命名・呼び出し規約を
`*_custom` に統一する。default visitor 側では `CALL_OR_PASS` で受けて、
hook が `pass` を返したら default 処理に fall through する。

`*_visitor` は MAYBE で受けて、設定されていれば pass 不可で完全乗っ取り。
これは旧 form で、新規コードでは使わず、既存も順次 rename していく。

## 動機

- **write once generate any language code**: backend が「特定 case だけ
  override したいが残りは default に任せたい」のが普通の使い方。pass で
  fall through できないと、hook 内で default の挙動を再実装する必要が出て
  重複コードが発生する。
- 命名から「pass 可かどうか」が判別できるようにする (`_custom` = pass 可)。

## 具体例

- `composite_field_decl_visitor` → `composite_field_decl_custom` rename
  (default Statement_COMPOSITE_FIELD_DECL の hook 呼び出しを MAYBE から
  CALL_OR_PASS に変更、ebm2go の hook 設定箇所も rename)。
- 既存の `_custom` hook (`struct_decl_custom`, `read_data_custom`,
  `variant_type_custom`, `assignment_custom` 等) は最初からこの規約。

## これは X を意味しない

- 既存の `*_visitor` を一斉削除はしない。使われている箇所を見つけたら
  順次 rename。
- 「特定 case だけ override」しない (= 完全乗っ取り) 用途で `*_visitor` を
  使いたいケースは残るが、`*_custom` でも `return Result` するだけで同じ
  挙動になるため、命名上の区別は薄い。

## 代替案

- 命名を `_visitor` のまま CALL_OR_PASS に変える。却下理由: 呼び出し規約の
  違い (MAYBE vs CALL_OR_PASS) が名前から判別できなくなる。
- すべての hook を CALL_OR_PASS にして `_visitor` で統一。却下理由:
  「完全乗っ取り」を意図する hook が消えるのは表現力低下。命名で意図を
  わかるようにする方が良い。
