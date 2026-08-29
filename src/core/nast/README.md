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

```sh
python src/core/nast/build.py          # 生成 + ビルド + 単体テスト
python src/core/nast/bench.py --tools  # 計測プログラム
```

`build/` に実行ファイルが出る。`.bgn` を引数に取るものが多い。

```sh
./src/core/nast/build/nast_corpus $(find example -name "*.bgn")        # parse と解析
./src/core/nast/build/nast_wire_test $(find example -name "*.bgn")     # 線上の往復
./src/core/nast/build/nast_unparse_test $(find example -name "*.bgn")  # .bgn の往復
./src/core/nast/build/nast_coverage $(find example -name "*.bgn")      # 型の付き具合
```
