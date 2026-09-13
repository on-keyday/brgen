# ebm2hike は Go 構文だが ebm2c 側の runtime モデルを採る

## 日付

- 判断時期: 2026-09-14 (ebm2hike 追加時)
- 文書化: 2026-09-14

## 判断

Hike (github.com/kanryu/hike-lang) 向け generator `ebm2hike` は、構文は
ebm2go を写すが、IO とエラーの runtime モデルは ebm2c を写す。すなわち
`*ebmDecoder` / `*ebmEncoder` という呼び出し側が所有する struct への
ポインタを渡し、返り値は `int` エラーコード (0 = 成功) とする。
生成物は import を一切持たず、runtime prelude を自分で抱える。

## 動機

- Hike の構文は Go そのもの (`type X struct`, メソッド, スライス,
  `append`, generics, 多値返却) なので、struct / 関数 / enum の形は
  ebm2go の knob 設定がほぼそのまま使える。
- 一方 ebm2go の IO は `io.Reader` / `io.Writer` と `encoding/binary`、
  エラーは `error` interface に乗っている。Hike の interface は宣言こそ
  できるが dynamic dispatch は README の Roadmap 側 (未チェック) にあり、
  `std/` の解決には `hike.mod` の `replace` ディレクティブが要る。
  生成コードが `hike.mod` を要求すると、単体の `.hike` を `hikec` に
  渡すだけでは通らなくなる。
- ebm2c は「interface も例外もない言語」向けに既にこの形を持っている。
  `EncoderInput*` / `DecoderInput*` と `int` 返りは、そのまま
  `*ebmEncoder` / `*ebmDecoder` と `int` に移せる。
- 結果として、構文レイヤ (entry_before の knob 群) は ebm2go、
  IO とエラーのレイヤ (`read_data_bytes_io_wrapper` /
  `write_data_bytes_io_wrapper` / `sub_byte_range_wrapper` /
  `is_error_visitor` / `error_return_visitor`) は ebm2c を参照して書ける。

## 具体例

- `visitor/includes.hpp` の `runtime_prelude` が `ebmDecoder` /
  `ebmEncoder` と `ebmDecoderTake` / `ebmDecoderInto` /
  `ebmEncoderWrite` などを定義し、`Statement_PROGRAM_DECL_before` が
  `package <name>` の直後にそれを出す。`--no-prelude` で抑止できる。
- union は Hike に union 型がないので、arm を全て並べた struct に
  なる (ebm2zig の `union(enum)` に対応する位置)。どの arm が生きて
  いるかは decoder が分岐したのと同じ条件で決まる。
- optional は `*T`。値が変換結果で `&` を取れない場合のために
  prelude に `ebmPtr[T](v T) *T` を置く。ただし hikec は struct で
  instantiate した generic を誤って emit するため、composite literal
  だけは `&T{...}` を直接使う。

## Hike 側の制約 (ADR 0043 の適用)

単一言語の compiler quirk は共有 knob ではなく backend 側で解く
(ADR 0043)。ebm2hike では以下を backend 側で吸収している。

- 2 文字以下の型名は `collectTypeParamsFromNode` が型パラメータと
  みなすため、`format X` のような短い型名は `XType` に伸ばす。
- `const` は宣言型を捨てるため enum メンバは `var` で出す。`const` の
  ままだと `Name_Member == x` が i64 対 i8 の `icmp` になり clang が
  弾く。`var (...)` ブロックは hikec が parse しないので 1 行ずつ出す。
- 整数リテラルは自分の型を明示する (`uint8(6)`)。untyped literal が
  i64 に落ちて、狭い側との演算で IR の型が食い違う。
- struct フィールド経由の代入 `outer.inner.field = v` は hikec が
  コピーに書いてしまい黙って消えるため、各段で `(&outer.inner).field`
  とアドレスを取る。
- 固定配列からスライスへは変換ではなくスライス式 `arr[:]` を使う。

これらは ebm2hike の hook 内に閉じており、共有 Visitor.hpp には
何も足していない。

## これは X を意味しない

- 「Go 風の言語は全部 ebm2c モデルにする」ではない。判断材料は構文の
  見た目ではなく、その言語の runtime に何があるか (interface dispatch /
  標準ライブラリの到達性) である。Hike に dynamic dispatch と import
  無しで届く IO 抽象が入れば、ebm2go 側のモデルに寄せ直す余地がある。
- 「上記の制約は Hike の言語仕様」ではない。現時点の `hikec` の実装
  都合であり、直れば回避策は外せる。外すかどうかは unictest の結果で
  決める。

## 代替案（あれば）

- `std/io` の `Reader` / `Writer` interface に乗せる案。生成物が
  `hike.mod` の `replace` を要求するようになり、`.hike` 単体を
  `hikec` に渡す unictest の形が取れなくなるため却下。
- union を tagless overlay (ebm2llvm 相当) にする案。Hike には union も
  再解釈キャストもないため不可。
