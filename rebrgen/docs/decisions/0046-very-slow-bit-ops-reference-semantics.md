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
3. **状態は RuntimeState companion (ADR 0039) で thread する** (2026-07-22 決定、
   当初案の inout u8 パラメータ 2 個から変更): バックエンドはスカラーパラメータを
   値渡しで emit するため、参照渡しスカラーという新パターンを言語ごとに実装する
   代わりに、既存 companion の配管 (gate / param 追加 / call site 引数 / pointer 化)
   を再利用する。フィールドは `current_bit` (0-7) と `current_byte` (部分バイト) を
   **新設**する。既存の `bit_offset` フィールドは lower_runtime_state の絶対 bit
   位置概念であり、0-7 のバイト内カーソルと混同してはならない。companion が
   未合成のモジュールでは derive が合成する (offset + current_bit + current_byte)。
   variant の companion param は公開シグネチャの一部であり (呼び出し側が開始
   bit 位置を与えるのが目的なので) wrapper による隠蔽はしない。
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

## 実装で確定した規則 (2026-07-22, ebm2go で検証済み)

- **置換位置は「バックエンドが native emit するノード」**: スカラー IO は lowered
  チェーン経由で emit される (ADR 0045) のでチェーン終端を置換する。bytes 型
  (u8 配列/ベクタ) IO はデフォルト visitor が (VECTORIZED_IO / SCAN_UNTIL の委譲を
  除き) そのノードで native emit するため、lowered の有無に関わらず**そのノードで**
  置換する。チェーン終端だけ置換すると bytes wrapper が先勝ちして素通りする。
- **offset 付き read は offset==0 (BYTE_FIXED) のみ許容**: bit-field 群バッファの
  位置指定読みは相対 offset 0 の逐次読みなので置換可能。それ以外は byte-IO 形
  維持 + 警告 (bit_offset==0 でのみ正しい)。
- **RESERVE_DATA の役割変換**: slice 系バックエンドの reserve は「出力窓への
  re-slice によるバッファ実体化」を兼ねる。対の write を bit ループへ置換した
  reserve は「バッファの zero-fill 実体化」に置き換える (チェーンの index 代入が
  実バッファに書き、bit ループがそれを読む)。flush 側の 1 byte write は実証済みの
  buffer→reserve→index 代入→write 形をそのまま生成する。
- **複製は dedup を迂回する (`add_unique`)**: リポジトリの内容ベース dedup は
  「ノード = immutable な値」前提の最適化。weak ref 付け替えを後回しにする複製は
  追加時点で原本とバイト同一になり得て、通常 add では原本への alias になり
  後段の付け替えが**原本を破壊**する。clone 経路は identity を明示する。
- **until-EOF (`[..]T`) は byte fetch の遅延性により shift 下でも正しい**: byte
  残量ベースの EOF 判定でも、バイトは必要になった時にのみ fetch されるため、
  末尾の部分バイトは最後の要素の読取り中に消費済みになり、判定はズレない
  (ipv6 NDP options で k=1..7 検証)。
- **呼び出し側契約**: 開始時 `current_bit=k` と (k>0 なら) 消費途中の
  `current_byte` を companion に seed する。encode 終了後の端数 bit の flush は
  最上位呼び出し側の責務 (ADR 0046 原則 5)。

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
