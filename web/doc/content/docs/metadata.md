---
title: "Metadata"
weight: 1
# bookFlatSection: false
# bookToc: true
# bookHidden: false
# bookCollapseSection: false
# bookComments: false
# bookSearchExclude: false
---

# メタデータ

ジェネレーター共通のメタデータについて述べる。

## 共通

### 元の URL のメタデータ

```
config.url = "https://example.com"
```

### RFC を指定する。

```
config.rfc = "https://example.com/url/to/rfc"
```

`config.url`を推奨する

### 文字列マッピングを指定する

生成先コードで特別扱いしたい文字列のマッピング情報を追加する。
識別子の名前で使われる。

```
config.word.map("Id","ID")
```

## 言語固有

### C++

#### 名前空間名を指定

```
config.cpp.namespace = "namespace name"
```

名前空間名の整合性については保証しない

#### バイト列の型を指定

```
config.cpp.bytes_type = "byte type name"
```

バイト列(`[]u8`)の型を指定。デフォルトは`::futils::view::rvec`

#### 動的配列の型を指定

```
config.cpp.vector_type = "vector type"
```

動的配列(`[]T`(T は u8 以外の任意の型))の型を指定。デフォルトは`std::vector`

### Go

#### パッケージ名を指定

```
config.go.package = "package name"
```
