# brgen - binary representation interpreter generator (仮)

ネットワーク・プロトコルのパケット等の解析/生成のためのコードを吐くジェネレータ

# 目標(Goal)

- lightweight runtime - ランタイムは軽いもしくは無い
- enough to represent formats - 世の中にあるネットワークプロトコルフォーマットを表現するのに十分な表現力
- easy to write - 簡単に書ける
- write once generate any language code - 一回書けば様々な言語で生成

# Playground

https://on-keyday.github.io/brgen

# How to build

## 必要なもの

- cmake (必須)
- clang++ (必須)
- ninja (必須)
- go (必須)
- emscripten/emsdk (wasm/build_all のときのみ)
- npm (wasm/build_all のときのみ)
- webpack (wasm/build_all のときのみ)
- typescript (wasm/build_all のときのみ)
- python (build_all のときのみ)
- vsce (build_all のときのみ)

## ネイティブ

- linux 環境の場合:
  1.  `. build.sh`を実行
- windows 環境の場合:
  1.  build.bat を開き、UTILS_DIR 環境変数を`.\utils`に設定する
  2.  `build.bat`を実行

## wasm(web)

- linux 環境の場合
  1. `emsdk_env.bat`を呼び出す
  1. `. build.sh wasm-em`を実行
- windows 環境の場合
  1.  `emsdk_env`を呼び出す
  1.  build.bat を開き、UTILS_DIR 環境変数を`.\utils`に設定する
  1.  `build.bat wasm-em`を実行

## build_all

- windows 環境のみ
  1. `EMSDK_PATH`を`emsdk_env.bat`のパスに設定する
  2. `build_all.bat`を実行
