# FIELD_STORE marker で decode の field store を抽象化し streaming / Sans-I/O codegen を導出する

## 日付

- 判断時期: 2026-05-29
- 文書化: 2026-05-29

## 判断

decode 時に「読んだ値を出力 field へ格納する」操作を、EBM の marker statement
`FIELD_STORE`（`FieldStoreDesc{ field, target, source, lowered_statement }`）で
包む。default 挙動は `lowered_statement`（通常の格納 statement）を visit するだけ
なので DOM 型 generator（C/Go/Rust/...）は無変更・出力バイト不変。streaming 型
generator（初出は ebm2wuffs）は `field_store_custom` を override し、field を
materialize する代わりに値を sink へ **emit** する。

marker は EBM 上に常時存在させ gating しない。挙動の差は generator 側だけに置く。
これは既存の `INT_TO_ARRAY` / `ARRAY_TO_INT`（endian 変換 marker）と同じパターンの
踏襲である。

## 動機

- **write once generate any language code**: Wuffs は no-heap かつ Sans I/O
  （ライブラリは I/O を持たず、バイト供給と出力受け取りは caller 責務）の codec
  言語であり、可変長データを struct field に保持できない。EBM の現行モデル
  「format = 全 decode 値を所有する struct + `decode(src)` が `this.*` を埋める」
  （C/Go/Rust 等に綺麗に写る DOM モデル）は、可変長・variant を持つ format で
  Wuffs に写らない。同じ EBM から「格納せず流す」派生を導出できれば、Sans-I/O 系
  backend を同一 IR で賄える。
- 以前から「EBM から流れる系（streaming）モデルを導出したい」という設計欲求が
  あり、ebm2wuffs がその具体的トリガーになった。
- 再利用性: Wuffs 専用ではない。SAX / pull parser / 低メモリ C パーサ等、
  「結果を所有せず逐次出力する」任意の backend が同じ marker を消費できる。

## 具体例

- marker descriptor: `FieldStoreDesc{ field:WeakStatementRef, target:ExpressionRef,
  source:ExpressionRef, lowered_statement:LoweredStatementRef }`。
- default hook `Statement_FIELD_STORE_class.hpp`:
  `field_store_custom` があれば呼ぶ（CALL_OR_PASS）。無ければ
  `ctx.visit(ctx.field_store.lowered_statement.id)` ＝ 通常 DOM 格納。
- scaffold 段階（producer 未実装）では FIELD_STORE ノードは一切生成されないため、
  既存 generator の出力はバイト不変であることを確認済み（ebm2wuffs / ebm2c で
  leb128 を生成、exit 0・サイズ不変）。
- ebm2wuffs（今後）: `field_store_custom` → `emit(field, source)`、struct には
  format field を置かない。

## これは X を意味しない

- **EBM が Sans I/O になる、ではない。** EBM は依然「caller 供給の *完結した
  random-access バッファ* に対する依存性注入 I/O」である（underflow は terminal
  error で suspend/resume を持たない。`is_seekable` / `ReserveData` は random
  access 前提）。FIELD_STORE が扱うのは「格納 vs 放出」の軸だけ。Sans-I/O 完全化に
  必要な他2軸は別物:
  - 読み側 suspend-on-underflow: Wuffs ターゲットでは `?` から Wuffs compiler が
    coroutine 状態を合成するため EBM の課題ではない。native coroutine を持たない
    言語 × incremental 入力を狙うときだけ EBM 側に suspend 概念が要る。
  - 書き側 back-patch: EBM の encode は明示長 field + `LENGTH_CHECK` assert で
    既に forward-only が支配的（`ReserveData` は seekable 時のみの省略可能経路）。
- **marker 単体で streaming が閉じる、ではない。** decode 内で再参照される field
  （例: leb128 の `len`/`value` がシフト量・累算に使われる = P_intra）は、store を
  emit に替えるだけだと後続の member read が壊れる。streaming 側で
  **field → decode-local 昇格**（read も local に解決）と、loop で更新される field の
  **emit 配置を「最後に到達する def の後（loop 外）」に置く**処理（def-use/liveness
  解析）が別途必要。これらは struct 形状と read site を変えるため DOM へ波及させ
  られず、**streaming generator 側（ebm2wuffs）に置く**。
- **DOM generator の挙動を変える、ではない。** lowered_statement fallback により
  C/Go/Rust 等は無変更（scaffold は inert と検証済み）。
- ここでの「streaming」は emit-not-store の意味であり、coroutine/suspend の意味
  ではない（後者はターゲット言語の関心事）。

## 代替案

- **「streaming-lowered EBM」を別 IR variant として gating 生成。** 却下: 
  INT_TO_ARRAY パターン（普遍 marker + lowered fallback + generator 別 dispatch）の
  ほうが綺麗で、gating 不要・DOM 無傷・IR を marker で enrich するだけで済む。
- **marker 無しで ebm2wuffs が自前で field store を検出。** 却下: 共有・再利用可能な
  primitive を失い、各 streaming backend が同じ解析を再導出することになる。marker は
  将来 def-use/emit 配置情報を付与する自然な場所にもなる。
- **field→local 昇格 + emit を普遍 transform にする。** 却下: struct 形状と read site
  を変えるため DOM generator を壊す。streaming ターゲット側に留めるべき。

## 関連

- ADR 0031（pure VARIANT 型の declaration 生成は backend 責務）: 「表現は backend が
  決める」哲学の同系。variant も streaming 下では「格納せず分岐して emit」に解消され、
  両判断は親和的。
- ADR 0005（ebmcg / ebmip 分離）: streaming は CodeWriter 中心の default visitor から
  外れる面があり、将来 ebm2wuffs を ebmip 寄りに再配置する論点に繋がりうる（現状は
  ebmcg）。
- ADR 0017（transform passes）: Phase 2 の producer は最終的に **convert 側
  (`statement.cpp` の代入/APPEND 変換)** に実装した。当初 transform pass で後付け
  wrapping を試みたが、target の `this.field` を get_field で再発見する方式が複雑
  フォーマット(leb128/DNS label)で脆く crash した。convert は生成時点で field /
  target / value が手元にあり、`has_self` + field-decl 解決だけで堅牢に判定できる。
  decode-output の field store は decode.cpp の machinery と、ユーザが `fn decode()`
  に手書きした代入(leb128 の `value |= ...`)の2系統から来るため、convert 側でも
  単一チョークポイントは無く、現状はユーザ手書き代入(statement.cpp)を wrap する。
  machinery 側(int/enum/array の暗黙 decode)の wrap は今後の課題。
  検証は unictest round-trip(ID 名に非依存)で行い、byte-diff は不適(生成識別子が
  EBM ID を埋め込むため producer の ID 採番でノイズ爆発)と判明した。
