---
title: "Setup"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# セットアップ

## バイナリをダウンロードする場合

- windows(x64),mac,linux(x64)のビルド済みバイナリを配布しています
- 現在(2023/12/30)の最新バージョンは[v0.0.5](https://github.com/on-keyday/brgen/releases/tag/v0.0.5)です

TODO(on-keyday): apt-get,brew,winget 等への対応

## ソースコードからビルドする場合

1. [clang](https://releases.llvm.org/download.html),[ninja](https://github.com/ninja-build/ninja/releases),[cmake](https://cmake.org/download/),[go](https://go.dev/dl/)をダウンロードして、PATH 環境変数に設定してください。
2. `git clone https://github.com/on-keyday/brgen.git`を実行
3. `cd brgen`を実行
4. `. build.sh`(mac,linux,git bash(windows))または`build.bat`(cmd.exe(windows))を実行
5. `tool`ディレクトリに実行可能ファイルが入っていることを確認
6. `tool/brgen -version`でバージョンが確認できます
7. `tool/brgen`を実行すると`ignore/example`ディレクトリにコードが生成されます

### internals

`build.sh`(及び`build.bat`)では内部で cmake と ninja を呼び出しています。

デフォルトのコンパイラは clang, clang++, gc(go compiler)です。

`build.sh`の呼び出し前に`UTILS_{C,CXX}_COMPILER`環境変数に C/C++コンパイラのパスを設定するとビルドの際にそのコンパイラを C/C++コンパイラとして使用するようになります。

`build.sh`呼び出し前に`GO_COMPILER`環境変数に Go コンパイラのパスを設定するとビルドの際にそのコンパイラを Go コンパイラとして使用するようになります
