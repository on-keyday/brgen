---
name: rebrgen-test
description: unictest.py を使った自動テストの実行方法と解釈。テスト失敗、未実装フックの確認、デバッグ出力の読み方に使う。テスト実行直前に必ず読むこと。
---

## 基本的な実行

**警告: `python script/unictest.py` を引数なしで実行するとPCが過負荷になるため厳禁。必ず `--target-runner ebm2<lang>` で言語を指定すること。**

出力は `save/unictest.txt` など`save/`内にリダイレクトしてから grep などで調べること。直接pipe処理だと裏側では毎回buildが走り繰り返すとpcが非常に重くなるため、必ず一旦ファイルに落としてから調べること。たとえ指定言語数が少ないとかテストケース単体だからといった例外は存在しない。必ず`save/`に落としてから調べること。

```bash
mkdir -p save
python script/unictest.py --target-runner ebm2<lang> --print-stdout > save/unictest.txt 2>&1

# 特定の入力に絞る場合
python script/unictest.py --target-runner ebm2<lang> --target-input <input_name> --print-stdout > save/unictest.txt 2>&1

# 調べる
grep "unimplemented\|FAIL\|ERROR" save/unictest.txt
```

## unictest が行うこと

1. `.bgn` → EBM 変換 (`ebmgen`)
2. コードジェネレーター実行 (`ebm2<lang>`)
3. 未実装ビジターフック検出・報告 (`--debug-unimplemented` を自動付与)
4. 生成コードと期待値の比較（生成コードはログ内に出力される）

## 未実装フックへの対応

テスト出力に `unimplemented hook: Statement_FOO` が出た場合:

### 基本: entry_before_class.hpp に書く

`src/ebmcg/ebm2<lang>/visitor/entry_before_class.hpp` 内の `DEFINE_VISITOR(entry_before)` で `config.foo_visitor` に設定する。量が少ない間はここにまとめる。

```cpp
config.foo_visitor = [](Context_Statement_FOO& ctx) -> expected<Result> {
    // 実装
};
```

### 量が増えたら: 専用ファイルに分割

```bash
python script/ebmtemplate.py Statement_FOO_class <lang>
# → src/ebmcg/ebm2<lang>/visitor/Statement_FOO_class.hpp を生成
```

生成されたファイルに `DEFINE_VISITOR(Statement_FOO) { ... }` を実装する。

## main ブランチとの比較

**直接 curl してパイプ処理するとPCが重くなるため必ず save/ に落としてから解析すること:**

```bash
mkdir -p save
curl -s "https://on-keyday.github.io/brgen/unictest-results/test_results.json" -o save/test_results_main.json

python -c "
import json
data = json.load(open('save/test_results_main.json'))
results = data['results']
runners = sorted(set(r['runner'] for r in results))
for runner in runners:
    rs = [r for r in results if r['runner'] == runner]
    passed = sum(1 for r in rs if r.get('success'))
    failed = [r['input_name'] for r in rs if not r.get('success')]
    print(f'{runner}: PASS={passed}, FAIL={len(failed)}, failed={failed}')
"
```

JSON の構造: `data['results']` がテスト結果のリスト。各エントリのフィールドは `runner`, `success` (bool), `input_name`, `source`, `format_name` など。

## 注意

EBM デバッグ出力 (`ebmgen -d`) は情報量が多すぎてAIには有害なことが多い。
失敗調査にはログ内の生成コードを直接確認するほうが有効。
