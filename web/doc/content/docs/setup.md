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

- windows(x64),mac,linux(x64,arm),android(arm,termux)のビルド済みバイナリを配布しています
- 現在(2024/1/23)の最新バージョンは[v0.0.5](https://github.com/on-keyday/brgen/releases/tag/v0.0.5)です

TODO(on-keyday): apt-get,brew,winget 等への対応

## ソースコードからビルドする場合

1. [clang](https://releases.llvm.org/download.html),[ninja](https://github.com/ninja-build/ninja/releases),[cmake](https://cmake.org/download/),[go](https://go.dev/dl/)をダウンロードして、PATH 環境変数に設定してください。
2. `git clone https://github.com/on-keyday/brgen.git`を実行
3. `cd brgen`を実行
4. `. build.sh`(mac,linux,git bash(windows))または`build.bat`(cmd.exe(windows))を実行
5. `tool`ディレクトリに実行可能ファイルが入っていることを確認
6. `tool/src2json --version`でバージョンが確認できます(TODO(on-keyday):現在、プログラムのバージョンはリリースのバージョンと同期していません)
7. `tool/brgen`を実行すると`ignore/example`ディレクトリにコードが生成されます

### internals

src2json,json2cpp2 は C++で書かれています。
brgen 及び json2go は Go 言語で書かれています。

C++の依存ライブラリとして https://github.com/on-keyday/utils.git を使用しています(ビルド時に自動でクローンされます)

またテスト用として [gtest](https://github.com/google/googletest)を使用しています。(ビルド時に同上)

`build.sh`(及び`build.bat`)では内部で cmake と ninja を呼び出しています。

デフォルトのコンパイラは clang, clang++, gc(go compiler)です。

`build.sh`の呼び出し前に`FUTILS_{C,CXX}_COMPILER`環境変数に C/C++コンパイラのパスを設定するとビルドの際にそのコンパイラを C/C++コンパイラとして使用するようになります。

`build.sh`呼び出し前に`GO_COMPILER`環境変数に Go コンパイラのパスを設定するとビルドの際にそのコンパイラを Go コンパイラとして使用するようになります

### experimental

`S2J_USE_NETWORK`環境変数を`1`に設定してビルドすることで src2json を http 経由で利用できるようになるモードが使用できるようになります。
このモードを利用する際は初回ビルド時にクローンされる`utils`ディレクトリ内で`. build shared Release fnet`と`. build shared Release fnetserv`を実行して、`libfnet`と`libfnetserv`をビルドする必要があります。
現在、windows 環境では動く可能性が高いですが、他環境での動作は保証されません。

`S2J_LIB`環境変数を`1`に設定してビルドすることで src2json を共有ライブラリの形でビルドできます。
公開する関数は`libs2j_call`関数で引数に argc,argv,capability(利用機能の制限),io_callback,io_callback_data を取ります。詳しくはソースコードを御覧ください。
argc,argv は C 言語の main 関数に渡されるものと同じ要件に従ったものを渡してください(つまり argv[argc]==nullptr であることを要求します)
現在、windows 環境では動く可能性が高いですが、他環境での動作は保証されません。
