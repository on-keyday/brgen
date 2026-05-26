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

### 🚨 新規 visitor フックファイルは必ず `ebmtemplate.py` で生成する 🚨

`Editor`/`Write` ツールで手書きしないこと。手書きするとビルドは通るがフックが**サイレントに呼ばれない**。

```bash
python script/ebmtemplate.py <hook>_class <lang>
# → src/ebmcg/ebm2<lang>/visitor/<hook>_class.hpp を生成
# → main.cpp の timestamp も touch される
```

**理由**: `main.cpp` は `__has_include("visitor/<hook>_class.hpp")` でフックを取り込むが、cmake は `__has_include` の依存を**追跡しない**。新規ファイル追加時に `main.cpp` を touch しないと再コンパイルされず、新規フックは include されず、何のエラーも出ずにスキップされる。`ebmtemplate.py` はこの touch まで自動でやる。

**実用上のスケール感**: hooklist は EBM kind × stage suffix × DSL 有無の cartesian で 4000+ 種類になるが、実際に各 backend が override してるのは 10-30 程度。`default_codegen_visitor` が 99 hook で大半を済ませているので、新言語追加時に書くのも一桁〜十数個になることが多い。

```bash
# 全 template 名カタログ (検索用、量に圧倒されない)
./tool/ebmcodegen --mode hooklist

# 既存言語が override してる hook を見る (← こちらが実用的)
python script/ebmtemplate.py list <lang>     # 例: python, rust, go, c
python script/ebmtemplate.py list all        # ebmcg 全言語
python script/ebmtemplate_ip.py list all     # ebmip (interpreter 系)
python script/ebmtemplate.py list default    # 共通の default 実装 (99 個、これが大半をカバー)
```

新言語の hook を考えるときは、近い既存言語 (例: 静的型なら rust / go、動的型なら python) の `list <lang>` を眺めて差分を埋める発想が早い。

### 緊急で手書きするしかない場合のフォールバック

ebmtemplate.py が動かない・テンプレートに無いフックを増設したい等で手書きが避けられない場合のみ:

```bash
# 1. 該当する visitor/<hook>_class.hpp を Write
# 2. main.cpp を必ず touch する (これを忘れるとサイレント失敗)
touch src/ebmcg/ebm2<lang>/main.cpp
python script/build.py
```

### entry_before_class.hpp でまとめる場合

`src/ebmcg/ebm2<lang>/visitor/entry_before_class.hpp` の `DEFINE_VISITOR(entry_before)` 内で `config.*_visitor` に設定する。これは ebmcodegen.py のスケルトン生成時点で既に存在するファイルなので、追加 touch なしで編集 OK。量が少ない間はここに集約してよい。

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

ファイルが大きくなったら個別フックに切り出す:

```bash
python script/ebmtemplate.py Statement_FOO_class <lang>
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
