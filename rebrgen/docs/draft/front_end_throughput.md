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

## 測っていないこと

- 構文解析 (A を引いた 358 ms) の内訳。先読みで捨てるノードが式の 38% を
  占めることは分かっているが、`make<T>` 自体は全体の 4% しかない
- Stream の +84 ms の内訳。`std::list<Token>` のノード確保が有力だが未測定

## 済んだもの

識別子 1 文字ごとに punct 全リテラルを照合していた件は `lexer.h` 側で修正済み
(commit `1d6b9568`)。字句解析単体で 294 → 154 ms。上の数値はその後のもの。
