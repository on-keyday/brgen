---
name: rebrgen-new-lang
description: 新しい言語ジェネレーター (ebm2<lang>) の追加手順とフック実装の方法。新言語対応を始めるときに参照する。
---

## セットアップ

```bash
# 1. スケルトン生成
python script/ebmcodegen.py <lang_name>
# → src/ebmcg/ebm2<lang_name>/ が作られる

# 2. ビルド
python script/build.py

# 3. テストで未実装フック一覧を確認
mkdir -p save
python script/unictest.py --target-runner ebm2<lang_name> --print-stdout > save/unictest.txt 2>&1
grep "unimplemented" save/unictest.txt
```

## 手動で生成を確認するとき

**必ず `--debug-unimplemented` を付ける。** これがないと未実装フックがサイレントに空文字を返すため、出力を見ても何が欠けているか分からない。

```bash
./tool/ebm2<lang>.exe -i save/some.ebm --debug-unimplemented 2>/dev/null
# → 未実装箇所に {{Unimplemented Statement_FOO N}} がインライン出力される
```

実装のゴールは2段階:

1. `--debug-unimplemented` 付きで `{{Unimplemented ...}}` が一切出なくなり、生成コードが構文的に正しい
2. unictest が通る (`python script/unictest.py --target-runner ebm2<lang>`)

フックを実装するごとにこのフラグ付きで確認し、段階的にゴール1→2を目指す。

## フックの実装方針

### 基本: entry_before_class.hpp にまとめる

`src/ebmcg/ebm2<lang>/visitor/entry_before_class.hpp` の `DEFINE_VISITOR(entry_before)` 内で `config.*_visitor` に設定する。量が少ない間はここに集約する。

```cpp
DEFINE_VISITOR(entry_before) {
    config.variable_define_keyword = "let mut";
    config.endof_statement = ";";
    // ...

    config.read_data_visitor = [](Context_Statement_READ_DATA& ctx) -> expected<Result> {
        using namespace CODEGEN_NAMESPACE;
        if (auto lw = ctx.read_data.lowered_statement()) {
            return ctx.visit(lw->io_statement.id);
        }
        // 言語固有の実装
        return pass;
    };
    return pass;
}
```

### 量が増えたら: 専用ファイルに分割

```bash
python script/ebmtemplate.py Statement_FOO_class <lang>
# → src/ebmcg/ebm2<lang>/visitor/Statement_FOO_class.hpp
```

`DEFINE_VISITOR(Statement_FOO) { ... }` に実装する。

## フック実装のコツ

- `ctx` オブジェクトのメンバーは IDE のオートコンプリートで確認できる
- 型エラーが出たら必ず `extended_binary_module.hpp` で定義を確認する（推測しない）
- フィールドか関数かを混同しやすい（例: `unit` はフィールド、`unit()` は誤り）
- `MAYBE(result, expr)` は `expected` の伝搬マクロ（Rust の `?` 相当）

## ファイル構成

```
src/ebmcg/ebm2<lang>/
├── main.cpp              # 自動生成（編集禁止）
├── codegen.hpp           # 自動生成（編集禁止）
├── visitor/
│   ├── entry_before_class.hpp   # 言語設定 + シンプルなフック
│   ├── Flags.hpp                # フラグ定義
│   └── *_class.hpp              # 専用ファイルに分割したフック
└── config.json           # 言語設定（任意）
```

## default_codegen_visitor の活用

`src/ebmcodegen/default_codegen_visitor/` にデフォルト実装がある。
まずデフォルト動作を確認してから、差分だけ上書きする方針が効率的。
