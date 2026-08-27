# フロントエンドのスループット

`example/` 314 ファイル / 703 KB を clang -O2 で測ったもの。2026-08-27 時点。

## 今どこに時間が行っているか

nast のコーパスドライバ (`nast_corpus --time`) で段ごとに:

```
read       61.3 ms   ファイルを開いて読む
parse    1117.5 ms   字句解析 + 構文解析
report      0.4 ms   診断を数える
import     86.6 ms   config.import を辿る
bind       91.1 ms   binder + スコープ解決
type       17.4 ms   型付け
total    1374.3 ms
```

解析 3 段の合計は 195 ms で、**81% がフロントエンド**にある。

## 字句解析の中身

同じ字句解析を 3 通りで回すと (`ignore/nast/streambench.cpp` 相当):

```
A  parse_one を直接 (std::string 上の Sequencer)     162.7 ms
B  File::parse をループ (add_file が開いた File)      552.0 ms   +389.3
C  Stream 経由 (list に積む / 行桁 / shrink)          668.7 ms   +116.7
B2 File::parse をループ (std::string で持たせた File) 165.1 ms
```

B2 が A とほぼ同じなので、**+389 ms は関数ポインタ経由でも Stream でもなく、
`add_file` が渡すバッファ型**。当初「Stream の上乗せ」と見ていたが、Stream 自身は
+117 ms しかない。

## 原因

`FileSet::get_input` (`src/core/common/file.h:362`) はファイルを
`futils::file::View` で開き、それを Sequencer のバッファにする。

`futils::file::View::operator[]` (`utils/src/include/file/file_view.h:56`):

```cpp
std::uint8_t operator[](size_t position) const {
    if (position >= size()) { return 0; }
    if (mmap) { return mmap.read_view()[position]; }
    // fallback: seek + read_file で 1 バイト読む
}
```

`read_view()` 自体は割り当てをしない。権限チェックと 2 ワードの `rvec` 構築
だけで、読めば安そうに見える。それでも実測は桁違いに遅い。

同じファイルを 3 通りで 1 バイトずつ読むと (`ignore/nast/idxbench.cpp` 相当、
20 KB を 200 周):

```
file::View::operator[]   26.25 ns/byte
std::string::operator[]   0.13 ns/byte
生ポインタ (v.data())     0.13 ns/byte
```

**mmap は成功している** (`v.data() != nullptr`)。同じマッピングを生ポインタで
読めば `std::string` と同じ速さなので、26 ns は mmap でもページフォルトでもなく
`operator[]` 自身のコード。1 バイトあたり 80 サイクル近い。

なぜそこまで掛かるかは特定していない。inline されていない (fallback 側が
`f.seek` / `f.read_file` という futils ライブラリ内の関数を呼ぶので関数が
大きい) のが有力だが、呼び出し 1 回では説明が付かない差がある。
ただし**どこを直すべきかは、原因の特定を待たずに決まる**: 生ポインタなら
同じ mmap で 200 倍速いので、Sequencer に `View` ではなくポインタか rvec を
渡せばよい。

## 直したもの

`file::View` が開いた時点で `mmap.read_view()` を 1 回受け取り、`operator[]` と
`data()` はそれを引くだけにした (utils_backup `0a02b86f1`)。

原因はアセンブリで確定した。`MMap` は `struct futils_DLL_EXPORT MMap` と
クラスごとエクスポートされているので、`read_view()` のような自明なメンバでも
**インポートテーブル経由で DLL に入る実呼び出し**になる (`__imp_?read_view@MMap@...`)。
しかも今つないでいるのは Debug ビルドの DLL。1 バイト読むたびにこれを払っていた。

`View` 側にはエクスポート指定が無く header-only なので、そちらで閉じられた。

```
View::operator[]   25.37 ns/byte  ->  0.37 ns/byte
生ポインタ          0.13 ns/byte     (参考、変化なし)
```

挙動が 1 つだけ変わる。mmap は在るが読み権限が無い場合、`read_view()` は空の
rvec を返し、元の `operator[]` はそれを添字していた (null 参照)。今は
未マップ時と同じ seek/read 経路へ落ちる。安全側。

## 直した後の数字

```
read       52.3 ms
parse     653.7 ms   (lex + parse)
report      0.4 ms
import     42.0 ms
bind       94.1 ms
type       17.5 ms
total     860.0 ms   (1374.3 ms から)
```

字句解析の 3 通り比較も、`File::parse` が直接呼びと並んだ:

```
A  parse_one 直接        162.5 ms
B  File::parse ループ    174.8 ms   +12
C  Stream 経由           258.6 ms   +84
```

**残るのは Stream の +84 ms。** これが次の対象になる。

## Stream の +84 ms の内訳

トークンを受け取ったあとに何をするかだけを変えて測った (`ignore/nast/tokbench.cpp` 相当、
3 回の最速):

```
1 受け取って捨てる                163.3 ms
2 std::list<Token> に積む         224.4 ms   +61.1
3 std::vector<Token> に積む       265.7 ms   +102.4
4 std::deque<Token> に積む        235.4 ms   +72.2
```

**`std::list` は 3 つの中で最も速い。** vector は再確保のたびに Token
(std::string を含む) を移動するので逆に悪い。容器の選択が問題ではない。

+61 ms は「トークンを取っておくこと自体」の代金で、内訳は:

- 約 28 ms: ノードの確保と解放。ただし **Debug CRT の値**。同じ回数の
  `operator new(72)`/`delete` は Debug CRT で 196 ns、Release CRT で 63 ns
  なので、Release なら約 9 ms に落ちる
- 残り約 33 ms: Token の移動 (std::string 込みで 1 個 56〜72 バイト) と list の帳簿

Stream 全体の +84 ms のうち、上の +61 を引いた **約 23 ms が eos / peek /
consume と行桁の更新**。

### Token::token が std::string であること

これが単独で一番大きい。トークン本文の切り出しだけを外した版と比べると
(`ignore/nast/strbench.cpp` 相当、どちらも 143251 トークン / 703531 バイトで
同じ仕事をしていることを確認済み):

```
1 parse_one (文字列あり) 捨てる      204.4 ms
2 文字列を作らない版 捨てる           95.3 ms
3 文字列あり list に積む             233.3 ms   (+60.9 over 1)
4 文字列なし list に積む             107.7 ms   (+20.1 over 2)
```

- 文字列の作成だけで **85〜110 ms** (実行ごとに振れる)
- 容器に積む代金も 61 ms 対 20 ms で、文字列が倍以上にしている
- `sizeof(Token)` は 88 バイト。本文を外すと 48 バイト

合わせて Stream 経由の字句解析 259 ms のうち **110〜150 ms**。しかも 2 と 4 は
ファイルを `std::string` にコピーする分を計測に含めているので、実際の削減は
これ以上になる。

構造的にも、**本文は既に二重に記録されている**。`Loc.pos` に begin/end があり、
入力は mmap で解析中ずっと生きているので、`token` は `loc.pos` から導ける。

#### 判別器は既に在る

`File::source()` は、Sequencer のバッファ型が `rvec` に変換できるときだけ
中身を返し、そうでなければ空を返す (`direct_source<T>` の `is_convertible_v`)。
これがそのまま「本文をビューにしてよいか」の判定になる。

入力はまず `set_file_with_input_mode` (`file.h:253`) が物理形式に応じて
`binary::EndianView` で包み、次に `set_input_with_mode` が解釈形式に応じて
`utf::U8View` / `U16View` / `U32View` で包む。実際に組み上がる型で
`is_convertible_v<T, rvec>` を見ると:

```
file::View                              true    utf8 入力 / utf8 解釈
EndianView<View, char16_t>              false   utf16 入力 / utf16 解釈
U8View<EndianView<View, char16_t>>      false   utf16le 入力 / utf8 解釈
U16View<file::View>                     false   utf8 入力 / utf16 解釈
U32View<EndianView<View, char16_t>>     false   utf16 入力 / utf32 解釈
EndianView<View, char32_t>              false   utf32 入力 / utf32 解釈
```

- **true は utf8 入力 / utf8 解釈だけ。** `loc.pos` が生のバイト列の添字なので
  `source().substr(pos)` が本文になる。`example/tpm2.bgn` で `source()` が
  20540 バイト (ファイル全長) を返すことも確認した
- **残り 5 通りは false。** `loc.pos` は包んだ後の並びの添字で、その並びは
  メモリ上に存在しない。特に「物理 utf16le / 位置解釈 utf8」は
  `U8View<EndianView<View, char16_t>>` という二重の包みになり、utf8 バイト列は
  読むたびに作られる。ここは本文を持つしかない

つまり変換が挟まる経路は判定で分離でき、バッファ型から静的に決まる。分岐を
新しく足す必要はない。

#### 位置は解釈モードの単位で入る (LSP のための仕様)

`loc.pos` は Sequencer のバッファ内の添字で、そのバッファは包んだ後の並び。
つまり**物理ファイルのバイト位置ではなく、解釈モードの単位**になる。
これは副作用ではなく、**LSP のために入れた機能**。クライアントが期待する単位で
オフセットを返せるようにするためのもの。物理オフセットに揃える方向で
「単純化」すると壊れる。

実際の LSP は `lsp/server/src/server.ts:163-164` で
`--stdin --interpret-mode utf16` を付けて src2json を起動し、編集のたびに
lexer 用と parser 用で 2 回叩く (`:202`, `:210`)。物理側は stdin の utf8 なので
`U16View<...>` 経路で、`source()` は空、`Token::token` は毎トークン
`utf::convert` を通っている。

文書はリクエストごとに読み直されて捨てられるので、**実体化はリクエストを
またいで償却されない**。得られるのは 1 回の解析の中の差だけ。

エラー表示も同じバッファを見る (`dump_source` が `seq.rptr = pos.begin` して
`write_src_loc` を呼び、読み終わったら戻す) ので、意味は一貫している。

したがって「包んだ後の並びを実体化する」変更は位置に影響しない。同じ並びが
計算で作られるか実メモリに在るかが変わるだけで、`pos` の値も意味も変わらない。

#### 変更の規模

`.token` を読んでいるのは lexer 自身を除いて **26 か所** で、全部パーサ:

```
src/core/ast/parse.cpp   13
src/core/nast/parse.cpp  13
```

そのほかに `Stream::take()` を外へ渡すのが `src2json.cpp:251` の 1 か所
(`--lexer` モードの出力)。ここはトークンを字句解析器の寿命の外へ出すので、
ビューのままにはできない。所有する形へ移すか、出力時に `source()` から
切り出すかを決める必要がある。

#### 形の候補

- **本文フィールドを消す** — `Loc.pos` と `FileSet` から引く関数にする。
  二重記録が無くなるが、パーサ 26 か所が `files` を持ち回る必要が出る
- **持てるものにする** — `token` を「ビュー、または所有した文字列」を表す型に
  する。読む側は `std::string_view` への暗黙変換で済み、26 か所はほぼ無変更

測った結果、**前者を採る**。後者は `std::string` メンバが残るので構造体が
むしろ膨らみ、容器の代金が下がらない:

```
現行 Token           sizeof  88   list に 143251 個積む 51.5 ms
本文なし             sizeof  48                        13.4 ms
ビュー or 所有       sizeof 104                        51.4 ms
```

後者が拾えるのは本文コピーの 85〜110 ms だけで、容器の 38 ms は取り逃す。

本文なしにする場合の帰結が 1 つある。`parse_one` は `TokenBuf` が `std::string`
でないとき `utf::convert` を通すので、`Token::token` は解釈モードに関わらず
常に `std::string` で、パーサは `tok.token == "format"` と書けている。
バッファから切り出す形にすると utf16 解釈では u16 が出てくるので、本文を返す
関数が `std::string` を返し、バイト列でないときだけ変換する形が要る。

ただし**それだけでは今より悪くなる**。`Stream::expect_token(std::string_view s)`
は `cur->token == s` で本文を比較していて (`stream.cpp:84`)、パーサは記号も
キーワードもこれで読む。1 つのトークンが複数の候補と比較されれば、その回数だけ
本文が要求される。素朴に「要求されたら変換する」形にすると、今の
「1 トークン 1 回」より増える。

`.token` の使われ方は 3 つに割れる (nast/parse.cpp):

```
等値比較 (consume_token("(") 等)          101 箇所   本文は要らない
AST ノードへ格納 (std::move(token))         5 箇所   要る
is_int_type / is_float_type                 2 箇所   要る (ビューで足りる)
長さだけ (base.token.size(), インデント幅)  4 箇所   要らない (loc.pos.len())
```

大多数は等値比較で、比較対象は全部 ASCII の記号・キーワード。バッファ上の範囲と
要素ごとに突き合わせれば、バイト列でも u16 でも変換せずに判定できる。

**つまり `expect_token(string_view)` をバッファとの直接比較に変えるのが前提。**
そのうえでなら、変換が要るのは本文を AST に入れるトークンだけになる:

```
ident 31812 + int_literal 5648 + str 574 + char 185 + regex 4
  = 38223 / 143251 = 26.7%
```

長さだけの 4 箇所も `loc.pos.len()` に置き換わる。

#### 実体化しても source() が非空になるとは限らない

`rvec` はバイト列のビューなので、`std::u16string` / `std::u32string` は
変換できない (測定済み)。実体化先の要素型で決まる:

```
file::View                          (utf8/utf8)    byte      非空 (mmap のまま)
U8View<EndianView<View,char16_t>>   (utf16le/utf8) byte      実体化で非空になる
EndianView<View,char16_t>           (utf16/utf16)  char16_t  空のまま
U16View<file::View>                 (utf8/utf16)   char16_t  空のまま
U32View<EndianView<...>>            (utf16/utf32)  char32_t  空のまま
EndianView<View,char32_t>           (utf32/utf32)  char32_t  空のまま
```

**`source()` が非空になるのは解釈モードが utf8 のときだけ。**
LSP は `--interpret-mode utf16` なので、そこは本文をバイトのビューにできない。

では実体化の意味は何かというと、**後戻りを安くすること**。
`utf::View::operator[]` はカーソル走査で、`pos` が現在位置より前だと 1 要素ずつ
巻き戻す。字句解析器は候補を落とすたびに `seq.rptr` を巻き戻すので
(16 通りを順に試す構造)、その回数だけ walk が走る。実体化すれば O(1) の添字になる。

実測した (`ignore/nast/utfbench.cpp` 相当、utf8 入力 / utf16 解釈、
トークン列が 143251 個一致することを確認):

```
A アダプタのまま (カーソル走査)   354.0 ms
B 実体化してから                 280.6 ms
  実体化の費用                    11.8 ms
  B + 実体化                     292.5 ms   x1.21
```

**1.21 倍。61 ms。** 想定していたほどではなかった。理由も数字に出ている:
703531 バイトの入力に対して utf16 units が 700555 で、**ほぼ全部 ASCII**。
ASCII なら utf8 -> utf16 は 1 対 1 なので、`move_to()` の走査は 1 歩ずつの前進が
大半になり、巻き戻しも短い。非 ASCII が多い入力なら差は開くはずだが、
`example/` では測れない。

案 A 自体は utf16 解釈でも残る。ビュー化はできないが、変換が全 143251 から
AST に格納する 26.7% に減り、`Token` が 88 -> 48 バイトになる。

#### 実体化しても位置は動かない

`utf::View` (`unicode/utf/view.h`) の作りがそのまま根拠になる。

- `size()` は `converted_size`。コンストラクタで `count_converted_size()` が
  入力を 1 周して数えた、変換後の要素数
- `operator[](pos)` は `move_to(pos)` でカーソル (`virtual_ptr`) を 1 要素ずつ
  進退させて `pos` に合わせ、その要素を返す

**「位置 i = 変換後の並びの i 番目」はアダプタ自身の定義**なので、
`i = 0..size()-1` を順に取って実体化すれば同じ並びが同じ添字で得られる。
整合を取る作業は無く、定義がそのまま保存される。

条件が 1 つ: **実体化はアダプタを反復して作ること。** 別に `utf::convert` を
呼んで作ると、不正シーケンスの扱いなどが食い違って添字がずれうる。

副産物が 2 つ:

- コンストラクタが既に入力を 1 周しているので、その周回で文字列も作れば
  追加のパスは要らない
- `operator[]` はカーソル走査で、`pos` が現在位置より前だと 1 要素ずつ巻き戻す。
  `dump_source` はエラー表示のたびに `rptr = pos.begin` へ飛んで戻すので、
  今それを払っている。実体化すれば O(1) になる

つまりこの経路で実体化は、位置に対して中立、速度に対しては改善方向のみ。

#### 比較の置き換えは収支ほぼゼロ (前提であって効果ではない)

実際の解析で `expect_token(std::string_view)` が何回呼ばれるかを数えた
(`stream.cpp` の複製に計数を入れて計測):

```
expect_token(string_view)  1252144 回   (143251 トークン -> 8.7 回/token)
  うち一致                   36747 回   (2.9%)
expect_token(Tag)           751561 回
literal の平均長             2.23 バイト
相手トークンの平均長         3.28 バイト
```

この分布で 3 形式を比べると:

```
A 現行 std::string == string_view      7.55 ms   (6.03 ns/回)
B 提案 バイト列の要素ループ            6.42 ms   (5.13 ns/回)
C 提案 u16 列の要素ループ              6.33 ms   (5.05 ns/回)
```

**-1.1 ms。u16 でも遅くならない。** 97.1% が不一致で、その大半は長さ比較だけで
決着するので、要素ループに入る回数自体が少ない。

見積もりに入っていないもの:

- u16/u32 経路では要素型を `File` の中でしか知らないので、比較を関数ポインタ
  越しに呼ぶ必要がある。1252144 回 x 数 ns で 3〜6 ms の上乗せ見込み。
  utf8 経路は `source()` を `Stream` に取っておけばインラインにできる
- ベンチのバッファ参照は連続で行儀が良い。実際は解析中の位置を飛び回る

つまり**この段は効果ではなく、次の段を可能にするためのもの**。収支はほぼゼロ。

#### 手順

1. `Stream::expect_token(string_view)` をバッファとの直接比較にする。
   ここを先に変えないと、次で比較のたびに変換が走る。**それ自体の効果は無い
   (収支ほぼゼロ)**
2. `Token::token` を消し、本文は `loc.pos` + バッファから引く関数にする。
   長さだけの箇所は `loc.pos.len()` へ

**アダプタの実体化は前提ではない。** `source()` を非空にできない以上、
これは独立した 61 ms の最適化 (utf16/utf32 解釈のときだけ)。上の 2 段とは
別に、やるかどうかを単独で決めればよい。

### ここから先の選択肢

- **トークンを malloc しない**: 先読みと後戻りのために緩衝は要るが、`shrink()`
  で捨てているので、ノードをアリーナか環状バッファから取れば確保が消える。
  効くのは上の 28 ms (Release なら 9 ms) 分
- **Token を軽くする**: `token` を `std::string` ではなく入力へのビューにする。
  入力は mmap で解析中ずっと生きている。ただし 15 文字を超えるトークンは
  comment 2985 個と str_literal 574 個だけで、残りは SSO に収まっているので、
  消えるのは確保ではなく 32 バイトのコピー。`lexer::Token` は src2json と共有
- **futils を Release で建てる**: コードの修正ではないが、今の数字は Debug CRT
  のぶん割り増しになっている

この 3 つはいずれも 10〜30 ms 規模で、上の Token 本文 (110〜150 ms) より小さい。

## 今いちばん大きいもの

View を直したあとの内訳:

```
total 860.0 ms のうち parse (lex + 構文) 653.7 ms
  そのうち 字句解析 (Stream 経由)        258.6 ms
  差し引き 構文解析そのもの              約 395 ms
```

**構文解析そのものの約 395 ms が単独で最大**。まだ内訳を取っていない。
先読みで捨てる式が 38% ある一方で `make<T>` は全体の 4% しかないので、
確保ではなく再構文解析そのものの可能性が高いが、未確認。

## 測っていないこと

- 構文解析そのもの (約 395 ms) の内訳

## 済んだもの

識別子 1 文字ごとに punct 全リテラルを照合していた件は `lexer.h` 側で修正済み
(commit `1d6b9568`)。字句解析単体で 294 → 154 ms。上の数値はその後のもの。
