# Contributing to brgen

## Issue Policy

機能の改善、提案、バグ報告などは [このリポジトリの GitHub Issue](https://github.com/on-keyday/brgen/issues/new) で受け付けています。
セキュリティ脆弱性等の報告方針は [SECURITY.md](SECURITY.md) を参照してください。

We welcome feature improvements, requests, and bug reports etc... on [this repository's GitHub Issue](https://github.com/on-keyday/brgen/issues/new).
See [SECURITY.md](SECURITY.md) about the security vulnerability report policy.

## Pull Request Policy

Issue と同様に受け付けています。
ただし、マージする場合は作者(リポジトリオーナー)に意図がわかる説明(目的、変更点など)をつけることと CI にパスすることを求めます。
以下のブランチ命名規則に従っていただけると作者に意図が伝わりやすくなると思います。

We welcome PRs as well as issues. However, if you ask for a PR to be merged, you will need to provide the author (repository owner) with a description of your PR (purpose, changes, etc.) and pass the CI tests.
Following the branch naming conventions below would make it easier for the author to understand the intention:

- `doc/` - ドキュメントへの提案/変更 / Proposals/changes to documentation
- `lang/` - 言語仕様/言語パーサーへの提案/変更 / Proposals/changes to language specifications/language parsers
- `gen/` - brgen(lang)を入力とするコードジェネレーターへの提案/変更 / Proposals/changes to code generators taking brgen(lang) as input
- `ast/` - AST コードジェネレーターや AST ハンドリングツールへの提案/変更 / Proposals/changes to AST code generators or AST handling tools
- `web/` - WebPlayground への提案/変更 / Proposals/changes to WebPlayground
- `lsp/` - LSP サーバーへの提案/変更 / Proposals/changes to LSP servers
- `env/` - 開発環境(shell script, GitHub Actions Workflow ファイル, Dockerfile など)への提案/変更 / Proposals/changes to development environment (shell scripts, GitHub Actions Workflow files, Dockerfile, etc.)
- `tool/` - その他のツール(他言語から brgen 形式にするツールなど)の提案/変更 / Proposals/changes to other tools (tools to convert from other languages to brgen format, etc.)
- `sample/` - フォーマットのサンプルへの提案/変更 / Proposals/changes to format samples
- `other/` - 以上以外の何かしら / Anything else not covered above

分類を増やすべきという場合やその他の事項は GitHub Issue で提案してください。
If there is a need to add more categories or any other issues, please propose them through GitHub Issue.

## Version Policy

0.0.x の間は作者(リポジトリオーナー)都合で更新を行います。
0.1.0 以降については別途定める予定です。

Updates for versions 0.0.x will be made at the discretion of the author (repository owner). For versions 0.1.0 and beyond, separate guidelines will be established.

## License Agreement

コントリビュートする場合は、コードを MIT ライセンスで公開することに同意する必要があります。

If you want to contribute to this product, you must agree with publishing your code under the MIT license.
