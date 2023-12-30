---
title: "Config"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# brgen（バイナリエンコーダ/デコーダジェネレータドライバー）設定ファイルスキーマ

###### documented by chatgpt

## 概要

この文書は、バイナリエンコーダおよびデコーダ機能を生成するためのツールである brgen の設定ファイルのスキーマについて説明しています。設定ファイルには、brgen ツールの動作に影響を与える設定が含まれています。

## スキーマ

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "description": "brgen（バイナリエンコーダ/デコーダジェネレータドライバー）設定ファイルスキーマ",
  "properties": {
    "src2json": {
      "type": "string",
      "description": "src2json実行可能ファイルへのパス（定義ファイルをJSON-AST形式に変換）（Windowsの場合、.exe拡張子を指定する必要はありません）（デフォルト：brgen実行可能ファイルを含むディレクトリにあるsrc2json）"
    },
    "suffix": {
      "type": "string",
      "description": "入力ファイルのサフィックス（デフォルト：.bgn）"
    },
    "input_dir": {
      "type": "array",
      "items": {
        "type": "string"
      },
      "description": "対象の入力ディレクトリの配列"
    },
    "warnings": {
      "type": "object",
      "properties": {
        "disable_untyped": {
          "type": "boolean",
          "description": "未指定の型の警告を無効にする"
        },
        "disable_unused": {
          "type": "boolean",
          "description": "未使用の警告を無効にする"
        }
      },
      "description": "警告の構成"
    },
    "output": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "generator": {
            "type": "string",
            "description": "ジェネレータ実行可能ファイルへのパス（Windowsの場合、.exe拡張子を指定する必要はありません）"
          },
          "output_dir": {
            "type": "string",
            "description": "出力ディレクトリ"
          },
          "args": {
            "type": "array",
            "items": {
              "type": "string"
            },
            "description": "引数の配列"
          }
        },
        "required": ["generator", "output_dir"],
        "additionalProperties": false
      },
      "description": "出力構成の配列"
    }
  },
  "required": ["input_dir", "output"],
  "additionalProperties": false
}
```

## 例

```json
{
  "src2json": "./tool/src2json",
  "input_dir": ["./example/", "./example/gen_step/"],
  "suffix": ".bgn",
  "warnings": {
    "disable_untyped": true,
    "disable_unused": true
  },
  "output": [
    {
      "generator": "./tool/json2cpp2",
      "output_dir": "./ignore/example/cpp2/",
      "args": ["--add-line-map"]
    },
    {
      "generator": "./tool/json2go",
      "output_dir": "./ignore/example/go/",
      "args": [
        "-map-word",
        "Id=ID",
        "-map-word",
        "IDentifier=Identifier",
        "-map-word",
        "Tcpsegment=TCPSegment",
        "-map-word",
        "Udpdatagram=UDPDatagram",
        "-map-word",
        "Udpheader=UDPHeader"
      ]
    },
    {
      "generator": "./tool/json2kaitai",
      "output_dir": "./ignore/example/kaitai/"
    }
  ]
}
```

## プロパティの詳細

- **src2json**: src2json 実行可能ファイルのパス（定義ファイルを JSON-AST 形式に変換）。
- **suffix**: 入力ファイルのサフィックス。
- **input_dir**: 対象の入力ディレクトリの配列。
- **warnings**: 警告の構成オブジェクト。
  - **disable_untyped**: 未指定の型の警告を無効にするかどうか。
  - **disable_unused**: 未使用の警告を無効にするかどうか。
- **output**: 出力構成の配列。
  - **generator**: ジェネレータ実行可能ファイルへのパス。
  - **output_dir**: 出力ディレクトリ。
  - **args**: ジェネレータに渡す引数の配列。

## 必須プロパティ

- **input_dir**: 対象の入力ディレクトリを指定。
- **output**: 出力を構成。

# brgen Configuration File Schema

###### documented by chatgpt

## Overview

This document describes the schema for the configuration file of brgen (Binary Encoder/Decoder Generator driver), a tool designed for generating binary encoding and decoding functionalities. The configuration file contains settings that influence the behavior of the brgen tool.

## Schema

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "description": "brgen(BinaRy encoder/decoder GENerator driver) configuration file schema",
  "properties": {
    "src2json": {
      "type": "string",
      "description": "Path to the src2json executable (converts definition file to JSON-AST format) (on Windows, unnecessary to specify .exe extension) (default: src2json in the directory that contains the brgen executable)"
    },
    "suffix": {
      "type": "string",
      "description": "Input file suffix (default: .bgn)"
    },
    "input_dir": {
      "type": "array",
      "items": {
        "type": "string"
      },
      "description": "Array of target input directories"
    },
    "warnings": {
      "type": "object",
      "properties": {
        "disable_untyped": {
          "type": "boolean",
          "description": "Disable untyped warnings"
        },
        "disable_unused": {
          "type": "boolean",
          "description": "Disable unused warnings"
        }
      },
      "description": "Warning configuration"
    },
    "output": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "generator": {
            "type": "string",
            "description": "Generator executable path (on Windows, unnecessary to specify .exe extension)"
          },
          "output_dir": {
            "type": "string",
            "description": "Output directory"
          },
          "args": {
            "type": "array",
            "items": {
              "type": "string"
            },
            "description": "Array of arguments"
          }
        },
        "required": ["generator", "output_dir"],
        "additionalProperties": false
      },
      "description": "Array of output configurations"
    }
  },
  "required": ["input_dir", "warnings", "output"],
  "additionalProperties": false
}
```

## Example

```json
{
  "src2json": "./tool/src2json",
  "input_dir": ["./example/", "./example/gen_step/"],
  "suffix": ".bgn",
  "warnings": {
    "disable_untyped": true,
    "disable_unused": true
  },
  "output": [
    {
      "generator": "./tool/json2cpp2",
      "output_dir": "./ignore/example/cpp2/",
      "args": ["--add-line-map"]
    },
    {
      "generator": "./tool/json2go",
      "output_dir": "./ignore/example/go/",
      "args": [
        "-map-word",
        "Id=ID",
        "-map-word",
        "IDentifier=Identifier",
        "-map-word",
        "Tcpsegment=TCPSegment",
        "-map-word",
        "Udpdatagram=UDPDatagram",
        "-map-word",
        "Udpheader=UDPHeader"
      ]
    },
    {
      "generator": "./tool/json2kaitai",
      "output_dir": "./ignore/example/kaitai/"
    }
  ]
}
```

## Property Details

- **src2json**: Path to the src2json executable (converts definition file to JSON-AST format).
- **suffix**: Input file suffix.
- **input_dir**: Array of target input directories.
- **warnings**: Warning configuration object.
  - **disable_untyped**: Disable untyped warnings.
  - **disable_unused**: Disable unused warnings.
- **output**: Array of output configurations.
  - **generator**: Generator executable path.
  - **output_dir**: Output directory.
  - **args**: Array of arguments passed to the generator.

## Required Properties

- **input_dir**: Specify target input directories.
- **output**: Configure output.
