# brgen - BinaRy encoder/decoder GENerator

[English README is here](README.md)

**brgen** はバイナリフォーマット定義言語とコードジェネレーター群です。ネットワークパケット・ファイルフォーマットなどのバイナリ構造を `.bgn` 言語で一度記述すれば、複数のターゲット言語向けのエンコーダー/デコーダーコードを生成できます。

読み方: ビーアールジェン(英語圏向け)あるいはビーアールゲン(作者の読み方/ローマ字風/~~作者が勝手に言っているだけ~~)

## クイックサンプル

```
format UDPHeader:
    src_port :u16
    dst_port :u16
    length   :u16
    checksum :u16

format UDPDatagram:
    header :UDPHeader
    data :[header.length-8]u8
```

この定義から、C++、Go、Rust、TypeScript などのエンコード/デコード関数が生成されます。

インストール不要の [Web Playground](https://on-keyday.github.io/brgen/) ですぐに試せます。F1 キー(もしくは右クリック → Command Palette)で表示されるコマンドパレットに `load example file` と入力するとサンプルファイルをロードできます。

実際のフォーマット定義(UDP、TCP、DNS、TLS、ZIP、ELF など)100 個以上が [`example/`](https://github.com/on-keyday/brgen/tree/main/example) にあります。

## 目標

- **enough to represent formats** — 世の中にあるネットワークプロトコルフォーマットを表現するのに十分な表現力
- **easy to write and read** — 簡単に書ける/読める
- **write once generate any language code** — 一回書けば様々な言語で生成

最終的な目標(大それた野望とも言う)は、バイナリフォーマットの仕様書に brgen の定義言語を添えることが常識となることである。機械可読で曖昧さのない定義があれば、仕様書を手に入れたら即コードジェネレーターに生成させることができ、実際に使用することも簡単になるであろう。生成コードが使えない場面でも、意味的に曖昧さのない定義書は手書き実装や初学者の理解に大いに役立つであろう。

## ステータス

本プロダクトは開発途上のものです。使用したことによる効果は保証しかねます。また破壊的変更が予告なしに行われることがあります。

開発は「まず最小限でいいから動くものを作り、徐々に成長させる」方針です。リポジトリには書きかけだが一応動くコードが多数あり、各ジェネレーターもターゲット言語の全機能をカバーすることは要求していません。ジェネレーターごとの適合状況は [unictest (e2e テスト)](https://github.com/on-keyday/brgen/tree/main/src/tool/unictest) で継続的に検証し、結果を公開しています:

**https://on-keyday.github.io/brgen/unictest-results/**

テストケースの拡充、テストフレームワークの拡充、バックエンド開発への貢献をぜひ求めています。

## アーキテクチャ

```
.bgn ファイル → [src2json] → AST (JSON) ─┬→ [json2<lang>] → ターゲットコード      (第1世代)
                                        └→ [ebmgen] → EBM IR → [ebm2<lang>]
                                                          → ターゲットコード      (第2世代)
```

brgen には2世代のコードジェネレーターがあります:

- **第1世代 — AST-to-Code** (`src/tool/json2*`): AST を直接変換するジェネレーター群。C++ (`json2cpp2`)、C、Go、TypeScript、Rust、Kaitai Struct、Mermaid、Graphviz などに対応。Web Playground と AST ライブラリ生成を支えています。各ジェネレーターはターゲットのエコシステム言語(C++/Go/Rust)で書かれており、AST が様々な言語から扱えることの実証でもあります。
- **第2世代 — AST-to-IR-to-Code** ([`rebrgen/`](rebrgen/)): 現在の開発主軸。AST を Extended Binary Module (EBM) 中間表現に変換し、共有 visitor フレームワーク上で `ebm2<lang>` ジェネレーターを構築します。対応ターゲット: C、C++、C#、Go、Java、LLVM IR、P4、Python、Ruby、Rust、TypeScript、Wuffs、Z3、Zig — 加えて非ソースコード系バックエンド(JSON ダンプ、ASCII 可視化、structured fuzzing に使うインタープリター)。上記 unictest で検証されているのはこの世代です。

第2世代の詳細なアーキテクチャは [`rebrgen/README.md`](rebrgen/README.md) を参照してください。

## 類似ツールとの違い

- **Kaitai Struct** — brgen に最も近いツールで、バイナリフォーマット定義から多言語のパーサーを生成します。ただし Kaitai が公式にサポートするのはデコードのみで、brgen はエンコード/デコードの両方を生成します。また `.bgn` では制御フローや演算式を直接書くことができ、brgen は IR (EBM) を独立したバイナリ形式として公開しています。
- **Protocol Buffers / Thrift / Cap'n Proto** — RPC/シリアライゼーション用の IDL。ユーザーが定義するのはメッセージの論理構造で、ワイヤ上のバイト表現はツール側が決めます。バイトレイアウトを直接指定できないため、TCP ヘッダや TLS レコードといった既存のバイナリプロトコルの記述には使えません。brgen はこのバイトレイアウトそのものを記述する言語です。
- **Zeek Spicy** — アプローチが最も近いツール(明示的 IR パイプライン: Spicy → HILTI → C++)。ただし出力は C++ のみでランタイムライブラリが必須です。brgen は多言語出力で、生成コードのランタイム依存を最小化しています。
- **P4** — プログラマブルスイッチ/NIC 向けのパケット処理言語。パケットヘッダと parser/deparser を定義してターゲット上でそのまま実行します。バイナリヘッダを DSL で記述するという問題空間は brgen と共通ですが、多言語コード生成ツールではなく実行用言語です。

これらのツールはそれぞれの領域で成功しており、brgen が上位互換を主張するものではありません。長期的な方向性は競合ではなく共存で、フォーマット定義の入口を `.bgn` で統一しつつ、各ツールのエコシステムをバックエンドとして活かすことを構想しています。`ebm2p4`(P4 出力)が既にその一例です。詳細な分析は [`rebrgen/docs/decisions/0021-positioning-among-idl-tools.md`](rebrgen/docs/decisions/0021-positioning-among-idl-tools.md) を参照してください。

## はじめかた

- **インストール不要**: [Web Playground](https://on-keyday.github.io/brgen/) を使う。
- **ビルド済みバイナリ**: [GitHub Releases](https://github.com/on-keyday/brgen/releases) からダウンロード。
- **ソースからビルド**: CMake、Ninja、Clang++ (C++20)、Go、Python 3 が必要(web ビルドには追加で Emscripten/npm)。

```bash
python build.py native   # ネイティブツール → tool/
python build.py web      # Playground 用 WASM ビルド
python build.py all      # native + wasm + npm + generate + lsp
```

`example/` に対してパイプライン全体を実行するには、`brgen.json`(入出力ディレクトリ、ジェネレーター)を設定して `tool/brgen` を実行します。

## ドキュメント

- ドキュメントサイト: https://on-keyday.github.io/brgen/doc
- フォーマット定義例: [`example/`](https://github.com/on-keyday/brgen/tree/main/example)
- 第2世代ジェネレーターフレームワーク: [`rebrgen/`](rebrgen/)

## ジェネレーターを書く

お好みの言語がない場合、自分でジェネレーターを書くことができます:

- **推奨**: rebrgen フレームワークを使う — 新しい `ebm2<lang>` バックエンドの visitor スケルトンが自動生成されるので、言語固有のフックだけ実装すれば済みます。[`rebrgen/README.md`](rebrgen/README.md) を参照。
- パース済み AST を直接扱う **AST ライブラリ**を C++ (`src/core/ast/`)、Go、TypeScript、Rust、Python (`astlib/`) 向けに提供しています。

新しいジェネレーターや AST ライブラリの Pull Request を歓迎します。

## コントリビューション

バグ報告・機能要望・Pull Request は [GitHub Issue](https://github.com/on-keyday/brgen/issues/new) で受け付けています。Issue/PR ポリシーとブランチ命名規則は [CONTRIBUTING.md](CONTRIBUTING.md)、脆弱性報告は [SECURITY.md](SECURITY.md) を参照してください。

## 謝辞

本作品は [SecHack365'23](https://sechack365.nict.go.jp/) の作品として作り始められました。成果発表会時点のコードは `SecHack365-final` タグのコミットになります。

## ライセンス

MIT License。

すべてのソースコードは MIT ライセンスで公開されています。著作権は on-keyday とコントリビューターに帰属します。コントリビュートする場合は、コードを MIT ライセンスで公開することに同意する必要があります。

コードジェネレーターから生成されたコード(本リポジトリに含まれないもの)のライセンスは自由に決めて構いません。

GitHub Releases のビルド済みバイナリについては、依存関係のライセンスを [licensed](https://github.com/github/licensed) と [gocredits](https://github.com/Songmu/gocredits) で収集し、バイナリに同梱しています。ライセンス上の問題を見つけた場合は GitHub Issue で教えてください。[license_note.txt](script/license_note.txt) も参照してください。
