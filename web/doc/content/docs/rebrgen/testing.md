---
title: "Testing"
weight: 4
---

{{< hint info >}}
このドキュメントは AI (Claude) によって生成されたものです。内容に誤りがある場合は [GitHub Issue](https://github.com/on-keyday/brgen/issues) で報告してください。
{{< /hint >}}

# Testing

## unictest.py による自動テスト

`script/unictest.py` はテストの自動化フレームワークです。以下のステップを順番に実行します:

1. `.bgn` ファイルを `ebmgen` で EBM に変換
2. 対象言語のコードジェネレーター (`ebm2<lang>`) を EBM に適用してコードを生成
3. 生成されたコードを言語固有のテストスクリプトで実行し、バイナリレベルでの round-trip 検証を行う

### 基本的な使い方

```bash
# 特定のジェネレーターでテストを実行
python script/unictest.py --target-runner ebm2go

# 詳細出力でデバッグ
python script/unictest.py --target-runner ebm2go --print-stdout

# 特定の入力ファイルのみテスト
python script/unictest.py --target-runner ebm2go --target-input websocket_frame_valid

# 複数のランナーを同時に指定
python script/unictest.py --target-runner ebm2go --target-runner ebm2rust
```

### 未実装フックの確認

テストを実行すると、コードジェネレーターは `--debug-unimplemented` フラグで起動され、未実装のフックを報告します。テスト失敗のログを確認することで次に実装すべきフックがわかります。

### テストデータ

テスト用 `.bgn` ファイルは `src/test/` にあります。`test/inputs.json` で各入力の名前とパスが管理されています (dns_packet, http2_frame, websocket, varint, bgp など)。

### unictest_runner.json

各言語ジェネレーターのディレクトリ (`src/ebmcg/ebm2<lang>/`) に `unictest_runner.json` が配置されており、そのジェネレーター固有のテスト設定を記述します。

## ebmtest.py による EBM スキーマ検証

`script/ebmtest.py` は `ebmgen` が出力する EBM JSON が:

- `ebmcodegen --mode spec-json` で得られるスキーマ定義に適合しているか
- テストケースに定義された期待値と一致しているか (Ref 解決を含む深い等値チェック)

を検証します。

## EBM インタラクティブクエリエンジン

`ebmgen` はデバッグのためのクエリエンジンを内蔵しています。

```bash
# インタラクティブモード
./tool/ebmgen -i input.ebm --interactive

# コマンドラインから直接クエリ実行
./tool/ebmgen -i input.ebm -q "Statement { body.kind == \"IF_STATEMENT\" }"
```

詳細は [EBM API Reference]({{< relref "ebm_api" >}}) を参照してください。

## 開発ワークフロー

1. `python script/unictest.py --target-runner ebm2<lang>` でテストを実行
2. 失敗ログで未実装フックを確認
3. `python script/ebmtemplate.py <Hook> <lang>` でフックファイルを生成
4. フック実装を記述
5. 再びテストを実行して確認
