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

## 案

### 案 1: mmap の rvec を直接 Sequencer に渡す

`View` は開いた時点で mmap を持っている。`mmap.read_view()` は `rvec` なので、
それを Sequencer のバッファにすれば要素アクセスは生ポインタになる。
`File::direct_source` が既に「rvec に変換できる型ならそのまま返す」形なので、
その系統に乗る。

- 得: コピーなし。B2 と同じ速度になるはず (未測定)
- 課題: `View` (と mmap) が Sequencer より長生きする必要がある。今の
  `set_input` は `Sequencer<U>` だけを `shared_ptr<void>` に持つので、
  View を同じ寿命で抱える口が要る
- 課題: mmap に失敗したときの fallback 経路が別に要る

### 案 2: ファイルを std::string に読み込む

`get_input` で `View` の代わりに全体を読む。

- 得: 実測済み (165.1 ms)。変更は数行
- 損: ファイルサイズ分のメモリ。ただし `example/` 最大でも 100 KB 未満で、
  そもそも AST が入力の数十倍になるので支配的にはならない
- 損: mmap の利点 (遅延読み込み / ページキャッシュ共有) を捨てる

### 案 3: View::operator[] を速くする

`open()` の時点で `read_view()` の結果を 1 回だけ持ち、`operator[]` は
その `rvec` を引くだけにする。fallback (mmap 失敗時) はポインタが null の
ときだけ通る形にする。

- 得: futils を使う全員に効く。生ポインタと同じ 0.13 ns/byte になるはず
- 得: brgen 側は 1 行も変えなくてよい
- 課題: futils 側の変更。brgen の外
- 課題: 上記のとおり 26 ns の内訳をまだ説明できていないので、この形にして
  実際に消えるかは未確認

## 測っていないこと

- 案 1 / 案 3 の実測。どちらも `View` の持ち方を変える必要があり、まだ書いていない
- `View::operator[]` の 26 ns/byte の内訳。inline されていないのか、
  権限チェックが重いのか、別の何かか
- 構文解析 (A を引いた 358 ms) の内訳。先読みで捨てるノードが式の 38% を
  占めることは分かっているが、`make<T>` 自体は全体の 4% しかない
- Stream の +117 ms の内訳。`std::list<Token>` のノード確保が有力だが未測定

## 済んだもの

識別子 1 文字ごとに punct 全リテラルを照合していた件は `lexer.h` 側で修正済み
(commit `1d6b9568`)。字句解析単体で 294 → 154 ms。上の数値はその後のもの。
