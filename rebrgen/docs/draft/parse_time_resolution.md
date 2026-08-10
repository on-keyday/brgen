# parse 時点で決まるもの / 決まらないもの

`src2json` は parse の後に 16 のパスを順に走らせる。そのうち何が本当に別パスを要するのかを、
各パスが読む情報から判定した。nast のプロトタイプに意味論を戻すときの作業リストを兼ねる。

測定日 2026-08-10。対象は `src/core/middle/` と `src/tool/src2json/src2json.cpp:440-570`。

## 判定基準

parse 時点で決められるのは、次の 3 つだけで判定が閉じるものとする。

1. 原文にそのまま書かれた名前の並び (`input.offset` / `config.endian.big` / `error` / `sizeof`)
2. 演算子 (`BinaryOp` は parse 時に確定している)
3. その場のノードの形 (`Binary(assign)` か `Call` か、引数が何個か)

逆に、**識別子が何を指すか (`ident->base`)**、**式が何型か (`expr_type`)**、
**モジュール全体の情報**のどれかを要するものは parse に入れてはならない。

この基準は `ast_prototype_analysis.md` の「parser が既に typing 解析の一部をやり始めている」
という指摘と矛盾しない。あちらが問題にしているのは **解決を要する** ものを parser がやっている点で、
以下に挙げるのは **解決を要さない固定名と局所形状** だけで決まるものである。

## 規模

`example/` 309 ファイルを `src2json` に通し、これらの書き換えが作ったノードを数えた。

| 書き換え結果のノード | 個数 | ファイル数 |
| --- | --- | --- |
| `assert` | 417 | 115 |
| `io_operation` | 335 | 100 |
| `metadata` | 325 | 207 |
| `specify_order` | 112 | 75 |
| `explicit_error` | 46 | 34 |
| `sizeof_` | 47 | 7 |
| `available` | 23 | 13 |

`metadata` が最多のファイル数で出るのは `config.url` が 175 ファイルにあるため。
どれも「たまに出る特殊形」ではない。

JSON 上のノード名は `sizeof_` であって `size_of` ではない (`resolve_available` が作る型は
`ast::SizeOf`)。`evaluate_sizeof` は値を畳んで `evaluated_value` に入れるだけでノードは残す。

## parse に寄せられるもの

共通の機構は `ast/tool/extract_config.h` の `extract_name` / `extract_config`。
これは `SpecialLiteral` → `MemberAccess` の連鎖を文字列に落とし、`Binary(assign)` と `Call` の
形を見るだけで、`base` も `expr_type` も一切読まない。以下はすべてこれに乗っている。

### 1. IO 操作の確定 (`resolve_io_operation`)

`MemberAccess` の連鎖が次の名前に一致したら `IOOperation` にする。

| 参照形 | 呼び出し形 |
| --- | --- |
| `input.offset` / `input.remain` / `input.scope_length` / `input.bit_offset` | `input.get` / `input.peek` / `input.backward` / `input.subrange` / `output.put` |
| `config.endian.big` / `.little` / `.native` | |
| `config.bit_order.msb` / `.lsb` | |

判定は名前の文字列一致のみ。

### 2. バイト順 / ビット順の指定 (`replace_specify_order`)

`input.endian` / `input.bit_order` / `input.bit_order.stream` / `input.bit_order.mapping` への
代入を `SpecifyOrder` にし、`OrderType` を名前から決める。

### 3. メタデータ (`replace_metadata`)

1 と 2 で取られなかった `config.*` への代入と `config.*(...)` 呼び出しを `Metadata` にする。
**「残り全部」を取るので、必ず 1 と 2 の後**。現状の `replace_metadata.h` にも
`// after resolve_io_operation` と書かれている。

### 4. `error()` (`replace_explicit_error`)

`error(...)` 呼び出しを `ExplicitError` にする。引数が 1 個以上で、第 1 引数が文字列リテラルで
あることを要求する。どちらも parse 時に分かる。

### 5. `available()` / `sizeof()` (`resolve_available`)

`Available` / `SizeOf` にする。引数が 1 個以上、`available` は第 1 引数が `Ident` か
`MemberAccess` であること。付与する `expr_type` も定数 (`BoolType` / `IntType(64)`) で、
推論の結果ではない。

### 6. アサーション (`replace_assert`)

文の位置にある `Binary` のうち `is_boolean_op(op)` が真のものを `Assert` にする。
演算子だけで決まる。

ただし `Assert::is_io_related` は `IOOperation` ノードを探すので **1 の後**である必要がある。
parse 内で 1 → 6 の順に置けば成立する。

### 順序が意味を持つ

1 / 2 / 3 は `config.*` と `input.*` という同じ構文空間を奪い合う。現状はパスの実行順
(`specify_order` → `explicit_error` → `io_operation` → `metadata` → `assert`) が優先順位を担っている。
parse に入れると **1 箇所の分岐順**になるので、暗黙の実行順依存が明示的な if 連鎖になる。
これ自体は改善だが、順序を変えると意味が変わることは変わらない。

## parse に寄せられないもの

| パス | 要求するもの |
| --- | --- |
| `resolve_import` | 別ファイルの読み込み。driver の仕事 |
| `analyze_type` (`typing.cpp`) | 名前解決と型推論そのもの |
| `monomorphize` | 型引数の解決とモジュール全体 |
| `collect_unused_warnings` | `expr_type` が `VoidType` かを見る |
| `mark_recursive_reference` | 型の参照グラフ |
| `detect_non_dynamic_type` | 同上 |
| `evaluate_sizeof` | 型の確定と単相化の結果 |
| `analyze_bit_size_and_alignment` (`type_attribute.cpp`) | 各フィールドの型の幅。`Follow` と `fixed_tail_size` はここ |
| `resolve_state_dependency` | `ident->base` を辿って format 間で伝播する |
| `analyze_block_trait` | 上のほぼ全部の結果 |

## nast 側に足りないノード

`src/core/nast/nodes.json` は 78 ノード。上記の書き換えが作るノードのうち、

- **ある**: `Assert`
- **無い**: `IOOperation` / `SpecifyOrder` / `Metadata` / `ExplicitError` / `Available` / `SizeOf`

6 種を追加しないと 1〜5 は入れられない。

## 注意点

**エラーの出る段が変わる。** `error()` の引数不足や `available()` の引数形の誤りは、今は parse を
通った後に報告される。parse に移すと構文エラーと同じ段で出る。
`F0062-error-tolerant-parsing` の回復の仕方が変わるので、`error_tolerant.bgn` の期待も変わりうる。

**固定名はシャドウされない。** `error` / `available` / `sizeof` は今も名前の文字列一致で判定していて、
ユーザーが同名の `fn` を定義しても奪われない。parse に移しても挙動は同じだが、
「parser がこれらの名前を知っている」ことが構造として明示される。
仕様として意図どおりかは別途決める必要がある (未確認)。

**LSP には利得がある。** middle を走らせずに parser 単体でこれらのノードが得られる。
`ast_prototype_analysis.md` の「最初から parser 単体で外部から使いやすくしたい」に直接効く。

## 未確認

- `input.bit_order.stream` / `.mapping` と `config.order.after` は example に用例が無い。
  実際の意味は `SpecifyOrder` / `OrderType` の消費側を見ないと確かめられていない
- `replace_metadata` が `resolve_io_operation` の後でなければならない理由は、コメントと
  名前空間の重なりから推測している。具体的に壊れる入力は特定していない
