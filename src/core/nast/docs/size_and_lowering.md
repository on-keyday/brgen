# 幅の解析と lowering/ の開始 (2026-08-30)

## きっかけ

バックエンドを 1 本書いてみるところから始めたが、途中で方向が違うことが分かった
のでその記録も含む。順に: バックエンド (nast2go) を書く → 詰まった箇所を数える →
それが ebmgen の transform と一対一で対応すると分かる → バックエンドを畳んで
front end 側 (サイズ解析) と lowering/ に移す。

## 1. nast2go で分かったこと (削除済み)

`.bgn` から Go の struct と Decode/Encode を出すところまで書いた。udp.bgn は
gofmt / go vet / 往復 go test まで通ったが、example/ 180 本のうち go vet を通る
Go が出たのは 3 本 (cmsg / simple / udp)。

詰まった理由を数えると、ebmgen の transform の段と一致した:

| nast2go が止まった理由 | それを解いている ebmgen の段 |
| --- | --- |
| 非オクテット整数 59 | `merge_bit_field` + `lowered_dynamic_bit_io` + `bit_manipulator` |
| enum 35 | `add_cast_func` |
| 匿名フィールド 10 | `derive_property_setter_getter` |
| custom encode/decode 7 | `derive_encode_decode_wrapper` |
| 配列の代入 | `derive_array_setter` |
| offset 追跡 | `lower_runtime_state` |

つまり **AST から直に codec を書くと 13 段を各バックエンドの中で再導出することに
なる**。これは ADR 0003 (AST→IR→Code パイプラインの採用) がそもそもの出発点として
述べていることで、新しい発見ではない。`{{Unhandled node:...}}` の内訳は「次に
埋めるもの」ではなく **lowering に持っていくものの一覧**として読む。

バックエンドを書いたこと自体から取れたのは、生成器の成果ではなく**フレームワークの
穴**だった:

- `BaseContext` が Arena しか持たず、side table (Resolution / ConstantValue /
  Requirements / UnionLayout / FormatState) がバックエンドに届いていなかった。
  解析の段を回す意味が消費者に渡っていない状態。`ctx.tables()` を追加
- 長さが式の配列に「長さなし」と同じ経路を通していて、**黙って末尾まで読む
  Go を出していた**。コンパイルも通り一見動くので、往復テストを書くまで
  気付かなかった

`backends/go` は削除した。`backend/entry.hpp` / `newbackend.py` / knob 機構は残す。

## 2. TypeSize 表

`bind/type_size.{hpp,cpp}`。段は `Stage::size` (evaluate の後、requires の前)。

```
kind = fixed    ビット数が定数。bits が有効
kind = dynamic  実行時に決まる。bits_expr に幅の式 (書けたとき)
kind = unknown  決まらない (末尾までの配列、循環、並びに出ない型)
```

**語彙を 3 つに絞った理由**: 最初の消費者 (ビット畳み込み / 連続 IO のまとめ) が
「固定で何ビットか」しか要らない。ebmgen の `SizeUnit` は 8 値あるが、
`BYTE_DYNAMIC` / `ELEMENT_DYNAMIC` / `DYNAMIC` の区別は「実行時のバイト数を
どう計算して出すか」のためで lowering の都合。解析の表に持ち込むと消費者の
いない語彙が増える。

**分岐に CFG は要らない**。binder が分岐ごとの field を `InnerStruct` 表に置いて
いるので、`StructUnionType` は各分岐を畳んで揃えば fixed。CFG が要るのは
ビット畳み込みの経路探索 (どの経路でも 8 の倍数に達するか) のほうで、これは
別の問い。

### symbolic (bits_expr)

幅が定数でないときは式として持つ。

```
UDPDatagram   64 + (8 * (header.length - 8))
Item          8 + (8 * len)
Uses          ((8 + bit_sizeof(items)) + 64) + (8 * n)
Var           8 + ((kind == 1) ? 16 : ((kind == 2) ? 32 : 8))
M (match)     8 + ((kind == 1) ? 8 : ((kind == 2) ? 16 : 32))
```

規則:

- **長さの式は元の木のノードをそのまま指す**。複製すると中の名前が Resolution
  表に載っていない別ノードになり、解決先を辿れなくなる。同じノードを 2 か所から
  指す形になるが、この式は木からは辿れず表からしか来ない
- **要素ごとに幅が違う配列は掛け算にしない**。全体は要素の幅の和であって
  `要素の幅 * 個数` ではない。和を書くには畳み込みが要るので、値の名前で呼ぶ
  (`bit_sizeof(items)`)
- **分岐で揃わない幅は三項**。条件は候補が持っているものを指す。既定の分岐が
  無ければ最後の腕は 0
- **型パラメータは `bit_sizeof(<T>)`**。monomorphize の前でも幅を運べる
- 合成した二項・三項・否定は `Paren` で包む。unparse は優先順位ではなく Paren
  ノードで括弧を出すので、包まないと綴りが嘘になる

### 測定 (example/ 180 本, 1827 型)

| | 数 | 割合 |
| --- | ---: | ---: |
| fixed | 657 | 36.0% |
| dynamic (式あり) | 574 | 31.4% |
| dynamic (式なし) | 14 | 0.8% |
| unknown | 387 | 21.2% |
| (うち畳み込みが要る配列) | 195 | 10.7% |

## 3. bit_sizeof

`sizeof` はバイト単位で、コーパスで既に著者が書いている
(`sizeof(<[16]u8>)` / `sizeof(T)` / `sizeof("WAVE")`)。単位を変えると壊れるので
ビット単位のほうを足した。

**別ノードにした理由**: `Sizeof` に単位フラグを足すと、フラグを見落とした消費者が
8 倍ずれた値を黙って返す。別ノードなら既定のハンドラに落ちて音が鳴る。
nast のノードが種別ごとに別型になっている構成とも揃う。

追加は parser 2 行 (`available` / `sizeof` と同じ名前一致の場所)、node 1 つ、
typer と unparse に各 1 分岐。

## 4. lowering/

**バックエンドが呼びたいときに呼ぶ変換の置き場。** 前に走るパイプラインではない。

規約 (`lowering/lowering.hpp`):

- 足すだけ。元の木は書き換えないし落とさない (`exit_and_reversibility.md` 規則 1)
- 合成ノードの loc は由来から (規則 2)
- 結果は由来をキーにした side table に置く。2 度呼んでも同じノードが返る (規則 3)
- **どこに置くかは返す側では決めない**

入っている規則:

| ファイル | 変換 |
| --- | --- |
| `conditional` | 三項 → if + 一時変数への代入 |
| `match_to_if` | match → if の連鎖 (比較の実体化を含む) |
| `predicate` | パターン → 述語 (OrCond / 範囲 / 素)、`a == (1..=10)` の展開 |

注意点:

- **宣言は合成しない**。nast の `VariableDefinition` は初期化子から型を取るので
  値なしの宣言が書けない。EBM は「型の既定値」ノードを持つので
  `tmp := default(T)` を作れる。宣言の書き方も言語ごとに違う
  (`var x T` / `T x;` / `let x;`) ので、名前と型を返してバックエンドに書かせる
- **比較は木に無い**。parse.cpp は match の分岐条件にパターンをそのまま置き、
  `kind == 1` を作らない。だから match→if の実体は比較の実体化であって
  詰め替えではない
- **範囲比較は展開するかどうかがバックエンドの判断**。Rust の
  `(a..=b).contains(&x)` のように native に書ける言語では意味を残したい
  (ADR 0035 の趣旨)。元の Binary は残したまま、要る言語だけが呼ぶ
- **既定の分岐は 2 通りの形で来る**。`else` は条件なしの `BodyStatement`、
  match の `..` は両端が空の `Range`。判定は `node/util.h` の `is_default_cond`

### IO の lowering (2026-08-31 開始)

nast には **IO の操作を表すノードが 1 つも無い** (89 ノード中 IO らしいのは
`StreamType` という型だけ)。「この field をここで読む」は暗黙で、
`FormatState.fields` の並びがそのまま読む順になっている = as_is モデル。
だから ebmgen の IO 系の段は移植ではなく、まず語彙を作る話になる。

前提のうち 2 つは揃った: 幅 (TypeSize) と要求 (Requirements)。

最初に入れたのは `int_bytes` (EBM の INT_TO_BYTE_ARRAY / ARRAY_TO_INT):

```
--- b :u16
decode: b = (u16(buf[o]) << 8) | u16(buf[o + 1])
encode:
  buf[o] = u8((b >> 8))
  buf[o + 1] = u8(b)

--- d :i16
decode: d = <i16>(((u16(buf[o]) << 8) | u16(buf[o + 1])))
encode:
  buf[o] = u8((u16(d) >> 8))
  buf[o + 1] = u8(u16(d))
```

符号つきは符号なしで合成してから落とす (そのまま OR すると上位バイトの符号
拡張が混ざる)。encode 側も右シフトを算術にしないため符号なしへ通す。

**これは他の規則と違い side table に載せない。** 出力が呼ぶ側の用意した
バッファ名に依存するので、由来のノードだけではキーにならない。由来ごとに
1 つ決まる変換 (conditional / match / range) は表に載せ、これは材料を受け取って
組み立てる道具、という線引き。

ADR 0045 (backend が要る IO ランタイムはバイト列 read/write だけ) が正しければ、
**新しく要る原始ノードは「バイト列を読む/書く」1 対だけ**で、シフト合成も境界
検査もループも既存ノードで書ける — というのが今の見立て。int_bytes はその
一番内側を、新ノード無しで組めることの確認でもある。

**見つかった穴: endian が伝播していない。** `input.endian = config.endian.little`
の後の `e :u16` も big のまま出る。typer は `input.endian` への代入を型なしで
素通しするだけで、`IntType::endian` を埋める段が無い。言語の既定が big なので
「未指定 = big」は正しいが、**スコープを記録する解析が無い**。int_bytes は
`IntType::endian` を正しく見ているので、埋める側を足せば直る。
これは memory の未完リスト (サイズ/ビット幅、sub_byte、**endian スコープ**、
monomorphize) にあったもので、幅の次はこれになる。

## 5. EBM との差 (訂正を含む)

「置く側が違う」と書きかけたが誤り。EBM も emit 時にホイストしている:

```cpp
// default_codegen_visitor/visitor/Expression_CONDITIONAL_STATEMENT_class.hpp
MAYBE(w, ctx.get_writer());          // 文レベルの writer を貰う
w.get().write(target_str.to_writer());       // 宣言
w.get().write(conditional_str.to_writer());  // if
return ctx.identifier(ctx.target_stmt);      // 式の位置には名前だけ
```

実際の差は 2 つ:

1. **いつ作るか**。ebmgen は消費者を知らないので全部の三項に文形式を作る
   (`convert/expression.cpp` の `make_conditional`)。nast は要る言語が
   呼んだときだけ作る。置くのはどちらも emit 側
2. **宣言を合成できるか**。上記の通り

nast に無い部品: **`WriterManager` に当たるもの**。`node/code_writer.h` にあるのは
`IndentStackOf` までで、「今の文の書き出し先」を辿る仕組みが無い。三項の文形式を
実際に使うバックエンドが出た時点で要る。

## 6. 次

- **CFG**。ビット畳み込み (`merge_bit_field` に当たるもの) が経路探索を要る。
  nast には無い。ebmgen の `analyze_ref` は block / if / loop / match /
  continue / break / return / io の 8 種で約 120 行。nast の対応物は全部あり、
  しかも `Break`/`Continue` は `related_loop` を、`Return` は `related_function`
  をノード自身が持っているので、構築時の loop_stack が要らない。
  面倒なのは `If`/`Match` が式なので、式の位置に出る制御フローを扱うこと
  (ebmgen の `CFGExpression` に当たるものが要る)
- 畳み込み構文 (`sum(items, ...)` 相当) を言語に足すかどうか。`available` /
  `sizeof` と同じ「値に対する述語で意味は lowering 側」の系列だが、束縛の構文が
  無いので言語設計の判断が要る
- 残り: 式なし 14 / unknown 387

## 検証方法についての観察

今日見つかった実バグ 4 件 — `24 * ..` (範囲を長さとして掛けた)、括弧落ち
(`8 * len - 8`)、`1 ? 8 : ...` (match のパターンを条件にした)、field の二重計上
(binder が分岐に 2 種類の field を作る) — は**全部 unparse で `.bgn` として
印字して見つかった**。木を読んで論じるより、綴りに戻して眺めるほうが速い。

`nast_size_probe` / `nast_lower_probe` (`bench/`) がその用途の道具。
