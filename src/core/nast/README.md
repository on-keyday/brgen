# NAST - new AST

`src/core/ast` (src2json が使う本番) を作り直す試作。両者は別物で共存している。

## ディレクトリ

| | |
| --- | --- |
| `node/` | ノードの定義とその上の道具。`nodes.json` が正本で、`nodegen.py` が `nodes.h` を生成する。走査 (`traverse.h`)、名前でのパス取得 (`access.h`)、比較 (`compare.{h,cpp}`)、表示 (`printer.h`)、アリーナ (`pool.h`)、JSON からの復元 (`from_json.h`)、組み込み型の綴り (`builtin.h`) |
| `parse/` | 原文から木へ、木から原文へ。字句 (`stream.{h,cpp}`)、構文 (`parse.{h,cpp}`)、逆変換 (`unparse.{h,cpp}`) |
| `bind/` | 解析の段。import 解決 → 束縛 → スコープ解決 → 型付け → 定数畳み込み → requires 推論 → union layout。結果はノードを書き換えず side table に置く |
| `wire/` | 線上表現。`wiregen.py` が `nodes.json` から `nast_wire.bgn` を出し、brgen 自身 (`src2json` → `json2cpp2`) がそれを符号化器にする |
| `tool/` | 実行するもの。単体テスト (`test.cpp`)、コーパス driver (`corpus.cpp`)、LSP 用ダンパ (`dump.cpp`)、線上往復 (`wire_test.cpp`)、逆変換往復 (`unparse_test.cpp`) |
| `bench/` | 計測プログラム。測るときの約束は `bench/README.md` |
| `gen/` | `nodegen.py` の各段 (C++ と TypeScript を出す) |
| `docs/` | 設計の議論と測定の記録 |

生成物は追跡しない: `node/nodes.h`、`wire/nast_wire_conv.hpp`、`build/`。
`lsp/server/src/nast_nodes.ts` は追跡する (LSP のビルドに要るため)。

## 建てる

CMake + Ninja。brgen 本体の CMake からは独立していて、ここだけを configure
できる (本体は C++20、nast は C++23)。

```sh
cmake -S src/core/nast -B src/core/nast/build/cmake -G Ninja
cmake --build src/core/nast/build/cmake
ctest --test-dir src/core/nast/build/cmake --output-on-failure
```

実行ファイルは `build/cmake/bin/` に出る。生成 (`node/nodes.h` /
`wire/nast_wire.*` / lsp の `nast_nodes.ts`) はビルドの依存として走るので、
`node/nodes.json` を直せば次のビルドで追随する。`wire/nast_wire.hpp` だけは
brgen 本体の `src2json` / `json2cpp2` を要るので、建っていなければ既存の
ものを残す。

ソースを足したら `CMakeLists.txt` にも足す (glob は使っていない)。

計測は `bench.py`。src2json と並べて測るドライバで、ビルドは CMake の
Release ツリー (`build/release`) に委ねる — 最適化の差がそのまま乗るので、
ctest が使う Debug ツリーとは分けてある。

```sh
python src/core/nast/bench.py          # 建てて測る
python src/core/nast/bench.py --tools  # -O2 で建てるだけ
```

ツールは `.bgn` を引数に取るものが多い。

```sh
B=src/core/nast/build/cmake/bin
$B/nast_corpus $(find example -name "*.bgn")        # parse と解析
$B/nast_wire_test $(find example -name "*.bgn")     # 線上の往復
$B/nast_unparse_test $(find example -name "*.bgn")  # .bgn の往復
$B/nast_coverage $(find example -name "*.bgn")      # 型の付き具合
```
