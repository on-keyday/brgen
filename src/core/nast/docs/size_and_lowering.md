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

ADR 0045 は「backend が用意すべき IO ランタイムはバイト列の read/write
プリミティブ」と言っている。ここから素直に読むと、いま要りそうな原始ノードは
「バイト列を読む/書く」あたりで、シフト合成も境界検査もループも既存ノードで
書ける。int_bytes はその一番内側が新ノード無しで組めることの確認。
ただし今見えている範囲での話で、subrange / peek / offset のような要求
(Requirements の語彙にあるもの) を実際に降ろすと足りないものが出てくると思われる。

### field の読み書き (2026-08-31)

`lowering/field_io` が int_bytes の外側を組む。EBM の `ENUM_UNDERLYING_TO_INT`
と `ARRAY_FOR_EACH` に当たる:

```
--- kind :Kind
decode: kind = <Kind>(u8(buf[o]))
encode: buf[o] = u8(<u8>(kind))

--- varying :[count]u16
decode:
for i40 := 0; i40 < count; i40 = i40 + 1:
    varying[i40] = (u16(buf[o + (i40 * 2)]) << 8) | u16(buf[o + (i40 * 2) + 1])
```

**位置を進めるところは組まない。** 何バイト読んだかをどう持つかは IO の
表し方と密で、IR で一意にできないと EBM が一度撤回している (ADR 0008)。
「この位置から読む」形までを返し、位置の管理は呼ぶ側。配列の要素位置は
要素幅が固定のときだけ組める (`offset + i * 幅`) ので、要素が可変幅の配列は
組めない — 進みながら読む形になり、同じ理由で呼ぶ側の領分。

測定 (example/ 180 本、読み書きの対象になる field 7066 のうち):

| | 数 | | |
| --- | ---: | ---: | --- |
| 組めた | 4053 | 57.4% | |
| StructType | 1310 | 18.5% | 入れ子 format。呼び出しの語彙が無い |
| IntType | 1090 | 15.4% | バイト境界に乗らない幅 (u1/u2/u4 等)。畳み込み待ち |
| ArrayType | 503 | 7.1% | 要素が可変幅 / 末尾まで |
| EnumType | 50 | 0.7% | 下地の型が書かれていない enum |
| StrLiteralType | 47 | 0.7% | magic |
| その他 | 13 | 0.2% | bool / 関数型 / generic / inline struct |

**分岐が合成した同名 field (型が UnionType、769 件) は母数に入れない。**
名前解決のための人工物で、実際に読むのは分岐の中に並んでいる field のほう。
判定は `node/util.h` の `is_layout_field` — `bind/type_size` が二重計上を
避けるために既に持っていたものを共有した。

浮動小数は同じ幅の整数として並べ、値は `BitCast` で移す:

```
--- single :f32
decode: single = bit_cast<f32>((((u32(buf[o]) << 24) | ...) | u32(buf[o + 3])))
encode: buf[o] = u8(bit_cast<u32>(single) >> 24)
        ...
```

**`Cast` とは別のノードにした。** `<u32>(f)` は値の変換で、ビットの読み替えと
取り違えると 1.0 が 1065353216 になる。EBM は `CastType::FLOAT_TO_INT_BIT` /
`INT_TO_FLOAT_BIT` として cast の種類で持ち、`get_cast_type(dest, src)` が型から
導出している (convert/expression.cpp:139)。**その導出だと float→int は常に
ビット再解釈**になり、値として切り捨てる変換が表現できない。nast は
`bit_sizeof` / `IsLittleEndian` と同じく、見落としたら音が鳴る別ノードにした。
消費側の綴りは言語ごと (go は `math.Float32bits` / `Float32frombits`、
zig / c / llvm にもそれぞれの分岐がある)。

副産物: unparse が `EnumType` / `StructType` を綴れず
`/*unprintable type*/` を出していた。原文には宣言の名前が書かれ、これらの
ノードは typer が合成するものなので、今まで印字の対象にならなかった。
lowering がこれらへの cast を組むので、宣言の名前で綴るようにした。

### 入出力からバイトを出し入れする (2026-08-31)

`lowering/stream_io`。**言語が既に持っている語彙をそのまま使う**:

```
--- scalar :u16
fill:
buf[0] = input.get()
buf[1] = input.get()
drain:
output.put(buf[0])
output.put(buf[1])

--- varying :[count]u16          個数が式なら回す形になる
fill:
for b837 := 0; b837 < count * 2; b837 = b837 + 1:
    buf[b837] = input.get()
```

`input.get()` / `output.put(x)` は手書きの `fn decode` / `fn encode` が使って
いるもので、コーパスに 74 箇所ある (asn1 / avro など)。typer の
`type_of_stream_call` が扱っていて、`input.get()` は引数なしなら u8、型リテラルを
渡せばその型。ADR 0045 の言う「backend が用意すべき read/write プリミティブ」に
当たるものが、**言語側にも名前を持っている**ので新しいノードは要らなかった。

**文にして並べるのが要点。** `(u16(input.get()) << 8) | u16(input.get())` の
ように式へ直接置くと評価順の保証が言語ごとに違い、上位と下位が入れ替わりうる。
ebm2go の生成物が一時配列に読んでから合成しているのも同じ理由:

```go
tmp1154 := [2]uint8{}
io.ReadFull(tmp50, tmp1154[:])
s.World = (uint16(tmp1154[0]) << 8) | (uint16(tmp1154[1]) << 0)
```

だから `stream_io` (バイトを並べる) と `int_bytes` (並んだバイトから値を組む)
を分けてある。合成して初めて「読む」形になる。位置は `input.get()` 自身が
進むのでこちらでは数えない。一時配列の宣言は組まない (呼ぶ側の領分)。

### endian のスコープ (2026-08-31)

`bind/endian_scope.{hpp,cpp}`、段は `Stage::endian` (evaluate と size の間)。
結果は `FieldEndian` 表 (over Field)。

規約は rebrgen の `converter.hpp:462` に書かれているものに合わせた:

> `input.endian = ...` は**字句スコープ**。書いた文からその block の末尾まで
> 効き、呼び出しグラフを辿って入れ子 format には及ばない。block は format の
> 本体・fn の本体・if/match/loop の本体。トップレベルの代入は block では
> ないので、そこから先のファイル全体に効く。

強さの順は 型に綴られたもの > スコープ > big (言語の既定)。

**値は定数とは限らない。** example/ に動的な代入が 12 箇所ある:

```
bpf.bgn:87     input.endian = endian.is_big ? config.endian.big : config.endian.little
elf.bgn:18     input.endian = endian == Endian.LittleEndian ? config.endian.little : config.endian.big
omg_cdr.bgn:47 input.endian = endian.is_little_endian
```

表はこのとき**式ではなく代入 (SpecifyOrder) を指す**。式は代入の位置で 1 回
評価されるものなので、field ごとに展開すると (a) 参照している field が先に
進んでいれば別の答えになり、(b) 同じ三項を field の数だけ焼く。EBM が
ENDIAN_VARIABLE という文に落としているのも同じ理由。

判定を専用ノードにするかどうかについて: EBM の
`IS_LITTLE_ENDIAN{endian_expr, little_endian_value}` の `endian_expr` は式では
なく ENDIAN_VARIABLE 文への参照で、実質「その変数 == little」である。専用な
のは左辺が普通の式でないためで、比較そのものは普通の比較。加えて格納される値
の型が揃っていない (bpf/elf は列挙、omg_cdr は bool) ので、どの値が little か
を持つ必要がある = `little_endian_value`。**代入の位置で正規化すれば**
(「little か」の bool か正準の列挙値に落とす)、判定は定数との普通の Binary に
なり、語彙の追加は要らなくなる。

なお ebmgen の動的 endian は未完で、コードにそう書いてある
(`converter.cpp:45-63`): `set_on_function()` が呼ばれないため
`current_dynamic_endian` が入らず、IS_LITTLE_ENDIAN が endian_expr 無しで
作られ、**動的 endian が全部 native として生成される**。写せる実装は無い。

値の判定は**列挙メンバの名前**で行う (`config.endian.little` の解決先が
組み込み `endian` 列挙の `little`)。畳んだ整数を使うと列挙の並び順に依存する。

元実装 (middle/typing.cpp:1993) はトップレベルの最後の 1 つをファイル全体に
効かせる。こちらは書かれた位置から効かせる。トップレベルの指定が全 format より
前にあるコーパスでは同じ結果。

測定 (example/ 180 本, 7090 field): big 6271 / little 641 / native 97 /
dynamic 81。

`int_bytes` はバイト順を引数で受け取る (渡さなければ型に綴られたもの)。
**組めるのは big と little だけで、他は null を返す。** 決まらない場合が
2 つあり、EBM の `IS_LITTLE_ENDIAN` はその両方を兼ねている:

- `native`: ターゲット上の静的な値。生成器には決められないが、ターゲットの
  コンパイラには決められる (rust `cfg!(target_endian)` / C++
  `std::endian::native` / C `#if BYTE_ORDER`)
- `dynamic`: 実行時の値。代入の位置で 1 度だけ評価した変数を読んで選ぶ

構造はどちらも「両方の順で組んだものと選択子を渡し、選ぶのは相手」で同じ。
`combine_int_either` / `split_int_either` がその形:

```
--- offset :u16          (input.endian = config.endian.native の下)
decode: offset = isLittle ? ((u16(buf[o + 1]) << 8) | u16(buf[o]))
                          : ((u16(buf[o]) << 8) | u16(buf[o + 1]))
encode:
if isLittle:
    buf[o] = u8(offset)
    buf[o + 1] = u8((offset >> 8))
else:
    buf[o] = u8((offset >> 8))
    buf[o + 1] = u8(offset)
```

判定は `IsLittleEndian` ノードとして置く。**綴りはバックエンドが決める。**
式に落とせないからノードにしてある: 実行時の順なら「代入の位置で材料化した
変数との比較」だが、native の判定は `cfg!(target_endian = "little")` のような
**ターゲット言語のテキスト**で、nast の式にならない。ノードのまま渡して綴りを
emit まで遅らせる。

```
--- a :u16                          (input.endian = config.endian.native)
decode: a = is_little_endian() ? <little> : <big>

--- c :u32                          (input.endian = order.is_big ? .. : ..)
decode: c = is_little_endian(order.is_big ? config.endian.big : config.endian.little) ? <little> : <big>
```

区別は `order` が空かどうかに置いた (空 = native)。EBM の `IS_LITTLE_ENDIAN`
と同じ場所で、`endian_expr` が空かどうかで分けている。ノードが持つのは式では
なく**代入**なので、バックエンドは代入の位置で材料化した値を読む
(式を field ごとに焼き直さない)。

`is_little_endian(...)` には .bgn の構文が無いので、この綴りは再 parse を
通らない。合成専用のノードは全部そうなるので、lowering が増えれば同じものも
増える。往復の検証が踏まないのは、合成ノードが木からは辿れず side table から
しか来ないため。unparse の保証は「parse が組んだ木の範囲で再 parse できる」
であって、木全体ではない (`parse/unparse.h` に明記した)。

EBM も同じ切り分けで、
`add_endian_specific` (converter.cpp:208) が native と dynamic を 1 つの経路に
まとめ、`IS_LITTLE_ENDIAN` の中身を言語側へ委ねている:

```cpp
const auto is_native_or_dynamic = endian.endian() == native || endian.endian() == dynamic;
if (is_native_or_dynamic) { ... EBM_IF_STATEMENT(res, is_little_ref, then, else); }
```

区別は `endian_expr` が空かどうかに押し込まれている (native なら空)。
消費側は空なら knob の文字列、そうでなければ変数比較:

```
rust    cfg!(target_endian = "little")
c/cpp   (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
python  sys.byteorder == 'little'
java    (java.nio.ByteOrder.nativeOrder() == java.nio.ByteOrder.LITTLE_ENDIAN)
llvm    ""                                  ← 空 = 未対応の目印に落ちる
```

当初 `native` を big として組んでいた (`effective != Endian::little` の判定)。
コーパスに 97 field ある。

### ebmgen の lowering 一覧 (2026-08-31 時点の棚卸し)

移す先を選ぶときの地図。3 系統ある。

**(a) `transform/` の 13 段** — 全員に対して 1 度走るパイプライン。

```
flatten_io_expression
lowered_dynamic_bit_io (read/write, CFG 使用)
merge_bit_field
vectorized_io (read/write)
derive_property_setter_getter
add_cast_func
derive_array_setter
derive_encode_decode_wrapper
propagate_io_input_desc
lower_runtime_state
[derive_very_slow_bit_ops]        フラグ付き
remove_unused_object
```

**(b) `LoweringIOType` 10 種** — READ_DATA / WRITE_DATA に付く形。

| | nast | |
| --- | --- | --- |
| `INT_TO_BYTE_ARRAY` | ✔ `int_bytes` | |
| `ENUM_UNDERLYING_TO_INT` | ✔ `field_io` | |
| `ARRAY_FOR_EACH` | ✔ `field_io` | |
| `STRING_FOR_EACH` | — | magic の各バイト (組めない field 47) |
| `FLOAT_TO_BYTE_ARRAY` | — | ビット列との相互変換 (20) |
| `STRUCT_CALL` | — | 入れ子 format の呼び出し (1310) |
| `BIT_FIELD_TO_BIT_SHIFT` | — | ビット畳み込み。CFG が前提 (1090) |
| `VECTORIZED_IO` | — | 連続する固定長のまとめ |
| `SCAN_UNTIL` | — | 末尾まで読む |
| `MULTI_REPRESENTATION` | — | 複数形を並べて選ばせる (ADR 0011) |

**(c) `lowered_expr` を持つ式** — 意味のノードと展開形を両方持つもの。既定の
visitor が展開形を出しているのは 6 種
(`default_codegen_visitor/visitor/Expression_*` にある):

| | nast |
| --- | --- |
| `CONDITIONAL` → `CONDITIONAL_STATEMENT` | ✔ `conditional` |
| `RANGE_EQUAL` → `a <= x && x <= b` | ✔ `predicate` の `lower_range_compare` |
| `AVAILABLE` | ✔ `available` (修飾は `WithReceiver`。腕が真偽値なので `\|\|` / `&&` に畳む) |
| `ENUM_IS_DEFINED` | ✔ `enum_defined` |
| `GET_STREAM_OFFSET` | — RuntimeState の読み (ADR 0039) |
| `MAX_VALUE` | — その型の最大値 |

**field の引数** は上の 3 系統のどれでもなく、`convert_field_serialize` が
直に組んでいる。nast は 2026-09-02 に `field_io` へ入れた。位置で書かれたもの
と名前つきで意味が別で、名前つきは 5 種類ある (原実装の振り分けは
`middle/typing.cpp:1774-1868`)。コーパス 180 本の内訳:

```
引数つき field 404
├ 位置指定 193      期待値。IntType 155 / EnumType 33 / StructType 4 / ArrayType 1
│                   うち 135 が assert 込みで組める (残りは型の側が未対応)
└ 名前つき 211
  ├ input      186  読む先の付け替え。全部 input.subrange(..)   sub_byte_length
  ├ vbr.len      9  呼ぶ先 (format VBR) の state へ代入          assigns
  ├ config.type  7  ビット列をこの整数型として表す               type_map
  ├ input.align  5  この位置で境界に揃える                       alignment
  ├ qstate.enable_length 2  同じく呼ぶ先の state                 assigns
  ├ input.peek   1  進めずに覗く                                 peek_value
  └ input.xor    1  原実装も知らない名前。Metadata で生成器に届く
```

右端は原実装の `FieldArgument` のどのフィールドに入るか。名前つきはどれも
バッファと位置そのものを差し替えるので `field_io` の線引きの外にあり、
組めない側に入れる (probe の `組めない (引数)` 213 = 名前つき 211 +
位置指定が 2 つ以上 2)。

位置指定は `assert(field == 期待値)` になり、読む側は読んだ後、書く側は書く前
に置く (ebmgen も同じ向き)。**配列の field に付いたときは要素と比べる** —
`[20]u8(0)` は 20 要素すべてが 0 の意味で、原実装が `repeat` mapping と
呼んでいるもの。期待値そのものが配列なら全体と比べる (`direct`)。書かれ方では
決まらないので期待値の型で分ける。位置指定が 2 つ以上ある `u8(7,128..255)` は
「どれかに一致」(`some_candidate`) で、比較 1 つには落ちないので組めない。

marker 系はほかに `FIELD_STORE` (ADR 0032)、`ENDIAN_VARIABLE`、
`IS_LITTLE_ENDIAN` (nast にノードを足した)、`INT_TO_ARRAY` / `ARRAY_TO_INT`。

nast にある残りは match→if (ebmgen だと `derive_match_lowered_if`) と
`stream_io` (バイトの出し入れ)。手を付けやすいのは `STRING_FOR_EACH`、
重いのは `STRUCT_CALL` と `BIT_FIELD_TO_BIT_SHIFT`。`AVAILABLE` と
`ENUM_IS_DEFINED`、field の位置引数 (期待値) は 2026-09-02 に済んだ。

### 真偽値の三項は論理演算に畳む (2026-09-02)

`branch_chain` (`lowering/predicate.hpp`) は分岐の候補を後ろから三項に積む。
`available` はその値が真偽値なので、畳まないと
`(kind == 1) ? true : ((kind == 2) ? true : false)` が出る。`Builder::cond` が
腕の真偽リテラルを見て論理演算にする:

```
c ? true : x     ->  c || x
c ? x : false    ->  c && x
c ? false : x    ->  !c && x
c ? x : true     ->  !c || x
c ? true : false ->  c
c ? false : true ->  !c
```

上 4 つは `x` を評価するのが「c がその向きに転んだとき」だけで、三項と評価の
順も回数も変わらない。**両腕が同じリテラルのときだけ c の評価が消える**ので、
そこは `node/util.h` の `is_side_effect_free` が通ったときに限る。副作用を
持ちうるのは呼び出しだけだが、`<u8>(x)` の型変換も木の上では Call なので
callee が TypeLiteral のものは除く。

コーパス 180 本の `available` 13 件は全部これで式のまま出るようになった:

```
before  (kind == 1) ? true : ((kind == 2) ? true : false)
after   (kind == 1) || (kind == 2)

before  (a == 1) ? ((b == 2) ? true : false) : false
after   (a == 1) && (b == 2)
```

サイズの `branch_chain` (値が整数) は畳まないので変わらない。三項 2517 件は
全部整数か enum で、真偽リテラルの腕を持つものは元の木には 1 つも無い。

### 値 knob と合成名の規約 (2026-08-31)

**knob はノードに紐づける。** `nodes.json` のノードが `knobs` を持ち、
`gen/backend.py` が `backend/config.hpp` を生やす。出力もノードごとに入れ子:

```cpp
ctx.config().IsLittleEndian.native_endian_check = "cfg!(target_endian = \"little\")";
```

knob は「そのノードのハンドラに付いた追加のつまみ」なので、宣言もハンドラの
隣にある。フックを書かずに済ませられるものはここに置く — 言語差の大半は
「どう綴るか」であって「どう辿るか」ではない (rebrgen 実測: 既定フック 103 本
に対し言語側のフック 3〜30 本、knob 設定は 50〜83 件)。1 つの言語にしか無い
ものは LangConfig に置き、3 言語で同じものを書いたら上げる (ADR 0016)。

ノードに紐づかない設定 (今のところ `--unhandled` の扱いだけ) は
`nodes.json` の `backend_config` に書く。生成器が特定のフィールド名や enum を
知らずに済むよう、そちらも宣言から出す。

最初の実例が `IsLittleEndian` の 2 つ。既定ハンドラは native の綴りを knob から
取り、空なら**黙って big と決めつけず未対応の目印に落ちる** (ebm2llvm が
`native_endian_check = ""` で同じことをしている)。

**合成した変数の名前は由来のノードから決まる** (`node/util.h` の
`derived_name`)。`tmp<id>` / `endian<id>` / `i<id>` / `b<id>` の 4 つが同じ規約で、
後から「その変数は何という名前か」を知りたい側は同じ関数を呼べばよい。
EBM は `ctx.identifier(ref)` という登録簿でこれをやっているが、由来が決まれば
名前も決まるので登録は要らない。

実行時に決まるバイト順は `lowering/endian_variable` が代入の位置で
`endian<id>` に落とし、`IsLittleEndian` の既定ハンドラがその名前を読んで
`endian<id> == <little_endian_value>` を出す。

### レシーバ (self) (2026-08-31)

**原文の木にレシーバは無い。** `data :[len]u8` の `len` は裸の `Reference` で、
`Resolution` に解決先が `Field` だと載っているだけ。生成コードでは `t.Len` の
ようにレシーバが要るのに、「ここに要る」という印が木のどこにもない。

最初は綴る側で足していた (「参照の解決先が `Field` なら前置する」)。2026-09-01 に
**束縛の段で木に実体化する**ほうへ移した — 下の「self を木に実体化した」を参照。
形を 1 つに揃える理由はどちらでも同じで、原文から来た参照と合成した参照が
違う形だと、**1 つの式の中で同じ意味のものが 2 つ**並ぶ:

```
((8 + bit_sizeof(self.items)) + 64) + (8 * n)     ← 一度こうなった
                    ^^^^ 合成                ^ 原文
```

**レシーバを使うのが普通なので、フックではなく knob で吸収する。** 既定の
`MemberAccess` / `Self` ハンドラが `Self.spelling` + `MemberAccess.separator` で
綴る。綴りが違うだけの言語は 2 つの文字列を申告すれば済む:

```cpp
ctx.config().Self.spelling = "t";          //  t.bytes.length
ctx.config().MemberAccess.separator = ".";
```

`Self.spelling` が空なら黙って名前だけ出さず、未対応の目印に落ちる。

**separator は深さに依らず一律で、レシーバだけ別の綴りにはしない。** 一度
`--self self --sep '->'` を「C 系の形」として出したが、これは継ぎ目が 1 つの
ときしか合っていない:

```
bytes.length   →   self->bytes->length     ← --sep '->' (壊れる)
bytes.length   →   (*this).bytes.length    ← 参照外しを spelling に畳む
```

レシーバの次は普通の member なので、hop ごとに `->` を選ぶ余地はない。参照外しが
要る言語は **spelling 側に畳む** (`(*this)` / `(*self)`)。ebm2cpp が
`config.self_value = "(*this)"` にして `MEMBER_ACCESS` の綴りを `.` 固定に
しているのと同じ形で、あちらはそもそも separator を knob にしていない。

見るには `nast_backend <file.bgn>` (既定は `--self '(*this)' --sep .`)。左が
原文の綴り、右が生成側の綴りで、変わったものだけ出る。

EBM は変換の時点で `MEMBER_ACCESS{base: SELF}` に実体化している。あちらは
変換が式を作り直す立場なので揃えられるが、こちらは原木を残す立場なので
揃える先が逆になる。綴りが言語ごとなのは同じ (`MEMBER_ACCESS` は共通化不適と
測定済みのグループ)。

**この差は `available` の解決で効く。** `available(field)` (裸) と
`available(a.b.field)` (修飾) では gating 条件を別の base に載せ直すかどうかが
変わるが、EBM は変換で裸の Ident も `MEMBER_ACCESS{base: SELF}` にしてしまう
ので、変換後の形では区別できず、変換前の AST を見に行っている
(`ebmgen/convert/expression.cpp` の `convert_expr_impl(Available)`)。木にレシーバを
実体化しないというのは、その区別を最後まで残すということでもある。実装するとき
`Available.target` にレシーバを足さないこと。

### ebmgen のレシーバ機構 (2026-08-31 に読み直し)

「なぜ EBM は変換時にレシーバを実体化したのか」を追った。理由は **レシーバが
一定でないから**で、機構は 2 段になっている。

**1. スコープのスタック (`ConverterState::self_ref`)** — 「今の self は何か」。
`set_self_ref` は RAII で元に戻す。切り替わるのは:

| 場所 | self | 出典 |
| --- | --- | --- |
| `Format` の変換に入る | `SELF` | `statement.cpp:753` |
| variant の腕の block | その腕の accessor 式 | `handle_variant_alternative` (`statement.cpp:517`, `564`, `1022`) |
| property の getter/setter | その property のレシーバ | `statement.cpp:1206` |
| `MemberAccess` の member 側 / enum member | 無し (`nullopt`) | `expression.cpp:525`, `583` |
| state variable / 関数ローカル | 付けない (`is_state_variable` / `has_parent`) | `expression.cpp:421` |

腕の中で self が変わるのは、EBM では分岐の field の **保存場所が腕の struct の
中** だから。`self.name` では届かない。

**2. 宣言ごとの表 (`self_ref_map`)** — 「その宣言はどこに居るか」。
FIELD_DECL を変換するたびに `MEMBER_ACCESS{base: その時点の self, member: 名前}`
を記録する (`statement.cpp:1178-1236`)。参照 (Ident) を変換するときは、
**使用位置の self ではなく宣言の記録の base** を取る (`expression.cpp:421-434`)。
腕の中で宣言された field を腕の外から参照しても正しい経路になるのはこれ。

**3. `on_available_check` フラグ** — 修飾された `available(a.b.field)` の gating
条件だけは、宣言側の base を無視して `a.b` を使う。宣言の記録が勝つと
`header.flag.padding` が variant の下に潜ってしまう (`expression.cpp:926-`)。

ADR 0027 (inner-anon accessor relocation) は同じ問題の生成側で、accessor の
body が外側の field を参照するので、レシーバは内側 struct ではなく外側でないと
解決できない、という話。

**nast との対応。** 構造的に起きないもの:

- state variable は `StateVariable` という別のノードなので、`Field` を見る判定に
  最初から掛からない
- `MemberAccess.member` は `Ident` であって式ではないので、member 側を参照と
  して綴る経路が無い

**関数ローカルは起きた。** `x := 1` は `VariableDefinition` で Field ではないが、
**`y :u8` は format の中でも関数の中でも同じ `Field`** で、木の形が同じ。
「解決先が Field なら前置」だけでは関数の中の宣言まで `(*this).y` になる。
ebmgen が `has_parent` (= 宣言の belong が function でない) で外していたのは
これで、nast も同じ区別が要った。

持ち主は `Field.belong` に置く。`belong` は元の AST では `Member` にあり
(Field も Member だった)、nast では body を持つ側 (`NamedBodyStatement`) と
`BodyStatement` / `MatchBranch` / `EnumMember` / `TypeParameter` にだけ残って
いた。Field に無かったのは移植の取りこぼしで、parse.cpp には
`// field->belong = state.current_member();` がコメントのまま残っている。
持ち主を追う機構 (`enter_member`) ごと落ちていたので、`parse_indent_block` が
既に受け取っている `scope_owner` から辿り直した — 名前と body を持つもの
(format / state / fn) だけを持ち主にし、分岐の block では変えない。

判定は `belong` が `Function` かどうか。ebmgen の `has_parent`
(= 宣言の belong が function でない) と同じ形になる。corpus では 2 箇所
(`llvm_ir.bgn:52` の `f :VBRField`、`zip.bgn:93` の `pkt :Section(...)`) が
誤ってレシーバを付けていた (参照 4 件、2683 -> 2679)。確認は
`testdata/receiver.bgn`。

残る 1 つが本題で、**分岐の中の field の保存場所**。nast は binder が名前ごとに
format 直下の union field を作るので、参照の解決先は使用位置で変わる:

```
if k == 1:
    v :u8
    w ::= v + 1     # ここの v -> 分岐の中の Field
z ::= v + 2         # ここの v -> format 直下の union field
```

(Resolution 表で確認: 同じ `v` が別の Field を指す。) 平らに持つバックエンドなら
どちらも `self.v` で正しい。variant で持つバックエンドでは内側のほうに腕の経路が
要る — つまり ebmgen の `self_ref_map` に当たるものが、変換順ではなく
`UnionLayout` の決定から要る。今のレシーバ規則が答えられるのは平らな場合まで。

### available (2026-08-31)

`available(x)` は「その field を宣言した分岐を通ったか」を訊く述語で、答えは
**その分岐の条件そのもの**。材料は binder が置いた `UnionType` — 分岐ごとに
宣言された同名 field を 1 つにまとめるとき、候補に「その分岐の条件」と
「その分岐にその field があるか (無ければ null)」が並んでいる。要るものは
既に木にあり、ここの仕事は畳み方だけ:

```
match kind:
    1 => value :u8
    2 => value :u16

available(value)       ->  (kind == 1) ? true : ((kind == 2) ? true : false)
available(value,u16)   ->  (kind == 1) ? false : ((kind == 2) ? true : false)
```

畳む形は幅の場合分けと同じなので、`lowering/predicate` の `branch_chain` に
出して両方から呼ぶ (`type_size` にあった同じ折り畳みは消した)。候補の型が
違うだけで、`cond` を持ち値を返せれば同じ規則で畳める。

**第 2 引数の型。** `available(x, u8)` は「今どちらの候補か」を訊く形
(`example/coap.bgn`)。候補の型が一致する分岐だけが真になる。nast の parser は
これを落としていた (`first_argument` しか見ていなかった) ので、
`Available.selected_type` に拾い、unparse も綴るようにした。ebmgen 側では
`expected_type` を読んでいる箇所が rebrgen に 1 つも無く、u8 の問いと u16 の
問いが同じ式になる。

**入れ子の分岐。** 分岐の中の分岐で宣言された field は、候補の field 自身が
また `UnionType` になっている。内側まで降りて掛け合わせる:

```
if a == 1:
    b :u8
    if b == 2:
        c :u8

available(c)  ->  (a == 1) ? ((b == 2) ? true : false) : false
```

外側の条件だけで答えると `a == 1` で真になってしまう。ebmgen の
`convert_expr_impl(Available)` は候補の field が null かどうかだけを見ていて
降りない。

**修飾された target (`available(lab.pointer)`) は WithReceiver で包む** (2026-09-02)。
候補の条件は内側の format の field を指しているので、そのまま綴ると self に
対する参照になる。`WithReceiver{receiver, expr}` は「この中の Self は receiver」
という包みで、綴る側 (unparse / バックエンドの既定ハンドラ) がレシーバを
スタックで持ち、`Self` に出会ったら積まれているものを綴る。**式は複製しない。**

```
available(lab.pointer)        -> (lab.prefix == 0b11) ? true : false
available(labels[i].pointer)  -> (labels[i].prefix == 0b11) ? true : false
available(opt.value)          -> (opt.kind == 1) ? true : ((opt.kind == 2) ? true : false)
```

複製する案 (`Self` の葉を差し替えた新しい式を作る) を採らなかったのは、
side table が 17 本あってコピー先には付いてこないため — `Resolution` の
再登録に加え、`ConstantValue` の欠けた式を「畳み込み済み」と読む側が出る。
包む形なら表は無傷で、知らない消費者は未対応ノードとして音が鳴る。

**base が式なので、綴りでは複数回出る** (`lab.a == 1 && lab.b == 2`)。
一時変数に束ねるには文の書き出し先が要る (ebmcodegen の `WriterManager` =
文のフックが積む一時 writer のスタック)。実際に使うバックエンドが出た時点で
同じものを足す。

**取りこぼしが 2 つあった** (どちらも今回の実装で判明):
`bind/receiver` は所有辺しか辿っておらず、(1) weak の指す先を差し替えて
おらず、(2) 表からしか指されていないノード (binder が作る union field と、
その `UnionType.cond` が持つ match の主語) に届いていなかった。前者は木から
外れた参照を指し続け、後者は綴る側がレシーバ無しの参照を見る。どちらも
「同じ名前が self 越しでもそうでなくても同じに綴れる」ぶん、実体化した
直後には見えなかった。

測定 (`nast_probe lower example/*.bgn testdata/*.bgn`):

```
available -> 式        21
available (組めない)    0
```

### 「平らな場合まで」の実物 (2026-09-01)

レシーバの節の「今のレシーバ規則が答えられるのは平らな場合まで」の、平らでない
側の実物。既存の生成器はどちらも分岐の field を腕ごとの struct に入れている。

`save/go/simple_case.go` (ebm2go):

```go
type tmp1109 struct { VarField1 []uint8 }
type tmp1112 struct { VarField2 []uint8 }
type Variant446 interface{ isVariant446() }

type Simple struct {
    Hello uint8          // 直下の field は平ら
    ...
    tmp804 Variant446    // 分岐の field はこの下
}
func (s *Simple) VarField1() *[]uint8 {
    if s.bitField1() == 0 {
        tmp1111, ok := s.tmp804.(*tmp1109)
        ...
```

`save/rust/http2.rs` (ebm2rust):

```rust
pub struct Struct983 { pub settings: Vec<Setting> }
pub enum Variant272 { None, V0(Struct2956), V1(Struct983) }
pub struct Struct564 { pub tmp570: Variant272 }
```

つまり分岐の中の field の置き場は `self.VarField1` ではない。直下の `Hello` は
平らなので `self.Hello` で合う — 今の規則が答えられているのはそちらだけ、という
意味。

### Field と分岐の対応 (2026-09-01 に確認)

**腕 → field はある。** `InnerStruct` (over `BodyStatement`) が分岐の block ごとの
field と assert を持ち、`StructUnionCandidate` が `{cond, inner_struct}` で腕と
その block を結んでいる。`binder.hpp:99-110` が blocks と 1:1 で候補を作り、その
型を持つ匿名 `Field` を format 直下に置く。`type_size.cpp:260` が既にこれを使って
分岐の幅を畳んでいる (「分岐に CFG は不要」の根拠)。

**field → 腕の逆引きは無い。** `Field.belong` は format / state / fn までで、
分岐の block では持ち主を変えない。導出するなら index 経由:

```
UnionType.candidates[i].field == 探している Field となる i
UnionType.base_type -> StructUnionType
StructUnionType.candidates[i].inner_struct       -> その腕の BodyStatement
```

index は揃う。`UnionType` 側は宣言の無い分岐に cond だけの pad を入れるため
(`binder.hpp:143-155`)。ただし**末尾の不在は pad を入れない**ので長さは短く
なりうる。整合するのは prefix まで。

### union 越しのメンバアクセス (未実装の構想)

`UnionType.member_candidates` (`vector<Node<Field>>`) は宣言と生成コードと wire に
だけあり、`bind/` `lowering/` `parse/` のどこも読み書きしていない。由来は
「UnionType のメンバアクセスをまた UnionType にしようとしていた頃の名残」
(2026-09-01)。想定していたのはこの形:

```
format PayloadA: len :u8  / payload :[len]u8
format PayloadB: len :u16 / payload :[len]u8

format Usage:
    tag :u8
    if tag <= 10: payload :PayloadA
    else:         payload :PayloadB
    utf.isUTF8(payload.payload) == true
```

`payload.payload` の実体を置く場所。要素が `Node<Field>` なのは、その先が
`field :UnionType` になっていてそこをまた辿る想定だったため。cond は親の
`candidates` と index で並ぶ (`UnionLayout` の member_types / cluster_types と
同じ並行ベクタの形)。

**今の挙動。** `typer.cpp:410` は union 越しのアクセスを common_type に剥がして
から引く (元実装の `lookup_union` と同じ)。`common_type` に StructType 同士の
分岐は無く `typer.cpp:569` で nullref に落ちるので、上の例は型が付かない。

**分配のほうが強い理由。** base の候補 `PayloadA` / `PayloadB` に共通型は無い
(UnionLayout で uncommon) が、メンバ側の候補 `[len:u8]u8` と `[len:u16]u8` は
どちらも `[..]u8`。**base に共通型が無くてもメンバには共通型がありうる**。
剥がしてから引く順序だとここで落ちる。

畳む機構は既にある — `lowering/predicate` の `branch_chain` を `available` と
`type_size` が共有している。メンバアクセスは同じ fold の「値」インスタンスで、
3 つ目の呼び出し側になる。

**需要。** コーパスの union 995 件のうち uncommon は 22 件だが、未型付けの残りは
fixture 等で説明が付く = uncommon union 越しのメンバアクセスはコーパスに無い。
コーパスが要求している穴ではなく、言語として書けるようにしたい機能の側。

**出口は `available` の修飾ケースと同じ。** 分配して得られるのは分岐ごとの式で、
綴るには腕の経路が要る。載せ替えの機構を 1 つ決めれば両方片付く。

### self を木に実体化した (2026-09-01、`bind/receiver`)

レシーバの節は「付けるのは綴る側」で書いていたが、名前解決の後に木を書き換える
ほうへ移した。`Reference` のうち解決先が「レシーバを取る field」のものを
`MemberAccess{base: Self, member: 同じ Ident}` に差し替える。

**規則には抵触しない。** 「原木を書き換えない」は lowering の規約
(`lowering/lowering.hpp` / `exit_and_reversibility.md` 規則 1) であって、
フロントエンド全体の禁止ではない。parse は既に書き換えている — 引数の `x = y`
を `NamedArgument` へ (`parse.cpp:833`)、文位置の `config.X = v` を `Metadata` へ
(`rewrite_builtin_statement`, `parse.cpp:1988`)。binder が `Self` を実体化するのは
この規則の外。

**「裸/修飾の区別が消える」は印を残せば起きない。** `Self` は .bgn の構文に無く
この段以外では作られないので、`available(x)` は base が `Self`、`available(a.b.x)`
は base がそれ以外、で見分けられる。前例は `Cast` の `is_explicit`
(`<u8>(x)` と `u8(x)` は同じノードになるが、フラグで往復が守れている)。EBM が
区別を失ったのは実体化そのものではなく、印を持たずに実体化したから。

**利点 2 つ。** (1) rebase の差し替え点が明示になる — 綴る側で足していたときは、
式を辿って各 `Reference` の `Resolution` を引き field なら包み直す walk が要った。
(2) 判定が 1 箇所になる — 「解決先が Field なら前置」は綴る側の再導出で、実際
2026-08-31 に関数ローカルを取りこぼしている (`belong` で修正)。

**Ident は作り直さず持ち回す。** `Resolution` のキーは `Reference` ではなく中の
`Ident` で、`MemberAccess.member` も `Node<Ident>` なので、同じノードを移せば表は
無傷。これは飾りではない — 分岐の中で宣言された field と format 直下の
union field は同じ名前で別の宣言を指すので、持ち主から名前で引き直すと使用位置の
区別が消える。`typer` の `type_of_member_access` も base が `Self` のときは
`Resolution` を優先する。

**2 パスで書き換える。** arena の pool は `vector` なので、走査中に `make` すると
`data_at` で取った `NodeData*` が無効になる。差し替える参照を集める → ノードを
作る → スロットを差し替える、の順にして、走査中は確保しない。

**波及したのは 6 か所。** `unparse` (実体化した base は綴らない。綴ると `self` が
構文に無いぶん再 parse できないテキストになる) / `node/util.h` の `assign_root`
(実体化したレシーバは越えない — 越えると書き込み先が全部 self になる) と
新しい `referenced_name` / `requires` の `lhs_root_name` / `lowering/self_ref` の
`field_ref` (合成する参照も同じ形にする) / `available` の裸判定 / 既定の
`Reference` ハンドラ (レシーバの前置をやめ、`MemberAccess` + `Self` が綴る)。
`evaluator` と `endian_scope` は元から両方の形を見ていたので変更なし。

**測定** (`example/` 314 ファイル):

```
実体化                1989 参照
typed 22224 / 22244   (99.9%、未型付け 20 = 実体化前と同じ)
到達可能な式          20255 -> 22244   (差し替えごとに Self が 1 つ増える)
corpus                311 ok / 3 error
wire / unparse 往復    ともに ok
available -> 式 16 / 組めない 5        (実体化前と同じ)
```

**境界。** 実体化するのは receiver 1 段 (= その format のインスタンス) まで。
腕の経路まで実体化すると格納戦略をフロントエンドが決めることになる — EBM の
`self_ref` スタックがそれ。腕の経路は `UnionLayout` + バックエンドの選択。
`Self.owner` はその起点を指すだけで、そこから先は持たない。

### `self` を文法に足した (2026-09-01)

実体化と同じノードを**原文にも書けるようにした**。予約語に `self` を足し
(`src/core/lexer/lexer.h`)、parse が `Self{is_explicit: true}` を作る。
`owner` は parse が入れる — `current_member_` (field の belong) と並べて
`current_receiver_` を持ち、こちらは format / state でだけ更新する
(fn の body に入っても `self` は外側の format)。format の外に書いたら
その場でエラー。

**暗黙と明示で解決の仕方が違う。** 意図的:

| | 解決 | |
| --- | --- | --- |
| 暗黙 (`x` → `self.x`) | 名前解決が決めた先をそのまま | 分岐の中の field と format 直下の union field の区別が残る |
| 明示 (`self.x`) | 持ち主からメンバを引く | 同名の local に隠されない |

暗黙のほうは「裸の名前をどう綴るか」の話なので解決は既に済んでいる。明示のほうは
書いた側が「レシーバのメンバ」と言っているので、普通のメンバアクセスとして引く。
`typer` の近道は `!is_explicit` のときだけ通る。

**書けると効くのは 2 つ。** 同名の local に隠された field を指すこと
(`testdata/self_keyword.bgn` で確認: `self.len` は 2 か所とも `Field` に、
裸の `len` は `VariableDefinition` に解決される) と、インスタンスそのものを
渡すこと。`available(self.x)` は `available(x)` と同義で、判定は
「base が `Self`」のままなのでどちらも裸扱い。

**往復は `is_explicit` が守る。** `Cast` の `<u8>(x)` / `u8(x)` と同じ、
「同じノードに畳んだ 2 つの書き方」の区別。unparse は明示のものだけ綴る。

コーパスへの影響なし (`example/` で `self` は全部コメント中の
`self-describing` などで、識別子としては 1 件も使われていない)。ただし
**予約語は src2json と共有**なので、`self` を書いた `.bgn` は旧経路では
構文誤りになる。

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
- **union 越しのメンバアクセス** (`payload.payload`)。載せ替えは `WithReceiver`
  で済んだので、残るのはメンバアクセスを候補に分配する側
- `x.is_defined` は落とせるようになった (`lowering/enum_defined`、メンバとの
  比較の連鎖)。並びが連続していれば範囲比較に畳めるが、native に書ける言語が
  あるので綴る側の判断
- 畳み込み構文 (`sum(items, ...)` 相当) を言語に足すかどうか。`available` /
  `sizeof` と同じ「値に対する述語で意味は lowering 側」の系列だが、束縛の構文が
  無いので言語設計の判断が要る
- 残り: 式なし 14 / unknown 387

## 検証方法についての観察

今日見つかった実バグ 4 件 — `24 * ..` (範囲を長さとして掛けた)、括弧落ち
(`8 * len - 8`)、`1 ? 8 : ...` (match のパターンを条件にした)、field の二重計上
(binder が分岐に 2 種類の field を作る) — は**全部 unparse で `.bgn` として
印字して見つかった**。木を読んで論じるより、綴りに戻して眺めるほうが速い。

`nast_probe` (`tool/probe/`) がその用途の道具。入り口は 1 つで、見たいものを
最初の引数で選ぶ。ファイルが 1 つなら明細、2 つ以上なら集計。ソースはモードごとに
`size.cpp` / `endian.cpp` / `lower.cpp` に分けてある — 覚える名前は増やさずに、
木を見て何が見られるか分かるように。

```sh
python src/core/nast/build.py -r probe size   src/core/nast/testdata/nested_size.bgn
python src/core/nast/build.py -r probe endian example/bpf.bgn
python src/core/nast/build.py -r probe lower  src/core/nast/testdata/match_patterns.bgn
python src/core/nast/build.py -r probe size   example/*.bgn      # 集計
```

`-r <名前>` は `nast_<名前>` を建ててから走らせて、以降の引数をそのまま渡す。
ビルドツリーの場所を知らずに済ませるためのもので、他の道具にも使える
(`-r corpus --tree <f>.bgn` / `-r dump <f>.bgn`)。

id が分かっているものを 1 つずつ追うときは `nast_query`。ebmgen の interactive
debugger に当たるもので、`p <id>` がノードとその下の side table を一緒に出し、
`u` で綴りに、`src` で原文の位置に戻せる。**中身は `query/session.{hpp,cpp}` に
あって `nast_core` に入っている** ので、バックエンドや LSP からも同じ問い合わせ
ができる (`tool/query.cpp` は引数を読むだけ)。出力は文字列に積むだけで、
どこへ出すかは呼ぶ側が決める。
`find <Kind> [{ 条件 }]` で種別から探し、`show <Kind>` でそのノードの
フィールド名と型 (列挙は取りうる値も)、`lower <id>` でそのノードに当てはまる
lowering 規則を当てた結果が見られる:

```sh
python src/core/nast/build.py -r query example/dns.bgn -c "find Available"
#165    Available            @46:16  available(labels[i].pointer)
#222    Available            @52:16  available(lab.pointer)
```

確認用の `.bgn` は `src/core/nast/testdata/` にある。目で見るために書いたもの
だが、見た後は wire / unparse の往復に乗せて守る側に置いてある
(CMakeLists の `NAST_CORPUS` が example/ と一緒に拾う)。
