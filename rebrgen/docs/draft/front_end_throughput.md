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

いずれも 10〜30 ms 規模で、下の「構文解析そのもの」より小さい。

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
