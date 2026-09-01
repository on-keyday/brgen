# nast の計測プログラム

ここにあるのは**計測**だけ。木を見て回る道具は `tool/`、ctest に乗る試験は
`test/` にある (`nast_probe` は計測ではないので 2026-09-01 に `tool/probe/` へ
移した)。

`python src/core/nast/bench.py --tools` で建つ (CMake の Release ツリー)。
出力は `build/release/bin/` に入る。
`example/` の `.bgn` を引数に取る。

```
./src/core/nast/build/release/bin/nast_coverage $(find example -name "*.bgn")
```

## 何を測るか

| | |
| --- | --- |
| `coverage.cpp` | 式の種類ごとの数と型の付き具合。木から到達できないノードの数も出す。**型解析を進めるたびに見る指標** |
| `parse_split.cpp` | 字句解析だけ / 字句 + 構文 の差。構文解析そのものの時間を取る |
| `lex_split.cpp` | 同じ字句解析を parse_one 直接 / File::parse / Stream 経由 の 3 通りで回す。どの層の上乗せかを分ける |
| `token_cost.cpp` | トークン本文を作る代金。本文ありとなしで、字句解析と容器への積み込みを比べる |
| `fn_fallible.cpp` | `fn` ごとに assert / error があるか、他の `fn` を呼ぶか。失敗しうる性を伝播させると何件増えるかを固定点まで回す |

## 測るときの約束

**このマシンはサーマルスロットリングする** (2.61 GHz ↔ 1.99 GHz、約 24% 差)。
特にビルドを繰り返した直後に落ちる。

- **A/B は交互に走らせる。** 「before を N 回 → after を N 回」は、その間の
  周波数変化を測ることになる。実際に 2026-08-27 に、逐次比較で「-55.6 ms」と
  出たものが交互では差ゼロだった
- 各条件で最小値と平均の両方を見る
- **10 ms / 数 % 程度の差はこの方法でも分解できない**と思っておく

```sh
for i in $(seq 1 8); do ./a.exe $FILES; ./b.exe $FILES; done
```

## 回数は目的ではない

比較回数やノード数が減っても、時間で裏が取れなければ「回数は減ったが時間は
測れなかった」と書く。同日に、演算子探索のタグ足切りが比較を 19% 減らしながら
時間を全く動かさなかった例がある (連鎖のほうが代金だった)。

到達点の数字と、どこに時間が行っているかは
`src/core/nast/docs/front_end_throughput.md` にある。

## 手法

ここに置いていない使い捨てのプログラムで測ったものがある。作り方だけ残す。

### 呼び出し回数を数える — ソースの複製に計数を入れる

`stream.cpp` を `ignore/` へ複製し、グローバルなカウンタを足して、
本体の代わりにその複製をリンクする。木を汚さずに回数が取れる。

```sh
cp src/core/nast/parse/stream.cpp ignore/nast/stream_counted.cpp
# 複製に std::size_t g_xxx = 0; を足し、数えたい関数で g_xxx++
clang++ -std=c++23 -O2 -I src/core/nast -I src \
    <driver>.cpp src/core/nast/parse/parse.cpp \
    ignore/nast/stream_counted.cpp ... -o out.exe
```

これで出た数字の例: `expect_token(string_view)` が 1252144 回
(1 トークンあたり 8.7 回)、うち一致 2.9%、`maybe_parse()` が 2880403 回。
**複製は本体が変わると腐るので、計測が済んだら捨てる。**

### どの行が熱いか — clang の計装

サンプリングプロファイラ (`wpr` / `xperf`) は**管理者権限が要って使えない**。
代わりに行ごとの実行回数を取る。

```sh
clang++ -std=c++23 -O1 -fprofile-instr-generate -fcoverage-mapping ... -o prof.exe
LLVM_PROFILE_FILE=p.profraw ./prof.exe <inputs>
llvm-profdata merge -sparse p.profraw -o p.profdata
llvm-cov show ./prof.exe -instr-profile=p.profdata <source.cpp>
```

これで `parse.cpp` の `consume_op` が突出していることが分かった。
時間ではなく回数なので、そのままでは「重い」の証拠にならない。
回数で当たりを付けて、時間は別に測る。

### 別 TU 呼び出しが効いているか — 1 TU にまとめる

`.cpp` を 1 つのファイルから `#include` して、インライン化される場合と
比べる。`stream.cpp` + `parse.cpp` で 463.3 -> 429.7 ms (7%) だった。

```cpp
// ignore/nast/onetu.cpp
#include "../../src/core/nast/parse/stream.cpp"
#include "../../src/core/nast/parse/parse.cpp"
```

### 変えても同じものが出るか — 出力どうしを比べる

字句解析器やパーサに手を入れるときは、**旧版を別の名前空間に複製して**
同じ入力に両方を通し、トークン列やノード数を突き合わせる。

```sh
git show HEAD:src/core/lexer/lexer.h \
  | sed -e 's/namespace internal {/namespace internal_old {/' \
        -e 's/parse_one(/parse_one_old(/' > ignore/nast/lexer_old.h
```

これで punct の判定を変えたとき 143251 トークンが全一致することを確かめた。

### 型の分類を確かめる — 推論しない

`is_convertible_v` のようなものは、読んで判断せず print する 10 行の
プログラムを書く。`file::View` は rvec に変換できるが `U8View<...>` は
できない、といった分類はこれで確定させた。

### 分母は到達可能なものに限る

アリーナは解放しないので、パーサが先読みして捨てたノードも残る
(式の 38%)。カバレッジを測るときは Module から `visit_all` で到達した
ものだけを数える。**`unique_id` はアリーナ内でしか一意でないので、
集合はファイルごとに作り直す。** 所有辺が 2 本入るノードもあるので
id で重複も落とす。
