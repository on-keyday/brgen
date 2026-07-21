# very_slow_bit_ops は 3 軸ビット順序の参照意味論である

## 日付

- 判断時期: 2026-07-22
- 文書化: 2026-07-22

## 判断

very_slow_bit_ops (bit 単位 IO の dual 実装) を、brgen 言語の 3 軸ビット順序
モデル (byte endian / bit stream order / bit mapping order) の**参照意味論
(実行可能な仕様)** として位置づける。既存の byte-IO 経路は「対角象限
(BE×MSB, LE×LSB, stream=mapping) に対する最適化」と再定義し、offset=0 での
出力一致 (差分オラクル) により参照意味論との一致を検証する。

言語仕様上「generator dependent」と曖昧にしてある軸合成の意味は、
参照マシンが動作を定義した象限から漸進的に確定していく。全象限を
一括で確定させる必要はない。

## 動機

- **enough to represent formats**: deflate / xz / capnproto / att が実使用する
  `input.bit_order = lsb` や、byte 境界に揃わないフォーマット、bit-unaligned
  位置からの呼び出し (bit ずらし) を正しく表現・生成するため。
- **write once generate any language code**: 参照マシンは純粋な EBM 文
  (shift / mask / 8bit IO / ループ) だけで表現するため、ebm2* 全バックエンドが
  無改修で対応する。意味論の確定を ebmgen (メタ層) が一回だけ担う。
- DSL-as-spec 方向: フォーマット著者の仕様記述言語として、ビット順序の
  意味を「参照実装がこう動く」で規範化する。

## 背景 (確認済みの事実, 2026-07-22 時点)

- AST 側は 3 軸を第一級で保持: `replace_order_spec.h` が `input.endian` →
  `OrderType::byte`、`input.bit_order.stream` → `bit_stream`、
  `input.bit_order.mapping` → `bit_mapping`、`input.bit_order` → `bit_both`
  に振り分ける。意味論の対応表が `example/brgen_help/ast_enum.bgn` の
  OrderType コメントに存在する (0xC0 の stream×mapping 4 通りの例)。
- ebmgen は `convert/statement.cpp` の SpecifyOrder 変換で `order_type` を
  読まず、全軸を単一の ENDIAN_VARIABLE (1 軸の Endian) に潰している。
  EBM で表現可能なのは対角 2 象限のみ。deflate/xz が壊れていないのは
  LE×LSB 対角に乗っているため。rebrgen の unictest 入力に bit_order 使用は
  ゼロで、この潰れはテスト未観測。
- `bit_fields.cpp` の merge は `bit_size % 8 == 0` のルートのみ finalize する
  ため、既存 lowering の terminal は消費 bit 数が正確 (切り上げなし)。
  したがって lowered チェーンを再利用し terminal の byte 読みだけを
  bit-stream 読みに差し替える方式 (terminal 置換) が消費境界を保存する。

## 設計骨子

1. **dual 生成**: 全 format に byte-IO 版と very_slow 版の両方の
   encode/decode を生成する (排他でも合成でもない、呼び出し側が選ぶ)。
2. **terminal 置換**: very_slow 版は既存 lowered チェーン構造を再利用し、
   終端の byte IO のみ bit-stream 読み書きに置換する。
3. **状態は inout パラメータ**: `bit_offset` (0-7) と `current_byte` を
   state variable 機構と同型の inout パラメータとして thread する。
   バックエンドに BitStream 抽象は要求しない。
4. **呼び出し閉包**: very_slow 版の STRUCT_CALL は very_slow 版 callee を
   呼ぶ (very_slow 関数集合は呼び出しについて閉じる)。
5. **encode は flush しない**: 端数 bit は inout 状態として返す。最終 flush と
   パディング bit の値は最上位呼び出し側の責務。
6. **v1 スコープ外**: `is_peek` / `has_offset` (byte seek) / SCAN_UNTIL を含む
   format の very_slow 生成は明示エラーまたは警告付きスキップ
   (silent skip 禁止)。
7. **multi-byte × unaligned × little-endian の合成**は brgen_help の表に
   含まれない新規決定であり、疑似コードの
   `target_shift = i if is_little_endian else (bit_size-1-i)`
   (純粋なビット列マッピング方向) を採用する。

## 段階計画

1. v1: 参照マシンを 3 軸パラメータ付きで実装、配線はデフォルト
   (big/msb/msb) 固定。
2. 差分オラクル確立: offset=0 で very_slow 出力 == byte-IO 出力。
3. SpecifyOrder 変換の潰れを修正し、EBM に軸情報を運ぶ
   (EndianVariable 拡張、未指定デフォルト big/msb で既存挙動不変)。
4. deflate/xz を bit_order 込みで検証。shifted round-trip
   (offset k で encode→decode) のテストドライバ追加。

## これは X を意味しない

- 「bit_fields.cpp (byte-IO の bit field 最適化) を very_slow が置き換える」
  ではない。両者は並行経路であり、byte-IO 版の内部最適化は独立に残る。
- 「非対角象限を byte-IO 側もサポートする義務が生じる」ではない。
  非対角は当面 very_slow のみ対応でよい。
- 「brgen 本体の既存 generator (json2cpp2 等) が即座に非準拠になる」では
  あるが、それらを壊すことは意味しない。参照意味論との食い違いは
  documented divergence として扱う。
- 「検証に生成コードの byte-diff を使う」ではない。検証は実行時出力の
  比較 (差分オラクル) と unictest round-trip で行う。

## 代替案

- 各バックエンドに BitStream 抽象を実装させる: 生成コードは素直になるが
  14+ 言語 × 実装保守コスト。言語追加ごとに負債が増えるため却下。
- 関数全体を lowering 前に複製する (early duplication): terminal 置換で
  消費境界が保存できると確認できたため不要。byte 系 transform から
  very_slow 複製体を除外する仕組みが増えるだけ。
- 曖昧さ温存 (very_slow をデフォルト軸専用にする): 実在の lsb フォーマット
  (deflate/xz) が検証対象から外れ、仕様記述言語としての核を先送りする
  ため却下。
