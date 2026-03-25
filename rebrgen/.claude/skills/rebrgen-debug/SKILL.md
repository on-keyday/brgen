---
name: rebrgen-debug
description: コンパイルエラー・型エラー・未定義シンボルのデバッグ戦略。エラーが出たとき、挙動が想定外のときに参照する。
---

## 大原則

**エラーの文言から推測して解決しようとするな。必ず定義を読め。**

コンパイルエラーの99%は定義の誤解が原因。マクロ展開のノイズに惑わされるな。

## エラーが出たときの手順

1. **型定義を読む** (`extended_binary_module.hpp`)
2. **変数の型をたどる** — `io_data` の型 → `size` メンバーの型 → `unit` の定義
3. **メンバーが関数かフィールドか確認する**

### 具体例

```
error: called object type 'SizeUnit' is not a function or function pointer
    io_data.size.unit()
                 ~~~~^
```

`unit()` と書いたが `unit` はフィールドであり関数ではない。
`extended_binary_module.hpp` で `SizeUnit unit` と定義されている → `io_data.size.unit` が正しい。

## よくある間違い

- **キャッシュした知識を使う**: ファイルを読まずに「たぶんこうだろう」と書く → 必ず ReadFile で確認
- **マクロ展開を疑う**: エラーにマクロが絡んでいても、原因はほぼ型定義の誤解。マクロ定義を読むのは最後の手段
- **「この結論しかない」と思い込む**: 視野が狭いだけ。定義を読んで現実を確認する

## 実装場所の判断

- **言語固有のロジック** → `src/ebmcg/ebm2<lang>/visitor/`
- **汎用ロジック** → `src/ebmcodegen/default_codegen_visitor/` や共通ヘルパー
- 迷ったら既存の実装と一貫性を保つ

## 出力が変わらないとき

生成が期待通りに変わらない場合は再ビルドを確認:

```bash
python script/build.py native Debug
mkdir -p save
python script/unictest.py --target-runner ebm2<lang> --print-stdout > save/unictest.txt 2>&1
```
