---
name: rebrgen-overview
description: rebrgen プロジェクトの全体像、アーキテクチャ、ファイル構成の概要。プロジェクトを把握したいとき、どこに何があるか確認したいときに参照する。**作業開始前に必ず読むこと。**
---

## プロジェクトの目的

rebrgen は brgen（バイナリフォーマット定義言語）のコードジェネレーター構築フレームワーク。

- **brgen**: `.bgn` ファイル → AST → 各言語コード（直接変換）
- **rebrgen**: `.bgn` ファイル → AST → **EBM (IR)** → 各言語コード（IR 経由）

IR を挟むことで、言語間の共通化・最適化パスを実現する。

## パイプライン

```
.bgn → [src2json] → brgen AST (JSON)
                          ↓
                       [ebmgen] → EBM (.ebm)
                                      ↓
                              [ebm2<lang>] → 各言語コード
```

`ebmcodegen` はコードジェネレーター自体のスケルトンを生成するメタツール。

## EBM (Extended Binary Module) の特徴

- **グラフ構造**: Statement/Expression/Type がそれぞれ集中テーブルに格納され ID で参照
- **構造化制御フロー**: if/loop/match を専用フォーマットで表現
- **高レベル抽象**: `READ_DATA` のような「何をするか」だけを記述。「どうするか」は各言語バックエンドが担う
- **Lowered Statement**: 高レベル IR を基本 IR に変換したもの（VECTORIZED_IO、ARRAY_FOR_EACH など）
- **セマンティクス保存**: エンディアン、ビットサイズなどを保持

## ファイル構成

```
src/
├── ebm/
│   ├── extended_binary_module.bgn    # EBM 構造定義（編集対象）
│   ├── extended_binary_module.hpp    # 自動生成（編集禁止）
│   └── extended_binary_module.cpp    # 自動生成（編集禁止）
│
├── ebmgen/                           # AST → EBM 変換ツール
│   ├── convert/                      # 変換ロジック（statement/expression/type/encode/decode）
│   ├── transform/                    # IR 最適化パス（CFG、ビット操作、IO ベクトル化など）
│   ├── interactive/                  # インタラクティブデバッガー・クエリエンジン
│   └── GEMINI.md                     # 詳細ガイド（長い）
│
├── ebmcodegen/                       # コードジェネレーター生成メタツール
│   ├── default_codegen_visitor/      # デフォルトビジター実装（各 lang が継承可能）
│   ├── stub/                         # 共通スタブ（entry.hpp, output.hpp）
│   ├── body_subset.cpp               # 自動生成（編集禁止）
│   └── class_based.cpp               # クラスベースフックシステム生成ロジック
│
├── ebmcg/                            # ebmコードジェネレーター
│   ├── ebm2c/                        # 言語及び実装hook一覧は`python script/ebmtemplate.py list all` で確認可能
│   ├── ebm2go/
│   ├── ebm2python/
│   ├── ebm2p4/
│   ├── ebm2rust/
|   ...
│
└── ebmip/                            # ebmインタープリター
    ├── ebm2rmw/                      # 言語及び実装hook一覧は`python script/ebmtemplate_ip.py list all` で確認可能
    ...
```

各 `ebm2<lang>/` の構成:

```
ebm2<lang>/
├── main.cpp              # 自動生成（編集禁止）
├── codegen.hpp           # 自動生成（編集禁止）
├── visitor/
│   ├── entry_before_class.hpp   # 言語設定 + フック登録（編集対象）
│   ├── Flags.hpp                # フラグ定義（編集対象）
│   └── *_class.hpp              # 各ノード用フック（量が増えたら分割）
└── config.json           # 言語設定（任意）
```

## 利用できるスキル

| スキル                         | 用途                                            |
| ------------------------------ | ----------------------------------------------- |
| `/rebrgen-build`               | ビルド・再生成手順                              |
| `/rebrgen-test`                | unictest テスト実行実行手順及び機能実装進捗確認 |
| `/rebrgen-new-lang`            | 新言語ジェネレーター追加                        |
| `/rebrgen-debug`               | コンパイルエラー・デバッグ戦略                  |
| `/rebrgen-query`               | EBM クエリエンジンの使い方                      |
| `/rebrgen-inter-lang-refactor` | リファクタリングの方針と手法                    |
| `/rebrgen-new-feature`         | 新機能追加手順及び開発者への対処                |
