---
name: rebrgen-build
description: rebrgen のビルド・再生成手順を提示する。ビルドエラー、EBM構造変更、ebmcodegen の再生成、新言語ジェネレーター追加時に参照する。
---

## 通常のビルド

```bash
python script/build.py
```

初回セットアップ時:
```bash
python script/auto_setup.py
```

## EBM 構造変更後の再生成

`extended_binary_module.bgn` を変更したら必ず実行:

```bash
python script/update_ebm.py
```

このスクリプトが行うこと:
1. `src/ebm/ebm.py` で `.hpp/.cpp` を再生成
2. `script/build.py` でビルド
3. `ebmcodegen --mode subset` → `src/ebmcodegen/body_subset.cpp`
4. `ebmcodegen --mode json-conv-header/source` → `src/ebmgen/json_conv.hpp/.cpp`
5. 変更があれば再ビルド
6. テスト用 hex データ生成

## ebmcodegen 自体が壊れているとき

```bash
export CODEGEN_ONLY=1
python script/build.py       # ebmcodegen だけビルド
export CODEGEN_ONLY=0
python script/ebmcodegen.py all
python script/build.py
```

## 新しい言語ジェネレーターを追加するとき

```bash
python script/ebmcodegen.py <lang_name>   # スケルトン生成
python script/build.py
python script/unictest.py --target-runner ebm2<lang_name>  # 未実装フック確認
```

## 開発サイクルの優先順序

コードを変更したら次の編集に進む前に確認する (数字が小さいほど優先):

1. `python script/unictest.py --target-runner <lang>` — 最優先
2. `python script/build.py native Debug`
3. 新規ファイルを追加した場合のみ: `python script/ebmcodegen.py <lang>`
4. EBM 構造変更時のみ: `python script/update_ebm.py`
